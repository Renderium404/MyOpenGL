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
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QPoint>
#include <QPointF>
#include <cmath>
#include <algorithm>
#include "MyOpenGL/Camera/Camera.h"
#include "MyOpenGL/Item/RenderItem.h"
#include "MyOpenGL/Item/RenderPart.h"
#include "MyOpenGL/Material/Material.h"
#include "MyOpenGL/Resource/Geometry.h"
#include "MyOpenGL/Resource/Texture.h"
#include "MyOpenGL/Viewer/Modeling/PrimitiveMeshBuilder.h"
class ViewportOverlayWidget : public QWidget
{
public:
    explicit ViewportOverlayWidget(OpenGLViewerWidget* viewer)
        : QWidget(viewer)
        , m_viewer(viewer)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);//鼠标穿透
        setAttribute(Qt::WA_NoSystemBackground, true);       //不自动清背景
        setAttribute(Qt::WA_TranslucentBackground, true);    //背景允许透明
        setAutoFillBackground(false);                        //不自动用调色板填背景
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        if (m_viewer == 0)
            return;

        QPainter painter(this);

        painter.save();
        m_viewer->drawViewportOverlay(painter);
        painter.restore();
    }

private:
    OpenGLViewerWidget* m_viewer;
};
OpenGLViewerWidget::OpenGLViewerWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , m_viewportOverlay(0)
    , m_measurementTool(0)
    , m_systemVertexColorMaterial(0)
    , m_navigationAnchorGeometry("NavigationAnchor", BufferUsage::Static, RenderType::Lines)
    , m_navigationAnchorVisible(false)
    , m_navigationAnchorPixelSize(28)
    , m_hasNavigationAnchor(false)
    , m_glReady(false)
    , m_releasePerformed(false)
    , m_sceneDepthWidth(0)
    , m_sceneDepthHeight(0)
    , m_sceneDepthValid(false)
{

    //OpenGL资源申请
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
    setMouseTracking(true);
    /// 默认 Camera

    Camera* camera = m_cameraManager.createCamera("MainCamera");
    if (camera == 0)
        qWarning() << "OpenGLViewerWidget construction failed: unable to create MainCamera.";
    //相机锚点资源构建
    if (!buildNavigationAnchorGeometry())
        qWarning() << "OpenGLViewerWidget construction failed: unable to build NavigationAnchor Geometry.";
    //坐标系，导航等系统资源构建
    buildViewerResources();

    m_navigationAnchorHideTimer.setSingleShot(true);

    connect(&m_navigationAnchorHideTimer, &QTimer::timeout, this, [this]()
    {
        if (m_hasNavigationAnchor)
            return;

        m_navigationAnchorVisible = false;
        update();
    });
    //悬浮层配置
    m_viewportOverlay = new ViewportOverlayWidget(this);
    m_viewportOverlay->setGeometry(rect());//将悬浮层塞满父窗口
    m_viewportOverlay->show();
    m_viewportOverlay->raise();
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
ItemManager& OpenGLViewerWidget::measurementItemManager()
{
    return m_measurementItemManager;
}

const ItemManager& OpenGLViewerWidget::measurementItemManager() const
{
    return m_measurementItemManager;
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

    m_renderer.setClearColor(QVector4D(192/255.0, 192/255.0, 192/255.0, 1.0f));

    m_glReady = true;

    // Viewer 初次建立 OpenGL 后，如果已经存在 Item，则自动适配一次观察范围。
    fitItemsToView();
}

void OpenGLViewerWidget::resizeGL(int width, int height)
{
    Q_UNUSED(width);
    Q_UNUSED(height);

    clearSceneDepthCache();
}
void OpenGLViewerWidget::paintGL()
{
    if (!m_glReady)
        return;
    QOpenGLFunctions_3_3_Core* gl = m_openGLContext.gl();

    if (gl == 0)
        return;
    /// Resource GPU 同步。
    if (!m_resourceManager.syncAll(gl))
    {
        qWarning() << "OpenGLViewerWidget paintGL failed: Resource synchronization failed.";
        return;
    }

    /// Frame Context。
    RenderContext renderContext;
    if (!buildRenderContext(renderContext))
    {
        qWarning() << "OpenGLViewerWidget paintGL failed: unable to build RenderContext.";
        return;
    }
    if (!m_renderer.beginFrame(renderContext))
        return;
    /// OpenGL 绘制阶段。
    drawSceneBackground(m_renderer, renderContext);
    drawOpenGLFrame(m_renderer, renderContext);
    /// 缓存主场景 Depth，前景 Viewer 对象不会影响拾取。
    cacheSceneDepth(renderContext);
    drawSceneFront(m_renderer, renderContext);
    m_renderer.endFrame();
    /// 2D Viewport Overlay。
    if (m_viewportOverlay != 0)
        m_viewportOverlay->update();
}
void OpenGLViewerWidget::drawSceneBackground(Renderer& renderer, const RenderContext& context)
{
    Q_UNUSED(renderer);
    Q_UNUSED(context);
}
void OpenGLViewerWidget::drawOpenGLFrame(Renderer& renderer, const RenderContext& context)
{
    Q_UNUSED(renderer);

    /// 场景灯光。
    ///
    /// Viewer 只在这里根据 Light::isEnabled() 构造正常场景使用的灯光集合。
    /// Renderer 不检查 Light::isEnabled()，只使用调用者明确传入的 Light。
    std::vector<const Light*> lights;
    m_lightManager.enabledLights(lights);

    /// 用户 Item。
    if (!drawItems(m_itemManager,context, lights))
        qWarning() << "OpenGLViewerWidget paintGL failed: Item drawing failed.";
}

void OpenGLViewerWidget::drawSceneFront(Renderer& renderer, const RenderContext& context)
{
    /// Viewer 系统显示
    ///
    /// 系统 Material 禁用光照，因此这里显式传入空灯光集合。
    /// 系统对象不会依赖当前场景 Light Selection。

    const std::vector<const Light*> noLights;
    /// 测量辅助对象。

    if (!drawItems(m_measurementItemManager, context, noLights))
        qWarning() << "OpenGLViewerWidget drawSceneFront failed: Measurement Item drawing failed.";
    /// 世界坐标系
    ///
    /// CoordinateSystem 使用世界原点确定屏幕位置，
    /// 作为固定 Pixel 大小的 Viewer 系统显示对象绘制。

    if (m_coordinateSystem.isVisible() && m_systemVertexColorMaterial != 0)
    {
        RenderState coordinateState;

        if (m_coordinateSystem.buildRenderState(context, coordinateState))
        {
            if (!renderer.clearDepth(coordinateState.viewport))
            {
                qWarning() << "OpenGLViewerWidget paintGL failed: CoordinateSystem depth clearing failed.";
            }
            else if (!renderer.drawGeometry(&m_coordinateSystem.geometry(), m_systemVertexColorMaterial, coordinateState, noLights))
            {
                qWarning() << "OpenGLViewerWidget paintGL failed: CoordinateSystem drawing failed.";
            }
        }
    }

    /// Camera Navigation 锚点

    if (m_navigationAnchorVisible && m_systemVertexColorMaterial != 0)
    {
        RenderState anchorState;
        if (buildNavigationAnchorRenderState(context, anchorState))
        {
            if (!renderer.clearDepth(anchorState.viewport))
            {
                qWarning() << "OpenGLViewerWidget paintGL failed: NavigationAnchor depth clearing failed.";
            }
            else if (!renderer.drawGeometry(&m_navigationAnchorGeometry, m_systemVertexColorMaterial, anchorState, noLights))
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

        if (m_viewNavigation.buildRenderState(context, navigationState))
        {
            if (!renderer.clearDepth(navigationState.viewport))
            {
                qWarning() << "OpenGLViewerWidget paintGL failed: ViewNavigation depth clearing failed.";
            }
            else
            {
                if (!renderer.drawGeometry(&m_viewNavigation.faceGeometry(), m_systemVertexColorMaterial, navigationState, noLights))
                    qWarning() << "OpenGLViewerWidget paintGL failed: ViewNavigation face drawing failed.";

                if (!renderer.drawGeometry(&m_viewNavigation.axisGeometry(), m_systemVertexColorMaterial, navigationState, noLights))
                    qWarning() << "OpenGLViewerWidget paintGL failed: ViewNavigation axis drawing failed.";
            }
        }
    }
    
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
    const QVector3D color(1.0f, 0.75f, 0.1f);
    const float length = 0.78f;

    /// 十字锚点。
    ///
    /// Geometry 在独立的 [-1, 1] Overlay 空间中绘制，
    /// 实际屏幕尺寸由 m_navigationAnchorPixelSize 控制。
    ///
    /// Vertex:
    /// Position.xyz + Color.rgb

    const std::vector<GLfloat> vertices =
    {
        -length, 0.0f, 0.0f, color.x(), color.y(), color.z(),
         length, 0.0f, 0.0f, color.x(), color.y(), color.z(),

         0.0f, -length, 0.0f, color.x(), color.y(), color.z(),
         0.0f,  length, 0.0f, color.x(), color.y(), color.z()
    };

    const std::vector<GLuint> indices =
    {
        0, 1,
        2, 3
    };

    std::vector<GeometryVertexAttribute> attributes;

    GeometryVertexAttribute position;
    position.location = GeometryAttribute::Position;
    position.componentCount = 3;
    position.valueOffset = 0;
    attributes.push_back(position);

    GeometryVertexAttribute colorAttribute;
    colorAttribute.location = GeometryAttribute::Color;
    colorAttribute.componentCount = 3;
    colorAttribute.valueOffset = 3;
    attributes.push_back(colorAttribute);

    m_navigationAnchorGeometry.setVertexLayout(6, attributes);
    m_navigationAnchorGeometry.setVertexData(vertices);
    m_navigationAnchorGeometry.setIndexData(indices);

    return true;
}

bool OpenGLViewerWidget::buildNavigationAnchorRenderState(const RenderContext& context, RenderState& state) const
{
    if (!m_navigationAnchorVisible || !context.isValid())
        return false;
    //将世界坐标转换到裁剪空间（x,y,z,w）
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
    //将裁剪坐标转屏幕的归一化坐标
    const float pixelX = (ndcX * 0.5f + 0.5f) * context.viewportWidth;
    const float pixelY = (ndcY * 0.5f + 0.5f) * context.viewportHeight;
    const int halfSize = m_navigationAnchorPixelSize / 2;

    state = RenderState();

    state.model.setToIdentity();

    state.view.setToIdentity();
    state.view.lookAt(QVector3D(0.0f, 0.0f, 3.0f), QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0.0f, 1.0f, 0.0f));

    state.projection.setToIdentity();
    state.projection.ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 10.0f);
    //视口位置跟随屏幕的映射坐标
    state.viewport = RenderViewport(
        static_cast<int>(pixelX) - halfSize,
        static_cast<int>(pixelY) - halfSize,
        m_navigationAnchorPixelSize,
        m_navigationAnchorPixelSize);

    state.depthTestEnabled = true;
    state.depthWriteEnabled = true;

    return state.viewport.isValid();
}

