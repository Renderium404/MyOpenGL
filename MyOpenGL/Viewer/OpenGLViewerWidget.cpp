#include "OpenGLViewerWidget.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDebug>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <QVector4D>
#include <QWheelEvent>

#include <cmath>

#include "MyOpenGL/Camera/Camera.h"
#include "MyOpenGL/Item/RenderItem.h"
#include "MyOpenGL/Item/RenderPart.h"
#include "MyOpenGL/Material/Material.h"
#include "MyOpenGL/Resource/Geometry.h"
#include "MyOpenGL/Viewer/Modeling/PrimitiveMeshBuilder.h"

OpenGLViewerWidget::OpenGLViewerWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , m_systemVertexColorMaterial(0)
    , m_navigationAnchorGeometry("NavigationAnchor", BufferUsage::Static, RenderType::Triangles)
    , m_navigationAnchorVisible(false)
    , m_navigationAnchorPixelSize(28)
    , m_hasNavigationAnchor(false)
    , m_glReady(false)
    , m_releasePerformed(false)
{
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    setFormat(format);

    setFocusPolicy(Qt::StrongFocus);

    // Viewer 内统一使用十字鼠标样式。
    setCursor(QCursor(Qt::CrossCursor));

    /// 默认 Camera

    Camera* camera = m_cameraManager.createCamera("MainCamera");

    if (camera == 0)
        qWarning() << "OpenGLViewerWidget construction failed: unable to create MainCamera.";

    if (!buildNavigationAnchorGeometry())
        qWarning() << "OpenGLViewerWidget construction failed: unable to build NavigationAnchor Geometry.";

    buildViewerResources();

    m_navigationAnchorHideTimer.setSingleShot(true);

    connect(&m_navigationAnchorHideTimer, &QTimer::timeout, this, [this]()
    {
        if (m_hasNavigationAnchor)
            return;

        m_navigationAnchorVisible = false;
        update();
    });
}

OpenGLViewerWidget::~OpenGLViewerWidget()
{
    releaseViewerGL();
    unregisterViewerResources();
}

/// Viewer 数据

ResourceManager& OpenGLViewerWidget::resourceManager()
{
    return m_resourceManager;
}

const ResourceManager& OpenGLViewerWidget::resourceManager() const
{
    return m_resourceManager;
}

MaterialManager& OpenGLViewerWidget::materialManager()
{
    return m_materialManager;
}

const MaterialManager& OpenGLViewerWidget::materialManager() const
{
    return m_materialManager;
}

LightManager& OpenGLViewerWidget::lightManager()
{
    return m_lightManager;
}

const LightManager& OpenGLViewerWidget::lightManager() const
{
    return m_lightManager;
}

CameraManager& OpenGLViewerWidget::cameraManager()
{
    return m_cameraManager;
}

const CameraManager& OpenGLViewerWidget::cameraManager() const
{
    return m_cameraManager;
}

ItemManager& OpenGLViewerWidget::itemManager()
{
    return m_itemManager;
}

const ItemManager& OpenGLViewerWidget::itemManager() const
{
    return m_itemManager;
}

/// Viewer 系统显示

CoordinateSystem& OpenGLViewerWidget::coordinateSystem()
{
    return m_coordinateSystem;
}

const CoordinateSystem& OpenGLViewerWidget::coordinateSystem() const
{
    return m_coordinateSystem;
}

ViewNavigation& OpenGLViewerWidget::viewNavigation()
{
    return m_viewNavigation;
}

const ViewNavigation& OpenGLViewerWidget::viewNavigation() const
{
    return m_viewNavigation;
}

/// Viewer 状态

bool OpenGLViewerWidget::viewerGLReady() const
{
    return m_glReady;
}

/// OpenGL

