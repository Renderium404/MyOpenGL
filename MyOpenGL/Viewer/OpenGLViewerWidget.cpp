#include "OpenGLViewerWidget.h"

#include <QDebug>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <QVector4D>
#include <QWheelEvent>

#include <cmath>

#include "MyOpenGL/Camera/Camera.h"
#include "MyOpenGL/Material/Material.h"
#include "MyOpenGL/Resource/Geometry.h"
#include "MyOpenGL/Scene/RenderItem.h"
#include "MyOpenGL/Scene/RenderPart.h"

OpenGLViewerWidget::OpenGLViewerWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , m_systemVertexColorMaterial(0)
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

    Camera* camera = new Camera();
    m_cameraManager.add(camera);

    buildViewerResources();
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

Scene& OpenGLViewerWidget::scene()
{
    return m_scene;
}

const Scene& OpenGLViewerWidget::scene() const
{
    return m_scene;
}

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

    if (!m_renderer.initialize(&m_openGLContext))
    {
        qWarning() << "OpenGLViewerWidget initializeGL failed: Renderer initialization failed.";
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
    fitSceneToView();
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

    if (!m_resourceManager.syncAll(gl))
    {
        qWarning() << "OpenGLViewerWidget paintGL failed: Resource synchronization failed.";
        return;
    }

    RenderContext renderContext;

    if (!buildRenderContext(renderContext))
    {
        qWarning() << "OpenGLViewerWidget paintGL failed: unable to build RenderContext.";
        return;
    }

    if (!m_renderer.beginFrame(renderContext))
        return;

    if (!drawScene(renderContext))
    {
        qWarning() << "OpenGLViewerWidget paintGL failed: Scene drawing failed.";
        m_renderer.endFrame();
        return;
    }

    /// 世界坐标系

    if (m_coordinateSystem.isVisible())
    {
        RenderState coordinateState;

        if (m_coordinateSystem.buildRenderState(renderContext, coordinateState))
        {
            if (!m_renderer.clearDepth(coordinateState.viewport))
            {
                qWarning() << "OpenGLViewerWidget paintGL failed: CoordinateSystem depth clearing failed.";
            }
            else
            {
                if (!m_renderer.drawGeometry(&m_coordinateSystem.geometry(), m_systemVertexColorMaterial, coordinateState))
                    qWarning() << "OpenGLViewerWidget paintGL failed: CoordinateSystem drawing failed.";
            }
        }
    }

    /// 右上角视图导航

    if (m_viewNavigation.isVisible())
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
                if (!m_renderer.drawGeometry(&m_viewNavigation.faceGeometry(), m_systemVertexColorMaterial, navigationState))
                    qWarning() << "OpenGLViewerWidget paintGL failed: ViewNavigation face drawing failed.";

                if (!m_renderer.drawGeometry(&m_viewNavigation.axisGeometry(), m_systemVertexColorMaterial, navigationState))
                    qWarning() << "OpenGLViewerWidget paintGL failed: ViewNavigation axis drawing failed.";
            }
        }
    }

    m_renderer.endFrame();
}

void OpenGLViewerWidget::releaseViewerGL()
{
    if (m_releasePerformed)
        return;

    m_releasePerformed = true;

    if (context() != 0)
    {
        makeCurrent();

        if (QOpenGLContext::currentContext() == context())
        {
            QOpenGLFunctions_3_3_Core* gl = m_openGLContext.gl();

            if (gl != 0)
            {
                if (!m_resourceManager.releaseGL(gl))
                    qWarning() << "OpenGLViewerWidget releaseViewerGL: some Resources failed to release.";

                m_renderer.release();
            }

            doneCurrent();
        }
    }

    m_glReady = false;
}

/// Viewer 系统资源