bool OpenGLViewerWidget::drawItems(const ItemManager& itemManager, const RenderContext& context, const std::vector<const Light*>& lights)
{
    const int itemCount = static_cast<int>(itemManager.count());

    /// 第一阶段：
    /// 绘制全部 Item 的 World Space RenderPart。
    ///
    /// 必须先把所有实体 Geometry 画完，
    /// 避免后续 Item Geometry 覆盖已经绘制的 Screen Label。
    for (int itemIndex = 0; itemIndex < itemCount; ++itemIndex)
    {
        const RenderItem* item = itemManager.itemAt(itemIndex);

        if (item == 0)
            continue;

        if (!drawItemParts(item, context, lights))
        {
            qWarning() << "OpenGLViewerWidget drawItems failed while drawing Item Parts:"
                       << item->name();

            return false;
        }
    }

    /// 第二阶段：
    /// 绘制全部 Item 的持久化 Screen Label。
    ///
    /// Label 禁用 Depth Test / Depth Write，
    /// 因此属于当前 ItemManager 的屏幕前景内容。
    for (int itemIndex = 0; itemIndex < itemCount; ++itemIndex)
    {
        const RenderItem* item = itemManager.itemAt(itemIndex);

        if (item == 0)
            continue;

        if (!drawItemLabels(item, context))
        {
            qWarning() << "OpenGLViewerWidget drawItems failed while drawing Item Labels:"
                       << item->name();

            return false;
        }
    }

    return true;
}