void OpenGLViewerWidget::initializeGL()
{
    m_glReady = false;
    m_releasePerformed = false;

    if (!m_openGLContext.initialize())
    {
        qWarning() << "OpenGLViewerWidget initializeGL failed: MyOpenGLContext initialization failed.";
        return;
    }

    // QOpenGLWidget 的 Context 可能在 Widget 生命周期中发生重建。
    // Context 销毁前必须先释放 Resource 和 Renderer 持有的 GPU Object。
    if (context() != 0)
    {
        connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, [this]()
        {
            releaseViewerGL();
        }, Qt::DirectConnection);
    }

    if (!m_renderer.initialize(&m_openGLContext))
    {
        qWarning() << "OpenGLViewerWidget initializeGL failed: Renderer initialization failed.";
        releaseViewerGL();
        return;
    }

    QOpenGLFunctions_3_3_Core* gl = m_openGLContext.gl();

    if (gl == 0)
    {
        qWarning() << "OpenGLViewerWidget initializeGL failed: OpenGL functions are unavailable.";
        releaseViewerGL();
        return;
    }

    if (!m_resourceManager.syncAll(gl))
    {
        qWarning() << "OpenGLViewerWidget initializeGL failed: Resource synchronization failed.";
        releaseViewerGL();
        return;
    }

    m_renderer.setClearColor(QVector4D(0.86f, 0.91f, 0.97f, 1.0f));

    m_glReady = true;

    // Viewer 初次建立 OpenGL 后，如果已经存在 Item，则自动适配一次观察范围。
    fitItemsToView();
}

void OpenGLViewerWidget::resizeGL(int width, int height)
{
    Q_UNUSED(width);
    Q_UNUSED(height);
}

void OpenGLViewerWidget::paintGL()
{
    if (!m_glReady)
        return;

    QOpenGLFunctions_3_3_Core* gl = m_openGLContext.gl();

    if (gl == 0)
        return;

    /// Resource GPU 同步

    if (!m_resourceManager.syncAll(gl))
    {
        qWarning() << "OpenGLViewerWidget paintGL failed: Resource synchronization failed.";
        return;
    }

    /// Frame Context

    RenderContext renderContext;

    if (!buildRenderContext(renderContext))
    {
        qWarning() << "OpenGLViewerWidget paintGL failed: unable to build RenderContext.";
        return;
    }

    /// 场景灯光
    ///
    /// Viewer 只在这里根据 Light::isEnabled() 构造正常场景使用的灯光集合。
    /// Renderer 不检查 Light::isEnabled()，只使用调用者明确传入的 Light。

    std::vector<const Light*> lights;
    m_lightManager.enabledLights(lights);

    if (!m_renderer.beginFrame(renderContext))
        return;

    /// 用户 Item

    if (!drawItems(renderContext, lights))
    {
        qWarning() << "OpenGLViewerWidget paintGL failed: Item drawing failed.";
        m_renderer.endFrame();
        return;
    }

    /// Viewer 系统显示
    ///
    /// 系统 Material 禁用光照，因此这里显式传入空灯光集合。
    /// 系统对象不会依赖当前场景 Light Selection。

    const std::vector<const Light*> noLights;

    /// 世界坐标系
    ///
    /// CoordinateSystem 使用世界原点确定屏幕位置，
    /// 作为固定 Pixel 大小的 Viewer 系统显示对象绘制。

    if (m_coordinateSystem.isVisible() && m_systemVertexColorMaterial != 0)
    {
        RenderState coordinateState;

        if (m_coordinateSystem.buildRenderState(renderContext, coordinateState))
        {
            if (!m_renderer.clearDepth(coordinateState.viewport))
            {
                qWarning() << "OpenGLViewerWidget paintGL failed: CoordinateSystem depth clearing failed.";
            }
            else if (!m_renderer.drawGeometry(&m_coordinateSystem.geometry(), m_systemVertexColorMaterial, coordinateState, noLights))
            {
                qWarning() << "OpenGLViewerWidget paintGL failed: CoordinateSystem drawing failed.";
            }
        }
    }

    /// Camera Navigation 锚点

    if (m_navigationAnchorVisible && m_systemVertexColorMaterial != 0)
    {
        RenderState anchorState;

        if (buildNavigationAnchorRenderState(renderContext, anchorState))
        {
            if (!m_renderer.clearDepth(anchorState.viewport))
            {
                qWarning() << "OpenGLViewerWidget paintGL failed: NavigationAnchor depth clearing failed.";
            }
            else if (!m_renderer.drawGeometry(&m_navigationAnchorGeometry, m_systemVertexColorMaterial, anchorState, noLights))
            {
                qWarning() << "OpenGLViewerWidget paintGL failed: NavigationAnchor drawing failed.";
            }
        }
    }

    /// 右上角视图导航
    ///
    /// ViewNavigation 是独立 Overlay，拥有自己的局部 Viewport。
    /// 为避免被主场景 Depth 遮挡，只清除它自己的 Viewport Depth。

    if (m_viewNavigation.isVisible() && m_systemVertexColorMaterial != 0)
    {
        RenderState navigationState;

        if (m_viewNavigation.buildRenderState(renderContext, navigationState))
        {
            if (!m_renderer.clearDepth(navigationState.viewport))
            {
                qWarning() << "OpenGLViewerWidget paintGL failed: ViewNavigation depth clearing failed.";
            }
            else
            {
                if (!m_renderer.drawGeometry(&m_viewNavigation.faceGeometry(), m_systemVertexColorMaterial, navigationState, noLights))
                    qWarning() << "OpenGLViewerWidget paintGL failed: ViewNavigation face drawing failed.";

                if (!m_renderer.drawGeometry(&m_viewNavigation.axisGeometry(), m_systemVertexColorMaterial, navigationState, noLights))
                    qWarning() << "OpenGLViewerWidget paintGL failed: ViewNavigation axis drawing failed.";
            }
        }
    }

    m_renderer.endFrame();
}