void OpenGLViewerWidget::buildViewerResources()
{
    /// 系统模型统一使用顶点颜色 Material。
    /// Material 由 MaterialManager 管理对象生命周期。

    m_systemVertexColorMaterial = new Material("ViewerSystemVertexColor");
    m_systemVertexColorMaterial->setVertexColor();
    m_materialManager.add(m_systemVertexColorMaterial);

    /// CoordinateSystem / ViewNavigation 自己拥有 Geometry。
    /// ResourceManager 这里只登记资源，不接管对象生命周期。

    if (m_resourceManager.registerResource(&m_coordinateSystem.geometry()) == InvalidResourceId)
        qWarning() << "OpenGLViewerWidget buildViewerResources failed: unable to register CoordinateSystem Geometry.";

    if (m_resourceManager.registerResource(&m_viewNavigation.faceGeometry()) == InvalidResourceId)
        qWarning() << "OpenGLViewerWidget buildViewerResources failed: unable to register ViewNavigation Face Geometry.";

    if (m_resourceManager.registerResource(&m_viewNavigation.axisGeometry()) == InvalidResourceId)
        qWarning() << "OpenGLViewerWidget buildViewerResources failed: unable to register ViewNavigation Axis Geometry.";

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

    // 正常情况下 releaseViewerGL() 已经释放 GPU 状态，
    // 此处不需要 gl。这里仍尝试获取 Context，作为异常释放路径的保护。
    if (context() != 0)
    {
        makeCurrent();

        if (QOpenGLContext::currentContext() == context())
        {
            gl = m_openGLContext.gl();
            contextCurrent = true;
        }
    }

    ResourceId id = m_coordinateSystem.geometry().id();

    if (id != InvalidResourceId && !m_resourceManager.unregisterResource(id, gl))
        qWarning() << "OpenGLViewerWidget unregisterViewerResources failed: CoordinateSystem Geometry.";

    id = m_viewNavigation.faceGeometry().id();

    if (id != InvalidResourceId && !m_resourceManager.unregisterResource(id, gl))
        qWarning() << "OpenGLViewerWidget unregisterViewerResources failed: ViewNavigation Face Geometry.";

    id = m_viewNavigation.axisGeometry().id();

    if (id != InvalidResourceId && !m_resourceManager.unregisterResource(id, gl))
        qWarning() << "OpenGLViewerWidget unregisterViewerResources failed: ViewNavigation Axis Geometry.";

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

bool OpenGLViewerWidget::drawScene(const RenderContext& context)
{
    for (int itemIndex = 0; itemIndex < m_scene.itemCount(); ++itemIndex)
    {
        const RenderItem* item = m_scene.item(itemIndex);

        if (item == 0)
            continue;

        if (!drawItem(item, context))
        {
            qWarning() << "OpenGLViewerWidget drawScene failed while drawing Item:" << item->name();
            return false;
        }
    }

    return true;
}

bool OpenGLViewerWidget::drawItem(const RenderItem* item, const RenderContext& context)
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

        // Line / LineStrip 不参与 Triangle DisplayMode。
        if (geometry->renderType() != Triangles)
        {
            if (material == 0)
            {
                qWarning() << "OpenGLViewerWidget drawItem failed: non-triangle Part requires Material:"
                           << "Item=" << item->name()
                           << "PartId=" << static_cast<qulonglong>(part->id());
                return false;
            }

            drawSucceeded = m_renderer.drawGeometry(geometry, material, state, &m_lightManager);
        }
        else
        {
            switch (item->displayMode())
            {
            case RenderItemDisplayShaded:
                if (material == 0)
                {
                    qWarning() << "OpenGLViewerWidget drawItem failed: shaded Part requires Material:"
                               << "Item=" << item->name()
                               << "PartId=" << static_cast<qulonglong>(part->id());
                    return false;
                }

                drawSucceeded = m_renderer.drawGeometry(geometry, material, state, &m_lightManager);
                break;

            case RenderItemDisplayWireframe:
                drawSucceeded = m_renderer.drawWireGeometry(geometry, item->edgeColor(), state, false);
                break;

            case RenderItemDisplayShadedWithEdges:
                if (material == 0)
                {
                    qWarning() << "OpenGLViewerWidget drawItem failed: shaded Part requires Material:"
                               << "Item=" << item->name()
                               << "PartId=" << static_cast<qulonglong>(part->id());
                    return false;
                }

                drawSucceeded = m_renderer.drawGeometry(geometry, material, state, &m_lightManager);

                if (drawSucceeded)
                    drawSucceeded = m_renderer.drawWireGeometry(geometry, item->edgeColor(), state, true);

                break;

            default:
                qWarning() << "OpenGLViewerWidget drawItem failed: unsupported DisplayMode:"
                           << "Item=" << item->name();
                return false;
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

bool OpenGLViewerWidget::fitSceneToView(float margin)
{
    if (width() <= 0 || height() <= 0)
        return false;

    AxisAlignedBoundingBox bounds;

    if (!m_scene.worldBounds(bounds, true))
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

    if (!fitSceneToView())
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

    if (!m_scene.worldBounds(bounds, true))
        return false;

    anchor = bounds.center();
    return true;
}
bool OpenGLViewerWidget::scenePointAt(const QPoint& position, QVector3D& point)
{
    if (!m_glReady || width() <= 0 || height() <= 0)
        return false;
    const Camera* camera = m_cameraManager.activeCamera();
    if (camera == 0 || context() == 0)
        return false;

    makeCurrent();
    QOpenGLFunctions_3_3_Core* gl = m_openGLContext.gl();
    if (gl == 0)
    {
        doneCurrent();
        return false;
    }
    const qreal pixelRatio = devicePixelRatioF();
    const int framebufferWidth = static_cast<int>(width() * pixelRatio);
    const int framebufferHeight = static_cast<int>(height() * pixelRatio);
    const int pixelX = static_cast<int>(position.x() * pixelRatio);
    const int pixelY = static_cast<int>((height() - 1 - position.y()) * pixelRatio);
    if (pixelX < 0 || pixelX >= framebufferWidth || pixelY < 0 || pixelY >= framebufferHeight)
    {
        doneCurrent();
        return false;
    }

    gl->glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
    GLfloat depth = 1.0f;
    gl->glReadPixels(pixelX, pixelY, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
    doneCurrent();
    // Depth = 1 表示该位置没有渲染到任何模型。
    if (depth >= 1.0f - 1.0e-6f)
        return false;

    const float ndcX =
        (static_cast<float>(position.x()) + 0.5f) /
        static_cast<float>(width()) * 2.0f - 1.0f;

    const float ndcY =
        1.0f -
        (static_cast<float>(position.y()) + 0.5f) /
        static_cast<float>(height()) * 2.0f;

    const float ndcZ = depth * 2.0f - 1.0f;

    const float aspect = static_cast<float>(width()) / static_cast<float>(height());

    const QMatrix4x4 viewProjection =
        camera->projectionMatrix(aspect) *
        camera->viewMatrix();

    bool invertible = false;
    const QMatrix4x4 inverseViewProjection = viewProjection.inverted(&invertible);

    if (!invertible)
        return false;

    QVector4D worldPosition =
        inverseViewProjection *
        QVector4D(ndcX, ndcY, ndcZ, 1.0f);

    if (std::fabs(worldPosition.w()) <= 1.0e-8f)
        return false;

    worldPosition /= worldPosition.w();
    point = worldPosition.toVector3D();

    return true;
}
/// Mouse

void OpenGLViewerWidget::mousePressEvent(QMouseEvent* event)
{
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
    }

    m_lastMousePosition = event->pos();

    if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton)
    {
        if (!scenePointAt(event->pos(), m_navigationAnchor))
            m_hasNavigationAnchor = navigationAnchor(m_navigationAnchor);
        else
            m_hasNavigationAnchor = true;

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

    if (event->buttons() & Qt::LeftButton)
    {
        if (!m_hasNavigationAnchor)
            return;

        const float degreesPerPixel = 0.3f;

        if (m_cameraManager.orbitAround(
                m_navigationAnchor,
                -delta.x() * degreesPerPixel,
                -delta.y() * degreesPerPixel))
        {
            update();
        }

        event->accept();
        return;
    }

    if (event->buttons() & Qt::MiddleButton)
    {
        if (!m_hasNavigationAnchor)
            return;

        if (m_cameraManager.panAt(
                m_navigationAnchor,
                delta.x(),
                delta.y(),
                width(),
                height()))
        {
            update();
        }

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
        event->accept();
        return;
    }

    QOpenGLWidget::mouseReleaseEvent(event);
}

void OpenGLViewerWidget::wheelEvent(QWheelEvent* event)
{
    QVector3D anchor;

    if (!scenePointAt(event->pos(), anchor))
    {
        if (!navigationAnchor(anchor))
        {
            QOpenGLWidget::wheelEvent(event);
            return;
        }
    }

    const float wheelSteps = static_cast<float>(event->angleDelta().y()) / 120.0f;
    const float factor = static_cast<float>(std::pow(1.15, wheelSteps));

    if (m_cameraManager.zoomAt(anchor, factor, width(), height()))
        update();

    event->accept();
}

/// Keyboard

void OpenGLViewerWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_F)
    {
        fitSceneToView();
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