bool OpenGLViewerWidget::drawItemParts(const RenderItem* item, const RenderContext& context, const std::vector<const Light*>& lights)
{
    if (item == 0)
        return false;

    if (!item->isVisible() || item->partCount() == 0)
        return true;

    /// 整个 Item 的 RenderPart 共用正常 World RenderState。
    RenderState state;

    state.model = item->transform().matrix();
    state.view = context.view;
    state.projection = context.projection;

    state.viewport = RenderViewport(
        0,
        0,
        context.viewportWidth,
        context.viewportHeight);

    state.depthTestEnabled = item->depthTestEnabled();
    state.depthWriteEnabled = true;
    state.blendEnabled = false;

    const Material* material = item->material();

    for (int partIndex = 0; partIndex < item->partCount(); ++partIndex)
    {
        const RenderPart* part = item->partAt(partIndex);

        if (part == 0 || part->geometry() == 0)
            continue;

        const Geometry* geometry = part->geometry();

        bool drawSucceeded = false;

        /// Lines / LineStrip 不使用 Triangle 的 Wireframe DisplayMode。
        ///
        /// 它们始终按照自身 RenderType 和 Item Material 正常绘制。
        if (geometry->renderType() != RenderType::Triangles)
        {
            if (material == 0)
            {
                qWarning() << "OpenGLViewerWidget drawItemParts failed: non-triangle Part requires Material:"
                           << "Item=" << item->name()
                           << "PartId=" << static_cast<qulonglong>(part->id());

                return false;
            }

            drawSucceeded = m_renderer.drawGeometry(
                geometry,
                material,
                state,
                lights);
        }
        else
        {
            switch (item->displayMode())
            {
            case DisplayMode::Shaded:
            {
                if (material == 0)
                {
                    qWarning() << "OpenGLViewerWidget drawItemParts failed: shaded Part requires Material:"
                               << "Item=" << item->name()
                               << "PartId=" << static_cast<qulonglong>(part->id());

                    return false;
                }

                drawSucceeded = m_renderer.drawGeometry(
                    geometry,
                    material,
                    state,
                    lights);

                break;
            }

            case DisplayMode::Wireframe:
            {
                drawSucceeded = m_renderer.drawWireGeometry(
                    geometry,
                    item->edgeColor(),
                    state,
                    false);

                break;
            }

            case DisplayMode::ShadedWithEdges:
            {
                if (material == 0)
                {
                    qWarning() << "OpenGLViewerWidget drawItemParts failed: shaded Part requires Material:"
                               << "Item=" << item->name()
                               << "PartId=" << static_cast<qulonglong>(part->id());

                    return false;
                }

                drawSucceeded = m_renderer.drawGeometry(
                    geometry,
                    material,
                    state,
                    lights);

                if (drawSucceeded)
                {
                    drawSucceeded = m_renderer.drawWireGeometry(
                        geometry,
                        item->edgeColor(),
                        state,
                        true);
                }

                break;
            }
            }
        }

        if (!drawSucceeded)
        {
            qWarning() << "OpenGLViewerWidget drawItemParts failed while drawing RenderPart:"
                       << "Item=" << item->name()
                       << "PartId=" << static_cast<qulonglong>(part->id())
                       << "Geometry=" << geometry->name();

            return false;
        }
    }

    return true;
}