/// OpenGL 生命周期

void OpenGLViewerWidget::releaseViewerGL()
{
    if (m_releasePerformed)
        return;

    m_releasePerformed = true;
    m_glReady = false;

    QOpenGLContext* viewerContext = context();

    if (viewerContext == 0 || !m_openGLContext.isInitialized())
        return;

    makeCurrent();

    if (QOpenGLContext::currentContext() != viewerContext)
    {
        doneCurrent();
        return;
    }

    QOpenGLFunctions_3_3_Core* gl = m_openGLContext.gl();

    if (gl != 0)
    {
        if (!m_resourceManager.releaseGL(gl))
            qWarning() << "OpenGLViewerWidget releaseViewerGL: some Resources failed to release.";

        m_renderer.release();
    }

    doneCurrent();
}

/// Viewer 系统资源

void OpenGLViewerWidget::buildViewerResources()
{
    /// 系统 Material
    ///
    /// 坐标系和导航器统一使用 VertexColor，
    /// 且 Viewer 系统显示不参与场景光照。

    m_systemVertexColorMaterial = m_materialManager.createMaterial("ViewerSystemVertexColor");

    if (m_systemVertexColorMaterial == 0)
    {
        qWarning() << "OpenGLViewerWidget buildViewerResources failed: unable to create system Material.";
    }
    else
    {
        if (!m_systemVertexColorMaterial->setSurfaceMode(SurfaceMode::VertexColor))
            qWarning() << "OpenGLViewerWidget buildViewerResources failed: unable to configure system Material.";

        m_systemVertexColorMaterial->setLightingEnabled(false);
    }

    /// 系统 Geometry
    ///
    /// CoordinateSystem / ViewNavigation 自己拥有 Geometry。
    /// ResourceManager 只借用这些对象并管理它们的 GPU 生命周期。

    if (m_resourceManager.borrow(&m_coordinateSystem.geometry()) == InvalidResourceId)
        qWarning() << "OpenGLViewerWidget buildViewerResources failed: unable to borrow CoordinateSystem Geometry.";

    if (m_resourceManager.borrow(&m_viewNavigation.faceGeometry()) == InvalidResourceId)
        qWarning() << "OpenGLViewerWidget buildViewerResources failed: unable to borrow ViewNavigation Face Geometry.";

    if (m_resourceManager.borrow(&m_viewNavigation.axisGeometry()) == InvalidResourceId)
        qWarning() << "OpenGLViewerWidget buildViewerResources failed: unable to borrow ViewNavigation Axis Geometry.";

    if (m_resourceManager.borrow(&m_navigationAnchorGeometry) == InvalidResourceId)
        qWarning() << "OpenGLViewerWidget buildViewerResources failed: unable to borrow NavigationAnchor Geometry.";

    /// Viewer 系统显示规则

    m_coordinateSystem.setWorldOrigin(QVector3D(0.0f, 0.0f, 0.0f));
    m_coordinateSystem.setPixelLength(90.0f);

    m_viewNavigation.setPixelSize(128);
    m_viewNavigation.setMargin(12);
}

void OpenGLViewerWidget::unregisterViewerResources()
{
    bool contextCurrent = false;
    QOpenGLFunctions_3_3_Core* gl = 0;

    // 正常情况下 releaseViewerGL() 已经释放全部 GPU 状态，
    // 因此 remove() 不需要 OpenGL Functions。
    // 这里仍尝试取得当前 Context，用于异常释放路径的保护。
    if (context() != 0 && m_openGLContext.isInitialized())
    {
        makeCurrent();

        if (QOpenGLContext::currentContext() == context())
        {
            gl = m_openGLContext.gl();
            contextCurrent = true;
        }
    }

    ResourceId id = m_coordinateSystem.geometry().id();

    if (id != InvalidResourceId && !m_resourceManager.remove(id, gl))
        qWarning() << "OpenGLViewerWidget unregisterViewerResources failed: CoordinateSystem Geometry.";

    id = m_viewNavigation.faceGeometry().id();

    if (id != InvalidResourceId && !m_resourceManager.remove(id, gl))
        qWarning() << "OpenGLViewerWidget unregisterViewerResources failed: ViewNavigation Face Geometry.";

    id = m_viewNavigation.axisGeometry().id();

    if (id != InvalidResourceId && !m_resourceManager.remove(id, gl))
        qWarning() << "OpenGLViewerWidget unregisterViewerResources failed: ViewNavigation Axis Geometry.";

    id = m_navigationAnchorGeometry.id();

    if (id != InvalidResourceId && !m_resourceManager.remove(id, gl))
        qWarning() << "OpenGLViewerWidget unregisterViewerResources failed: NavigationAnchor Geometry.";

    if (contextCurrent)
        doneCurrent();
}

/// 渲染编排

bool OpenGLViewerWidget::buildRenderContext(RenderContext& context) const
{
    const Camera* camera = m_cameraManager.activeCamera();

    if (camera == 0 || width() <= 0 || height() <= 0)
        return false;

    const float aspect = static_cast<float>(width()) / static_cast<float>(height());

    context.view = camera->viewMatrix();
    context.projection = camera->projectionMatrix(aspect);
    context.cameraPosition = camera->position();
    context.cameraForward = camera->forward();
    context.cameraUp = camera->up();
    context.viewportWidth = width();
    context.viewportHeight = height();

    return context.isValid();
}