bool OpenGLViewerWidget::drawItemLabels(const RenderItem* item, const RenderContext& context)
{
    if (item == 0)
        return false;

    if (!item->isVisible() || item->labelCount() == 0)
        return true;

    if (!context.isValid())
        return false;

    const std::vector<const Light*> noLights;

    const QVector3D cameraForward = context.cameraForward.normalized();
    const QVector3D cameraUp = context.cameraUp.normalized();
    const QVector3D cameraRight = QVector3D::crossProduct(cameraForward, cameraUp).normalized();
    const QVector3D sceneUp = QVector3D::crossProduct(cameraRight, cameraForward).normalized();

    if (cameraRight.lengthSquared() <= 1.0e-12f || sceneUp.lengthSquared() <= 1.0e-12f)
        return false;

    for (int labelIndex = 0; labelIndex < item->labelCount(); ++labelIndex)
    {
        const RenderLabel* label = item->labelAt(labelIndex);

        if (label == 0 || !label->isRenderable())
            continue;

        const Geometry* geometry = label->geometry();
        const Material* material = label->material();

        if (geometry == 0 || material == 0)
            continue;

        const QVector2D& sceneAnchor = label->anchorSence();
        const QVector3D worldAnchor = label->anchorWorld() + cameraRight * sceneAnchor.x() + sceneUp * sceneAnchor.y();

        const QVector4D anchorClip = context.projection * context.view * QVector4D(worldAnchor, 1.0f);

        if (anchorClip.w() <= 1.0e-8f)
            continue;

        const float anchorNdcX = anchorClip.x() / anchorClip.w();
        const float anchorNdcY = anchorClip.y() / anchorClip.w();
        const float anchorNdcZ = anchorClip.z() / anchorClip.w();

        if (anchorNdcZ < -1.0f || anchorNdcZ > 1.0f)
            continue;

        float pixelX = (anchorNdcX * 0.5f + 0.5f) * static_cast<float>(context.viewportWidth);
        float pixelY = (anchorNdcY * 0.5f + 0.5f) * static_cast<float>(context.viewportHeight);

        pixelX += static_cast<float>(label->pixelOffset().x());
        pixelY -= static_cast<float>(label->pixelOffset().y());

        RenderState state;
        state.model.setToIdentity();
        state.view.setToIdentity();
        state.projection.setToIdentity();
        state.projection.ortho(0.0f, static_cast<float>(context.viewportWidth), 0.0f, static_cast<float>(context.viewportHeight), -1.0f, 1.0f);
        state.viewport = RenderViewport(0, 0, context.viewportWidth, context.viewportHeight);
        state.depthTestEnabled = false;
        state.depthWriteEnabled = false;

        if (material->surfaceMode() == SurfaceMode::Texture)
        {
            /// Texture Label 的 Geometry 使用 Pixel 单位。
            if (anchorNdcX < -1.0f || anchorNdcX > 1.0f || anchorNdcY < -1.0f || anchorNdcY > 1.0f)
                continue;

            pixelX = static_cast<float>(qRound(pixelX));
            pixelY = static_cast<float>(qRound(pixelY));

            state.model.translate(pixelX, pixelY, 0.0f);
            state.blendEnabled = true;
        }
        else
        {
            /// Geometry Label 的 XY 使用标尺尺度，需要转换成当前屏幕 Pixel 基向量。
            const QVector3D worldXAxis = worldAnchor + cameraRight;
            const QVector3D worldYAxis = worldAnchor + sceneUp;

            const QVector4D xClip = context.projection * context.view * QVector4D(worldXAxis, 1.0f);
            const QVector4D yClip = context.projection * context.view * QVector4D(worldYAxis, 1.0f);

            if (xClip.w() <= 1.0e-8f || yClip.w() <= 1.0e-8f)
                continue;

            const float xPixelX = (xClip.x() / xClip.w() * 0.5f + 0.5f) * static_cast<float>(context.viewportWidth);
            const float xPixelY = (xClip.y() / xClip.w() * 0.5f + 0.5f) * static_cast<float>(context.viewportHeight);
            const float yPixelX = (yClip.x() / yClip.w() * 0.5f + 0.5f) * static_cast<float>(context.viewportWidth);
            const float yPixelY = (yClip.y() / yClip.w() * 0.5f + 0.5f) * static_cast<float>(context.viewportHeight);

            const QVector2D pixelXAxis(xPixelX - pixelX, xPixelY - pixelY);
            const QVector2D pixelYAxis(yPixelX - pixelX, yPixelY - pixelY);

            state.model.setColumn(0, QVector4D(pixelXAxis.x(), pixelXAxis.y(), 0.0f, 0.0f));
            state.model.setColumn(1, QVector4D(pixelYAxis.x(), pixelYAxis.y(), 0.0f, 0.0f));
            state.model.setColumn(2, QVector4D(0.0f, 0.0f, 1.0f, 0.0f));
            state.model.setColumn(3, QVector4D(pixelX, pixelY, 0.0f, 1.0f));
            state.blendEnabled = false;
        }

        if (!m_renderer.drawGeometry(geometry, material, state, noLights))
        {
            qWarning() << "OpenGLViewerWidget drawItemLabels failed:" << "Item=" << item->name() << "LabelId=" << static_cast<qulonglong>(label->id());
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

void OpenGLViewerWidget::setMeasurementTool(MeasurementTool* tool)
{
    if (m_measurementTool == tool)
    {
        if (m_measurementTool != 0)
            m_measurementTool->reset();

        update();
        return;
    }

    if (m_measurementTool != 0)
        m_measurementTool->reset();

    m_measurementTool = tool;

    if (m_measurementTool != 0)
        m_measurementTool->reset();

    update();
}
MeasurementTool* OpenGLViewerWidget::measurementTool()
{
    return m_measurementTool;
}

const MeasurementTool* OpenGLViewerWidget::measurementTool() const
{
    return m_measurementTool;
}
void OpenGLViewerWidget::clearMeasurementItems()
{
    while (m_measurementItemManager.count() > 0)
    {
        const int index = static_cast<int>(m_measurementItemManager.count()) - 1;

        if (!removeMeasurementItemAt(index))
        {
            qWarning() << "OpenGLViewerWidget clearMeasurementItems failed at index:" << index;
            break;
        }
    }

    update();
}

bool OpenGLViewerWidget::removeLastMeasurementItem()
{
    const int count = static_cast<int>(m_measurementItemManager.count());

    if (count <= 0)
        return false;

    if (!removeMeasurementItemAt(count - 1))
        return false;

    update();
    return true;
}

bool OpenGLViewerWidget::removeMeasurementItemAt(int index)
{
    if (index < 0 || index >= static_cast<int>(m_measurementItemManager.count()))
        return false;

    const RenderItem* item = m_measurementItemManager.itemAt(index);

    if (item == 0)
        return false;

    std::vector<ResourceId> geometryIds;
    std::vector<ResourceId> textureIds;
    std::vector<MaterialId> materialIds;

    /// 收集 RenderPart Geometry。
    for (int partIndex = 0; partIndex < item->partCount(); ++partIndex)
    {
        const RenderPart* part = item->partAt(partIndex);

        if (part == 0 || part->geometry() == 0)
            continue;

        const ResourceId geometryId = part->geometry()->id();

        if (geometryId != InvalidResourceId && std::find(geometryIds.begin(), geometryIds.end(), geometryId) == geometryIds.end())
            geometryIds.push_back(geometryId);
    }

    /// 收集 RenderLabel Geometry / Material / Texture。
    for (int labelIndex = 0; labelIndex < item->labelCount(); ++labelIndex)
    {
        const RenderLabel* label = item->labelAt(labelIndex);

        if (label == 0)
            continue;

        if (label->geometry() != 0)
        {
            const ResourceId geometryId = label->geometry()->id();

            if (geometryId != InvalidResourceId && std::find(geometryIds.begin(), geometryIds.end(), geometryId) == geometryIds.end())
                geometryIds.push_back(geometryId);
        }

        const Material* material = label->material();

        if (material == 0)
            continue;

        const MaterialId materialId = material->id();

        if (materialId != InvalidMaterialId && std::find(materialIds.begin(), materialIds.end(), materialId) == materialIds.end())
            materialIds.push_back(materialId);

        const Texture* texture = material->texture();

        if (texture == 0)
            continue;

        const ResourceId textureId = texture->id();

        if (textureId != InvalidResourceId && std::find(textureIds.begin(), textureIds.end(), textureId) == textureIds.end())
            textureIds.push_back(textureId);
    }

    const RenderItemId itemId = item->id();

    /// 先删除 Item。
    /// RenderItem 会销毁自己拥有的 RenderPart 和 RenderLabel，但不会删除其借用的 Geometry / Material / Texture。
    if (!m_measurementItemManager.remove(itemId))
        return false;

    bool contextCurrent = false;
    QOpenGLFunctions_3_3_Core* gl = 0;

    if (context() != 0 && m_openGLContext.isInitialized())
    {
        makeCurrent();

        if (QOpenGLContext::currentContext() == context())
        {
            gl = m_openGLContext.gl();
            contextCurrent = true;
        }
    }

    bool result = true;

    /// 删除 Label Material。
    /// MaterialManager::remove() 不会删除 Material 引用的 Texture，所以 Texture 单独在下面释放。
    for (std::size_t materialIndex = 0; materialIndex < materialIds.size(); ++materialIndex)
    {
        if (!m_materialManager.remove(materialIds[materialIndex]))
        {
            qWarning() << "OpenGLViewerWidget removeMeasurementItemAt failed to remove Label Material:" << static_cast<qulonglong>(materialIds[materialIndex]);
            result = false;
        }
    }

    /// 删除 Part / Label Geometry。
    for (std::size_t geometryIndex = 0; geometryIndex < geometryIds.size(); ++geometryIndex)
    {
        if (!m_resourceManager.remove(geometryIds[geometryIndex], gl))
        {
            qWarning() << "OpenGLViewerWidget removeMeasurementItemAt failed to remove Geometry:" << static_cast<qulonglong>(geometryIds[geometryIndex]);
            result = false;
        }
    }

    /// 删除 Label Texture。
    for (std::size_t textureIndex = 0; textureIndex < textureIds.size(); ++textureIndex)
    {
        if (!m_resourceManager.remove(textureIds[textureIndex], gl))
        {
            qWarning() << "OpenGLViewerWidget removeMeasurementItemAt failed to remove Texture:" << static_cast<qulonglong>(textureIds[textureIndex]);
            result = false;
        }
    }

    if (contextCurrent)
        doneCurrent();

    return result;
}
bool OpenGLViewerWidget::scenePointAtWorld(const QPointF& scene, QVector3D& world) const
{
    if (width() <= 0 || height() <= 0)
        return false;
    if(scene.x() >= 0.0 &&scene.x() < width() &&scene.y() >= 0.0 &&scene.y() < height())
        return scenePointAtWorldFromDepth(scene, world);
    return scenePointAtWorldFromRay(scene, world);
}
bool OpenGLViewerWidget::worldPointAtScene(const QVector3D& world, QPointF& scene) const
{
    return projectWorldPointToScene(world,scene);
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

QVector3D OpenGLViewerWidget::screenPointToZoomAnchor(const QPointF& position) const
{
    const Camera* camera = m_cameraManager.activeCamera();

    if (camera == 0 || width() <= 0 || height() <= 0)
        return m_coordinateSystem.worldOrigin();

    /// 优先使用深度缓存获取屏幕位置对应的真实场景点。

    QVector3D scenePoint;

    if (scenePointAtWorld(position, scenePoint))
        return scenePoint;

    /// 未命中场景时，使用 Screen Ray 与 Near / Far 中间平面的交点。

    QVector3D rayOrigin;
    QVector3D rayDirection;

    if (!camera->screenPointToRay(position.x(), position.y(), width(), height(), rayOrigin, rayDirection))
        return m_coordinateSystem.worldOrigin();

    const QVector3D forward = camera->forward();
    const float middleDepth = (camera->nearPlane() + camera->farPlane()) * 0.5f;
    const QVector3D planePoint = camera->position() + forward * middleDepth;
    const float denominator = QVector3D::dotProduct(rayDirection, forward);

    if (qAbs(denominator) <= 1.0e-8f)
        return planePoint;

    const float distance = QVector3D::dotProduct(planePoint - rayOrigin, forward) / denominator;

    if (distance < 0.0f)
        return planePoint;

    return rayOrigin + rayDirection * distance;
}





QVector3D OpenGLViewerWidget::screenPointToAnchor(const QPointF& position) const
{
    QVector3D scenePoint;

    if (scenePointAtWorld(position, scenePoint))
        return scenePoint;

    QVector3D anchor;

    if (navigationAnchor(anchor))
        return anchor;

    return m_coordinateSystem.worldOrigin();
}

bool OpenGLViewerWidget::scenePointAtWorldFromDepth(const QPointF& scene, QVector3D& world) const
{
    if (!m_sceneDepthValid || m_sceneDepthWidth <= 0 || m_sceneDepthHeight <= 0)
        return false;

    if (width() <= 0 || height() <= 0)
        return false;

    if (scene.x() < 0.0 || scene.x() >= width() || scene.y() < 0.0 || scene.y() >= height())
        return false;

    const int pixelX = static_cast<int>((scene.x() + 0.5) * m_sceneDepthWidth / width());
    const int pixelY = m_sceneDepthHeight - 1 - static_cast<int>((scene.y() + 0.5) * m_sceneDepthHeight / height());

    if (pixelX < 0 || pixelX >= m_sceneDepthWidth || pixelY < 0 || pixelY >= m_sceneDepthHeight)
        return false;

    const float depth = m_sceneDepthBuffer[pixelY * m_sceneDepthWidth + pixelX];

    if (depth >= 1.0f - 1.0e-7f)
        return false;

    const float ndcX = (static_cast<float>(pixelX) + 0.5f) / static_cast<float>(m_sceneDepthWidth) * 2.0f - 1.0f;
    const float ndcY = (static_cast<float>(pixelY) + 0.5f) / static_cast<float>(m_sceneDepthHeight) * 2.0f - 1.0f;
    const float ndcZ = depth * 2.0f - 1.0f;

    const QVector4D worldPoint = m_sceneDepthInverseViewProjection * QVector4D(ndcX, ndcY, ndcZ, 1.0f);

    if (qAbs(worldPoint.w()) <= 1.0e-8f)
        return false;

    world = QVector3D(worldPoint.x() / worldPoint.w(), worldPoint.y() / worldPoint.w(), worldPoint.z() / worldPoint.w());

    return true;
}
bool OpenGLViewerWidget::scenePointAtWorldFromRay(const QPointF& scene, QVector3D& world) const
{
    const Camera* camera = m_cameraManager.activeCamera();
    if (camera == 0 || width() <= 0 || height() <= 0)
        return false;
    //提取射线
    QVector3D rayOrigin;
    QVector3D rayDirection;
    if (!camera->screenPointToRay(scene.x(), scene.y(), width(), height(), rayOrigin, rayDirection))
        return false;

    bool found = false;
    float nearestDistance = 0.0f;
    QVector3D nearestPoint;

    const int itemCount = static_cast<int>(m_itemManager.count());
    //遍历Item检查是否存在Item与光线相交
    for (int index = 0; index < itemCount; ++index)
    {
        const RenderItem* item = m_itemManager.itemAt(index);
        if (item == 0 || !item->isVisible())
            continue;
        RenderItemRayHit hit;
        if (!item->raycast(rayOrigin, rayDirection, hit))
            continue;
        //找到最小的相交点
        if (!found || hit.distance < nearestDistance)
        {
            found = true;
            nearestDistance = hit.distance;
            nearestPoint = hit.position;
        }
    }

    if (!found)
        return false;

    world = nearestPoint;
    return true;
}
bool OpenGLViewerWidget::projectWorldPointToScene(const QVector3D& world, QPointF& scene) const
{
    if (width() <= 0 || height() <= 0)
        return false;

    const Camera* camera = m_cameraManager.activeCamera();

    if (camera == 0)
        return false;

    const float aspect = static_cast<float>(width()) / static_cast<float>(height());
    const QVector4D clip = camera->projectionMatrix(aspect) * camera->viewMatrix() * QVector4D(world, 1.0f);

    if (qAbs(clip.w()) <= 1.0e-8f)
        return false;

    if (camera->projectionType() == ProjectionType::Perspective && clip.w() <= 0.0f)
        return false;

    const double ndcX = static_cast<double>(clip.x() / clip.w());
    const double ndcY = static_cast<double>(clip.y() / clip.w());

    scene.setX((ndcX * 0.5 + 0.5) * width());
    scene.setY((0.5 - ndcY * 0.5) * height());

    return true;
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
    if(m_itemManager.worldBounds(bounds))
    {
        m_cameraManager.setViewBounds(bounds);
    }
    else
    {
        // 使用世界原点为中心的默认包围盒。
        bounds.set(QVector3D(-1.0f, -1.0f, -1.0f), QVector3D(1.0f, 1.0f, 1.0f));
    }

    if (!m_cameraManager.setViewDirection(bounds.center(), forward, up))
        return false;

    if (!m_cameraManager.fitBounds(bounds, width(), height()))
        return false;

    update();
    return true;
}

bool OpenGLViewerWidget::cacheSceneDepth(const RenderContext& context)
{
    QOpenGLFunctions_3_3_Core* gl = m_openGLContext.gl();

    if (gl == 0 || !context.isValid())
    {
        clearSceneDepthCache();
        return false;
    }

    bool invertible = false;
    const QMatrix4x4 inverse = (context.projection * context.view).inverted(&invertible);

    if (!invertible)
    {
        clearSceneDepthCache();
        return false;
    }

    GLint viewport[4] = { 0, 0, 0, 0 };
    gl->glGetIntegerv(GL_VIEWPORT, viewport);

    if (viewport[2] <= 0 || viewport[3] <= 0)
    {
        clearSceneDepthCache();
        return false;
    }

    m_sceneDepthWidth = viewport[2];
    m_sceneDepthHeight = viewport[3];
    m_sceneDepthInverseViewProjection = inverse;
    m_sceneDepthBuffer.resize(m_sceneDepthWidth * m_sceneDepthHeight);

    gl->glReadPixels(viewport[0], viewport[1], m_sceneDepthWidth, m_sceneDepthHeight, GL_DEPTH_COMPONENT, GL_FLOAT, m_sceneDepthBuffer.data());

    m_sceneDepthValid = true;

    return true;
}
void OpenGLViewerWidget::clearSceneDepthCache()
{
    m_sceneDepthBuffer.clear();
    m_sceneDepthInverseViewProjection.setToIdentity();
    m_sceneDepthWidth = 0;
    m_sceneDepthHeight = 0;
    m_sceneDepthValid = false;
}

/// Mouse

void OpenGLViewerWidget::mousePressEvent(QMouseEvent* event)
{
    if (m_measurementTool != 0 && m_measurementTool->mousePressEvent(this, event))
    {
        event->accept();
        update();
        return;
    }
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
    if (m_measurementTool != 0 && m_measurementTool->mouseMoveEvent(this, event))
    {
        event->accept();
        update();
        return;
    }

    const QPointF currentPosition = event->pos();
    const QPointF delta = currentPosition - m_lastMousePosition;

    m_lastMousePosition = currentPosition;

    if (!m_hasNavigationAnchor)
    {
        QOpenGLWidget::mouseMoveEvent(event);
        return;
    }

    if (event->buttons() & Qt::LeftButton)
    {
        const float degreesPerPixel = 0.3f;

        if (m_cameraManager.orbitAround(m_navigationAnchor, -delta.x() * degreesPerPixel, -delta.y() * degreesPerPixel))
            update();

        event->accept();
        return;
    }

    if (event->buttons() & Qt::MiddleButton)
    {
        if (m_cameraManager.panAt(m_navigationAnchor, delta.x(), delta.y(), width(), height()))
            update();

        event->accept();
        return;
    }

    QOpenGLWidget::mouseMoveEvent(event);
}

void OpenGLViewerWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_measurementTool != 0)
    {
        MeasurementTool* tool = m_measurementTool;
        const bool handled = tool->mouseReleaseEvent(this, event);

        if (event->button() == Qt::LeftButton && tool->state() == MeasurementState::Finished)
            emit measurementFinished(tool->type());

        if (handled)
        {
            event->accept();
            update();
            return;
        }
    }
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
    const QVector3D anchor = screenPointToZoomAnchor(event->pos());

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
    if (m_measurementTool != 0 && m_measurementTool->keyPressEvent(this, event))
    {
        event->accept();
        update();
        return;
    }

    if (handleKeyPress(event))
    {
        event->accept();
        return;
    }

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
    /// 子类扩展
    populateContextMenu(menu);
    if (!menu.isEmpty())
        menu.exec(event->globalPos());
    event->accept();
}

void OpenGLViewerWidget::resizeEvent(QResizeEvent* event)
{
    QOpenGLWidget::resizeEvent(event);

    if (m_viewportOverlay != 0)
    {
        m_viewportOverlay->setGeometry(rect());
        m_viewportOverlay->raise();
    }
}


/// 子类扩展

void OpenGLViewerWidget::populateContextMenu(QMenu& menu)
{
    Q_UNUSED(menu);
}

bool OpenGLViewerWidget::handleKeyPress(QKeyEvent* event)
{
    Q_UNUSED(event);
    return false;
}
void OpenGLViewerWidget::drawViewportOverlay(QPainter& painter)
{

    /// 当前正在操作的临时 Overlay。
    if (m_measurementTool != 0)
        m_measurementTool->drawOverlay(this, painter);
}
void OpenGLViewerWidget::updateViewportOverlay()
{
    if (m_viewportOverlay != 0)
        m_viewportOverlay->update();
}