bool OpenGLViewerWidget::buildNavigationAnchorGeometry()
{
    PrimitiveMeshBuilder builder;

    if (!builder.appendSphere(QVector3D(0.0f, 0.0f, 0.0f), 0.38f, QVector3D(1.0f, 0.75f, 0.1f), 16, 8))
        return false;

    std::vector<GeometryVertexAttribute> attributes;

    GeometryVertexAttribute position;
    position.location = GeometryAttribute::Position;
    position.componentCount = 3;
    position.valueOffset = PrimitiveMeshBuilder::PositionOffset;
    attributes.push_back(position);

    GeometryVertexAttribute color;
    color.location = GeometryAttribute::Color;
    color.componentCount = 3;
    color.valueOffset = PrimitiveMeshBuilder::ColorOffset;
    attributes.push_back(color);

    m_navigationAnchorGeometry.setVertexLayout(PrimitiveMeshBuilder::VertexStride, attributes);
    m_navigationAnchorGeometry.setVertexData(builder.vertexData());
    m_navigationAnchorGeometry.setIndexData(builder.indexData());

    return true;
}

bool OpenGLViewerWidget::buildNavigationAnchorRenderState(const RenderContext& context, RenderState& state) const
{
    if (!m_navigationAnchorVisible || !context.isValid())
        return false;

    const QVector4D clip = context.projection * context.view * QVector4D(m_navigationAnchor, 1.0f);

    if (clip.w() <= 1.0e-8f)
        return false;

    const float ndcX = clip.x() / clip.w();
    const float ndcY = clip.y() / clip.w();
    const float ndcZ = clip.z() / clip.w();

    if (ndcX < -1.0f || ndcX > 1.0f ||
        ndcY < -1.0f || ndcY > 1.0f ||
        ndcZ < -1.0f || ndcZ > 1.0f)
    {
        return false;
    }

    const float pixelX = (ndcX * 0.5f + 0.5f) * context.viewportWidth;
    const float pixelY = (ndcY * 0.5f + 0.5f) * context.viewportHeight;
    const int halfSize = m_navigationAnchorPixelSize / 2;

    state = RenderState();

    state.model.setToIdentity();

    state.view.setToIdentity();
    state.view.lookAt(QVector3D(0.0f, 0.0f, 3.0f), QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0.0f, 1.0f, 0.0f));

    state.projection.setToIdentity();
    state.projection.ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 10.0f);

    state.viewport = RenderViewport(
        static_cast<int>(pixelX) - halfSize,
        static_cast<int>(pixelY) - halfSize,
        m_navigationAnchorPixelSize,
        m_navigationAnchorPixelSize);

    state.depthTestEnabled = true;
    state.depthWriteEnabled = true;

    return state.viewport.isValid();
}

bool OpenGLViewerWidget::drawItems(const RenderContext& context, const std::vector<const Light*>& lights)
{
    const int itemCount = static_cast<int>(m_itemManager.count());

    for (int itemIndex = 0; itemIndex < itemCount; ++itemIndex)
    {
        const RenderItem* item = m_itemManager.itemAt(itemIndex);

        if (item == 0)
            continue;

        if (!drawItem(item, context, lights))
        {
            qWarning() << "OpenGLViewerWidget drawItems failed while drawing Item:" << item->name();
            return false;
        }
    }

    return true;
}

bool OpenGLViewerWidget::drawItem(const RenderItem* item, const RenderContext& context, const std::vector<const Light*>& lights)
{
    if (item == 0)
        return false;

    if (!item->isVisible() || item->partCount() == 0)
        return true;

    RenderState state;
    state.model = item->transform().matrix();
    state.view = context.view;
    state.projection = context.projection;
    state.viewport = RenderViewport(0, 0, context.viewportWidth, context.viewportHeight);
    state.depthTestEnabled = item->depthTestEnabled();
    state.depthWriteEnabled = true;

    const Material* material = item->material();

    for (int partIndex = 0; partIndex < item->partCount(); ++partIndex)
    {
        const RenderPart* part = item->partAt(partIndex);

        if (part == 0 || part->geometry() == 0)
            continue;

        const Geometry* geometry = part->geometry();
        bool drawSucceeded = false;

        // Lines / LineStrip 不使用 Triangle 的 Wireframe DisplayMode。
        // 它们始终按照自身 RenderType 和 Material 正常绘制。
        if (geometry->renderType() != RenderType::Triangles)
        {
            if (material == 0)
            {
                qWarning() << "OpenGLViewerWidget drawItem failed: non-triangle Part requires Material:"
                           << "Item=" << item->name()
                           << "PartId=" << static_cast<qulonglong>(part->id());
                return false;
            }

            drawSucceeded = m_renderer.drawGeometry(geometry, material, state, lights);
        }
        else
        {
            switch (item->displayMode())
            {
            case DisplayMode::Shaded:
                if (material == 0)
                {
                    qWarning() << "OpenGLViewerWidget drawItem failed: shaded Part requires Material:"
                               << "Item=" << item->name()
                               << "PartId=" << static_cast<qulonglong>(part->id());
                    return false;
                }

                drawSucceeded = m_renderer.drawGeometry(geometry, material, state, lights);
                break;

            case DisplayMode::Wireframe:
                drawSucceeded = m_renderer.drawWireGeometry(geometry, item->edgeColor(), state, false);
                break;

            case DisplayMode::ShadedWithEdges:
                if (material == 0)
                {
                    qWarning() << "OpenGLViewerWidget drawItem failed: shaded Part requires Material:"
                               << "Item=" << item->name()
                               << "PartId=" << static_cast<qulonglong>(part->id());
                    return false;
                }

                drawSucceeded = m_renderer.drawGeometry(geometry, material, state, lights);

                if (drawSucceeded)
                    drawSucceeded = m_renderer.drawWireGeometry(geometry, item->edgeColor(), state, true);

                break;
            }
        }

        if (!drawSucceeded)
        {
            qWarning() << "OpenGLViewerWidget drawItem failed while drawing RenderPart:"
                       << "Item=" << item->name()
                       << "PartId=" << static_cast<qulonglong>(part->id())
                       << "Geometry=" << geometry->name();
            return false;
        }
    }

    return true;
}

/// Camera

bool OpenGLViewerWidget::fitItemsToView(float margin)
{
    if (width() <= 0 || height() <= 0)
        return false;

    AxisAlignedBoundingBox bounds;

    if (!m_itemManager.worldBounds(bounds, true))
        return false;

    if (!m_cameraManager.fitBounds(bounds, width(), height(), margin))
        return false;

    update();
    return true;
}

void OpenGLViewerWidget::toggleProjection()
{
    Camera* camera = m_cameraManager.activeCamera();

    if (camera == 0)
        return;

    bool changed = false;

    if (camera->projectionType() == ProjectionType::Perspective)
        changed = camera->setParallel(10.0f, camera->nearPlane(), camera->farPlane());
    else
        changed = camera->setPerspective(45.0f, camera->nearPlane(), camera->farPlane());

    if (!changed)
        return;

    // 有 Item 时重新 Fit，保证投影切换后模型仍完整可见。
    // 没有 Item 时只刷新当前 Camera。
    if (!fitItemsToView())
        update();
}

bool OpenGLViewerWidget::navigationAnchor(QVector3D& anchor) const
{
    if (m_cameraManager.hasViewBounds())
    {
        anchor = m_cameraManager.viewBounds().center();
        return true;
    }

    AxisAlignedBoundingBox bounds;

    if (!m_itemManager.worldBounds(bounds, true))
        return false;

    anchor = bounds.center();
    return true;
}

QVector3D OpenGLViewerWidget::screenPointToAnchor(const QPoint& position) const
{
    const Camera* camera = m_cameraManager.activeCamera();

    if (camera != 0 && width() > 0 && height() > 0)
    {
        QVector3D rayOrigin;
        QVector3D rayDirection;

        if (camera->screenPointToRay(position.x(), position.y(), width(), height(), rayOrigin, rayDirection))
        {
            bool found = false;
            float nearestDistance = 0.0f;
            QVector3D nearestPoint;

            const int itemCount = static_cast<int>(m_itemManager.count());

            for (int i = 0; i < itemCount; ++i)
            {
                const RenderItem* item = m_itemManager.itemAt(i);

                if (item == 0 || !item->isVisible())
                    continue;

                RenderItemRayHit hit;

                if (!item->raycast(rayOrigin, rayDirection, hit))
                    continue;

                if (!found || hit.distance < nearestDistance)
                {
                    found = true;
                    nearestDistance = hit.distance;
                    nearestPoint = hit.position;
                }
            }

            if (found)
                return nearestPoint;
        }
    }

    QVector3D anchor;

    if (navigationAnchor(anchor))
        return anchor;

    return m_coordinateSystem.worldOrigin();
}

bool OpenGLViewerWidget::setStandardView(ViewNavigationFace face)
{
    if (width() <= 0 || height() <= 0)
        return false;

    QVector3D forward;
    QVector3D up;

    if (!m_viewNavigation.viewDirection(face, forward, up))
        return false;

    AxisAlignedBoundingBox bounds;

    if (m_cameraManager.hasViewBounds())
    {
        bounds = m_cameraManager.viewBounds();
    }
    else
    {
        // Camera 没有 View Bounds 时，使用世界原点为中心的默认包围盒。
        bounds.set(QVector3D(-1.0f, -1.0f, -1.0f), QVector3D(1.0f, 1.0f, 1.0f));
    }

    if (!m_cameraManager.setViewDirection(bounds.center(), forward, up))
        return false;

    if (!m_cameraManager.fitBounds(bounds, width(), height()))
        return false;

    update();
    return true;
}

/// Mouse

void OpenGLViewerWidget::mousePressEvent(QMouseEvent* event)
{
    m_lastMousePosition = event->pos();

    if (event->button() == Qt::LeftButton)
    {
        RenderContext context;

        if (buildRenderContext(context))
        {
            ViewNavigationFace face;

            if (m_viewNavigation.hitTest(event->pos(), context, face))
            {
                QVector3D forward;
                QVector3D up;
                QVector3D anchor;

                if (m_viewNavigation.viewDirection(face, forward, up) &&
                    navigationAnchor(anchor) &&
                    m_cameraManager.setViewDirection(anchor, forward, up))
                {
                    update();
                }

                event->accept();
                return;
            }
        }

        // 左键没有命中 ViewNavigation，记录当前鼠标位置对应的导航锚点。
        m_navigationAnchor = screenPointToAnchor(event->pos());
        m_hasNavigationAnchor = true;
        m_navigationAnchorHideTimer.stop();
        m_navigationAnchorVisible = true;

        update();

        event->accept();
        return;
    }

    if (event->button() == Qt::MiddleButton)
    {
        // 中键始终记录当前鼠标位置对应的导航锚点。
        m_navigationAnchor = screenPointToAnchor(event->pos());
        m_hasNavigationAnchor = true;
        m_navigationAnchorHideTimer.stop();
        m_navigationAnchorVisible = true;

        update();

        event->accept();
        return;
    }

    QOpenGLWidget::mousePressEvent(event);
}

void OpenGLViewerWidget::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint currentPosition = event->pos();
    const QPoint delta = currentPosition - m_lastMousePosition;

    m_lastMousePosition = currentPosition;

    if (!m_hasNavigationAnchor)
    {
        QOpenGLWidget::mouseMoveEvent(event);
        return;
    }

    if (event->buttons() & Qt::LeftButton)
    {
        // 左键拖动：持续围绕按下时确定的锚点旋转。
        const float degreesPerPixel = 0.3f;

        if (m_cameraManager.orbitAround(m_navigationAnchor, -delta.x() * degreesPerPixel, -delta.y() * degreesPerPixel))
            update();

        event->accept();
        return;
    }

    if (event->buttons() & Qt::MiddleButton)
    {
        // 中键拖动：持续基于按下时确定的锚点进行平移。
        if (m_cameraManager.panAt(m_navigationAnchor, delta.x(), delta.y(), width(), height()))
            update();

        event->accept();
        return;
    }

    QOpenGLWidget::mouseMoveEvent(event);
}

void OpenGLViewerWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton)
    {
        m_hasNavigationAnchor = false;
        m_navigationAnchorVisible = false;
        m_navigationAnchorHideTimer.stop();

        update();

        event->accept();
        return;
    }

    QOpenGLWidget::mouseReleaseEvent(event);
}

void OpenGLViewerWidget::wheelEvent(QWheelEvent* event)
{
    const QVector3D anchor = screenPointToAnchor(event->pos());

    m_navigationAnchor = anchor;
    m_navigationAnchorVisible = true;

    const float wheelSteps = static_cast<float>(event->angleDelta().y()) / 120.0f;
    const float factor = static_cast<float>(std::pow(1.15, wheelSteps));

    if (m_cameraManager.zoomAt(anchor, factor, width(), height()))
        update();

    m_navigationAnchorHideTimer.start(350);

    event->accept();
}

/// Keyboard

void OpenGLViewerWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_F)
    {
        fitItemsToView();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_P)
    {
        toggleProjection();
        event->accept();
        return;
    }

    QOpenGLWidget::keyPressEvent(event);
}

/// Context Menu

void OpenGLViewerWidget::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    QAction* fitAllAction = menu.addAction(tr("Fit All"));
    connect(fitAllAction, &QAction::triggered, this, [this]()
    {
        AxisAlignedBoundingBox bounds;
        if(m_itemManager.worldBounds(bounds))
        {
            m_cameraManager.fitBounds(bounds,width(),height());
        }
        else{
            bounds.set(QVector3D(-1.0,-1.0,-1.0),QVector3D(1.0,1.0,1.0));
            m_cameraManager.focusBounds(bounds);
        }
        
    });
    /// View

    QMenu* viewMenu = menu.addMenu(tr("View"));

    QAction* topAction = viewMenu->addAction(tr("Top"));
    QAction* bottomAction = viewMenu->addAction(tr("Bottom"));

    viewMenu->addSeparator();

    QAction* leftAction = viewMenu->addAction(tr("Left"));
    QAction* rightAction = viewMenu->addAction(tr("Right"));

    viewMenu->addSeparator();

    QAction* frontAction = viewMenu->addAction(tr("Front"));
    QAction* backAction = viewMenu->addAction(tr("Back"));

    /// Visualization

    QMenu* visualizationMenu = menu.addMenu(tr("Display"));

    QAction* coordinateSystemAction = visualizationMenu->addAction(tr("Axes"));
    coordinateSystemAction->setCheckable(true);
    coordinateSystemAction->setChecked(m_coordinateSystem.isVisible());

    QAction* viewNavigationAction = visualizationMenu->addAction(tr("View Navigation"));
    viewNavigationAction->setCheckable(true);
    viewNavigationAction->setChecked(m_viewNavigation.isVisible());

    /// Execute

    QAction* selectedAction = menu.exec(event->globalPos());

    if (selectedAction == topAction)
    {
        setStandardView(ViewNavigationFaceTop);
    }
    else if (selectedAction == bottomAction)
    {
        setStandardView(ViewNavigationFaceBottom);
    }
    else if (selectedAction == leftAction)
    {
        setStandardView(ViewNavigationFaceLeft);
    }
    else if (selectedAction == rightAction)
    {
        setStandardView(ViewNavigationFaceRight);
    }
    else if (selectedAction == frontAction)
    {
        setStandardView(ViewNavigationFaceFront);
    }
    else if (selectedAction == backAction)
    {
        setStandardView(ViewNavigationFaceBack);
    }
    else if (selectedAction == coordinateSystemAction)
    {
        m_coordinateSystem.setVisible(coordinateSystemAction->isChecked());
        update();
    }
    else if (selectedAction == viewNavigationAction)
    {
        m_viewNavigation.setVisible(viewNavigationAction->isChecked());
        update();
    }

    event->accept();
}