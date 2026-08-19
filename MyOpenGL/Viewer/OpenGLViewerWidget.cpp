#include "OpenGLViewerWidget.h"

#include <QApplication>
#include <QDebug>
#include <QKeyEvent>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QLabel>
#include <QMenu>
#include <QOpenGLContext>
#include <QWheelEvent>
#include <QSurfaceFormat>
#include <algorithm>
#include <cmath>
#include <vector>

#include "MyOpenGL/Camera/Camera.h"
#include "MyOpenGL/Resource/BufferGeometry.h"
#include "MyOpenGL/Resource/Geometry.h"
#include "MyOpenGL/Viewer/Geometry/CoordinateSystemGeometry.h"
#include "MyOpenGL/Viewer/Geometry/GridPlaneGeometry.h"
#include "MyOpenGL/Viewer/Geometry/ViewNavigationGeometry.h"
#include "MyOpenGL/Scene/AxisAlignedBoundingBox.h"
#include "MyOpenGL/Scene/RenderItem.h"

const char* viewerPickModeName(ViewerPickMode mode)
{
    switch (mode)
    {
    case ViewerPickModePoint:
        return "Point";
    case ViewerPickModeTriangle:
        return "Triangle";
    case ViewerPickModeItem:
        return "Item";
    }

    return "Unknown";
}

OpenGLViewerWidget::OpenGLViewerWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , m_pickedItem(0)
    , m_grid(0)
    , m_axes(0)
    , m_viewNavigation(0)
    , m_cameraTargetMarker(0)
    , m_boundsHighlight(0)
    , m_primitiveHighlight(0)
    , m_measurementMarker(0)
    , m_measurementLine(0)
    , m_measurementPointALabel(0)
    , m_measurementPointBLabel(0)
    , m_measurementLengthLabel(0)
    , m_rightDragOccurred(false)
    , m_glReady(false)
    , m_releasePerformed(false)
    , m_showGrid(false)
    , m_showAxes(true)
    , m_showViewNavigation(true)
    , m_showCameraTarget(true)
    , m_showBoundsHighlight(false)
    , m_pickMode(ViewerPickModePoint)
    , m_primitivePickTolerance(5.0f)
    , m_pointPickTolerance(8.0f)
    , m_showPrimitiveHighlight(false)
    , m_hasMeasurementPointA(false)
    , m_hasMeasurementPointB(false)
    , m_measurementPointA(0.0f, 0.0f, 0.0f)
    , m_measurementPointB(0.0f, 0.0f, 0.0f)
{
    // MyOpenGL Renderer 使用 QOpenGLFunctions_3_3_Core 和 GLSL 330。
    // Viewer 自己在 QOpenGLWidget Context 创建前请求 3.3 Core，避免依赖宿主业务 main() 的全局 QSurfaceFormat。
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);   // Scene 深度测试需要稳定的 Depth Buffer。
    format.setStencilBufferSize(8);  // 保留常规 Widget Stencil 能力，不影响当前 Renderer。
    setFormat(format);
    setFocusPolicy(Qt::StrongFocus);

    // Measurement 文字使用普通 QWidget Child Overlay。
    // Qt 5.8 在 OpenGL 3.3 Core Profile 下的 QPainter OpenGL Paint Engine 会生成不兼容 Shader，
    // 因此不能在 QOpenGLWidget::paintGL() 中直接使用 QPainter(this) 绘制文字。
    const QString measurementLabelStyle =
        "QLabel {"
        " color: rgb(220, 255, 245);"
        " background-color: rgba(20, 24, 28, 180);"
        " border: 1px solid rgba(100, 180, 170, 180);"
        " padding: 2px;"
        "}";

    m_measurementPointALabel = new QLabel(this);
    m_measurementPointALabel->setStyleSheet(measurementLabelStyle);
    m_measurementPointALabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_measurementPointALabel->hide();

    m_measurementPointBLabel = new QLabel(this);
    m_measurementPointBLabel->setStyleSheet(measurementLabelStyle);
    m_measurementPointBLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_measurementPointBLabel->hide();

    m_measurementLengthLabel = new QLabel(this);
    m_measurementLengthLabel->setStyleSheet(measurementLabelStyle);
    m_measurementLengthLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_measurementLengthLabel->hide();

    buildViewerResources();
    resetCamera();
}

OpenGLViewerWidget::~OpenGLViewerWidget()
{
    releaseViewerGL();
}

/// Framework 状态

ResourceManager& OpenGLViewerWidget::resourceManager()
{
    return m_resourceManager;
}

const ResourceManager& OpenGLViewerWidget::resourceManager() const
{
    return m_resourceManager;
}

CameraManager& OpenGLViewerWidget::cameraManager()
{
    return m_cameraManager;
}

const CameraManager& OpenGLViewerWidget::cameraManager() const
{
    return m_cameraManager;
}

LightManager& OpenGLViewerWidget::lightManager()
{
    return m_lightManager;
}

const LightManager& OpenGLViewerWidget::lightManager() const
{
    return m_lightManager;
}

MaterialManager& OpenGLViewerWidget::materialManager()
{
    return m_materialManager;
}

const MaterialManager& OpenGLViewerWidget::materialManager() const
{
    return m_materialManager;
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

/// Viewer 显示状态

bool OpenGLViewerWidget::gridVisible() const
{
    return m_showGrid;
}

bool OpenGLViewerWidget::axesVisible() const
{
    return m_showAxes;
}

bool OpenGLViewerWidget::viewNavigationVisible() const
{
    return m_showViewNavigation;
}

bool OpenGLViewerWidget::cameraTargetVisible() const
{
    return m_showCameraTarget;
}

void OpenGLViewerWidget::setGridVisible(bool visible)
{
    m_showGrid = visible;
    viewerStateChanged();
    update();
}

void OpenGLViewerWidget::setAxesVisible(bool visible)
{
    m_showAxes = visible;
    viewerStateChanged();
    update();
}

void OpenGLViewerWidget::setViewNavigationVisible(bool visible)
{
    m_showViewNavigation = visible;
    viewerStateChanged();
    update();
}

void OpenGLViewerWidget::setCameraTargetVisible(bool visible)
{
    m_showCameraTarget = visible;
    viewerStateChanged();
    update();
}

/// Item 显示状态

bool OpenGLViewerWidget::setItemVisible(RenderItem* item, bool visible)
{
    if (item == 0)
    {
        qWarning() << "OpenGLViewerWidget setItemVisible failed: item is null.";
        return false;
    }

    bool belongsToScene = false;

    for (int i = 0; i < m_scene.itemCount(); ++i)
    {
        if (m_scene.item(i) == item)
        {
            belongsToScene = true;
            break;
        }
    }

    if (!belongsToScene)
    {
        qWarning() << "OpenGLViewerWidget setItemVisible failed: item does not belong to current Scene.";
        return false;
    }

    if (item->isVisible() == visible)
        return true;

    item->setVisible(visible);

    // Hidden Item 仍保留在 Picking Candidate 集合中；Scene Raycast 的 visibleOnly 会自动忽略它。
    // 这样重新显示 Item 后无需重新建立 Picking Candidate 身份。
    if (!visible && m_pickedItem == item)
    {
        clearPickedItem();
        clearBoundsHighlight();
        clearPrimitiveHighlight();
    }

    viewerStateChanged();
    update();
    return true;
}

bool OpenGLViewerWidget::setItemsVisible(const std::vector<RenderItem*>& items, bool visible)
{
    // 批量操作先验证全部输入，避免只修改前半部分 Item 后因非法对象中断。
    for (std::size_t itemIndex = 0; itemIndex < items.size(); ++itemIndex)
    {
        RenderItem* item = items[itemIndex];

        if (item == 0)
        {
            qWarning() << "OpenGLViewerWidget setItemsVisible failed: item is null.";
            return false;
        }

        bool belongsToScene = false;

        for (int sceneIndex = 0; sceneIndex < m_scene.itemCount(); ++sceneIndex)
        {
            if (m_scene.item(sceneIndex) == item)
            {
                belongsToScene = true;
                break;
            }
        }

        if (!belongsToScene)
        {
            qWarning() << "OpenGLViewerWidget setItemsVisible failed: item does not belong to current Scene.";
            return false;
        }
    }

    bool pickedItemHidden = false;
    bool changed = false;

    for (std::size_t itemIndex = 0; itemIndex < items.size(); ++itemIndex)
    {
        RenderItem* item = items[itemIndex];

        if (item->isVisible() == visible)
            continue;

        item->setVisible(visible);
        changed = true;

        if (!visible && m_pickedItem == item)
            pickedItemHidden = true;
    }

    if (pickedItemHidden)
    {
        clearPickedItem();
        clearBoundsHighlight();
        clearPrimitiveHighlight();
    }

    if (changed)
    {
        viewerStateChanged();
        update();
    }

    return true;
}

void OpenGLViewerWidget::setAllItemsVisible(bool visible)
{
    bool changed = false;

    for (int i = 0; i < m_scene.itemCount(); ++i)
    {
        RenderItem* item = m_scene.item(i);

        if (item == 0 || item->isVisible() == visible)
            continue;

        item->setVisible(visible);
        changed = true;
    }

    if (!visible && m_pickedItem != 0)
    {
        clearPickedItem();
        clearBoundsHighlight();
        clearPrimitiveHighlight();
    }

    if (changed)
    {
        viewerStateChanged();
        update();
    }
}

bool OpenGLViewerWidget::isolateItems(const std::vector<RenderItem*>& items)
{
    if (items.empty())
    {
        qWarning() << "OpenGLViewerWidget isolateItems failed: item list is empty.";
        return false;
    }

    // Isolate 同样先验证全部目标属于当前 Scene，避免非法输入导致部分可见性修改。
    for (std::size_t itemIndex = 0; itemIndex < items.size(); ++itemIndex)
    {
        RenderItem* item = items[itemIndex];

        if (item == 0)
        {
            qWarning() << "OpenGLViewerWidget isolateItems failed: item is null.";
            return false;
        }

        bool belongsToScene = false;

        for (int sceneIndex = 0; sceneIndex < m_scene.itemCount(); ++sceneIndex)
        {
            if (m_scene.item(sceneIndex) == item)
            {
                belongsToScene = true;
                break;
            }
        }

        if (!belongsToScene)
        {
            qWarning() << "OpenGLViewerWidget isolateItems failed: item does not belong to current Scene.";
            return false;
        }
    }

    bool changed = false;

    for (int sceneIndex = 0; sceneIndex < m_scene.itemCount(); ++sceneIndex)
    {
        RenderItem* sceneItem = m_scene.item(sceneIndex);

        if (sceneItem == 0)
            continue;

        const bool shouldBeVisible = std::find(items.begin(), items.end(), sceneItem) != items.end();

        if (sceneItem->isVisible() == shouldBeVisible)
            continue;

        sceneItem->setVisible(shouldBeVisible);
        changed = true;
    }

    // 当前 Picked Item 若不在 Isolate 集合中，则其可见性已经变为 false。
    if (m_pickedItem != 0 && std::find(items.begin(), items.end(), m_pickedItem) == items.end())
    {
        clearPickedItem();
        clearBoundsHighlight();
        clearPrimitiveHighlight();
    }

    if (changed)
    {
        viewerStateChanged();
        update();
    }

    return true;
}

/// Measurement

void OpenGLViewerWidget::setMeasurementPointA(const QVector3D& point)
{
    m_measurementPointA = point;
    m_hasMeasurementPointA = true;
    m_hasMeasurementPointB = false;

    qDebug() << "OpenGLViewerWidget Measurement Point A:"
             << "Position=" << m_measurementPointA;

    viewerStateChanged();
    update();
}

bool OpenGLViewerWidget::setMeasurementPointB(const QVector3D& point)
{
    if (!m_hasMeasurementPointA)
    {
        qWarning() << "OpenGLViewerWidget setMeasurementPointB failed: Point A does not exist.";
        return false;
    }

    m_measurementPointB = point;
    m_hasMeasurementPointB = true;

    if (m_measurementLine != 0)
    {
        const GLfloat vertices[] =
        {
            m_measurementPointA.x(), m_measurementPointA.y(), m_measurementPointA.z(),    0.15f, 1.0f, 0.85f,
            m_measurementPointB.x(), m_measurementPointB.y(), m_measurementPointB.z(),    0.15f, 1.0f, 0.85f
        };

        const int vertexValueCount = sizeof(vertices) / sizeof(vertices[0]);

        if (!m_measurementLine->updateVertexData(0, vertices, vertexValueCount))
        {
            qWarning() << "OpenGLViewerWidget setMeasurementPointB failed: unable to update Measurement Line.";
            m_hasMeasurementPointB = false;
            return false;
        }
    }

    qDebug() << "OpenGLViewerWidget Measurement Point B:"
             << "Position=" << m_measurementPointB
             << "Length=" << measurementLength();

    viewerStateChanged();
    update();
    return true;
}

bool OpenGLViewerWidget::measurementPointA(QVector3D& point) const
{
    if (!m_hasMeasurementPointA)
        return false;

    point = m_measurementPointA;
    return true;
}

bool OpenGLViewerWidget::measurementPointB(QVector3D& point) const
{
    if (!m_hasMeasurementPointB)
        return false;

    point = m_measurementPointB;
    return true;
}

float OpenGLViewerWidget::measurementLength() const
{
    if (!m_hasMeasurementPointA || !m_hasMeasurementPointB)
        return 0.0f;

    return (m_measurementPointB - m_measurementPointA).length();
}

void OpenGLViewerWidget::clearMeasurement()
{
    const bool changed = m_hasMeasurementPointA || m_hasMeasurementPointB;

    m_hasMeasurementPointA = false;
    m_hasMeasurementPointB = false;

    if (m_measurementPointALabel != 0)
        m_measurementPointALabel->hide();

    if (m_measurementPointBLabel != 0)
        m_measurementPointBLabel->hide();

    if (m_measurementLengthLabel != 0)
        m_measurementLengthLabel->hide();

    if (changed)
    {
        // qDebug() << "OpenGLViewerWidget Measurement cleared.";
        viewerStateChanged();
        update();
    }
}

/// Picking Mode

ViewerPickMode OpenGLViewerWidget::pickMode() const
{
    return m_pickMode;
}

bool OpenGLViewerWidget::setPickMode(ViewerPickMode mode)
{
    switch (mode)
    {
    case ViewerPickModePoint:
    case ViewerPickModeTriangle:
    case ViewerPickModeItem:
        break;

    default:
        qWarning() << "OpenGLViewerWidget setPickMode failed: unsupported mode:" << static_cast<int>(mode);
        return false;
    }

    if (m_pickMode == mode)
        return true;

    m_pickMode = mode;

    // Pick / Highlight 表达的是上一模式的命中语义，切换模式时清除；
    // Measurement 是已经形成的明确 World Geometry，不依赖当前 Picking Mode，因此保留。
    clearPickedItem();
    clearBoundsHighlight();
    clearPrimitiveHighlight();

    // qDebug() << "OpenGLViewerWidget Picking Mode changed:" << viewerPickModeName(m_pickMode);

    viewerStateChanged();
    update();
    return true;
}

void OpenGLViewerWidget::cyclePickMode()
{
    switch (m_pickMode)
    {
    case ViewerPickModePoint:
        // setPickMode(ViewerPickModeTriangle);
        setPickMode(ViewerPickModeItem);
        break;

    // case ViewerPickModeTriangle:
    //     setPickMode(ViewerPickModeItem);
    //     break;

    case ViewerPickModeItem:
        setPickMode(ViewerPickModePoint);
        break;
    }
}

/// Picking Candidates

const RenderItemCandidates& OpenGLViewerWidget::pickCandidates() const
{
    return m_pickCandidates;
}

bool OpenGLViewerWidget::addPickCandidate(RenderItem* item)
{
    if (item == 0)
    {
        qWarning() << "OpenGLViewerWidget addPickCandidate failed: item is null.";
        return false;
    }

    bool belongsToScene = false;

    for (int i = 0; i < m_scene.itemCount(); ++i)
    {
        if (m_scene.item(i) == item)
        {
            belongsToScene = true;
            break;
        }
    }

    if (!belongsToScene)
    {
        qWarning() << "OpenGLViewerWidget addPickCandidate failed: item does not belong to current Scene.";
        return false;
    }

    if (std::find(m_pickCandidates.begin(), m_pickCandidates.end(), item) != m_pickCandidates.end())
        return true;

    m_pickCandidates.push_back(item);
    return true;
}

bool OpenGLViewerWidget::removePickCandidate(RenderItem* item)
{
    if (item == 0)
    {
        qWarning() << "OpenGLViewerWidget removePickCandidate failed: item is null.";
        return false;
    }

    RenderItemCandidates::iterator it = std::find(m_pickCandidates.begin(), m_pickCandidates.end(), item);

    if (it == m_pickCandidates.end())
        return false;

    m_pickCandidates.erase(it);
    return true;
}

void OpenGLViewerWidget::clearPickCandidates()
{
    m_pickCandidates.clear();
}

float OpenGLViewerWidget::primitivePickTolerance() const
{
    return m_primitivePickTolerance;
}

bool OpenGLViewerWidget::setPrimitivePickTolerance(float pixels)
{
    if (pixels <= 0.0f)
    {
        qWarning() << "OpenGLViewerWidget setPrimitivePickTolerance failed: pixels must be greater than zero.";
        return false;
    }

    m_primitivePickTolerance = pixels;
    return true;
}

float OpenGLViewerWidget::pointPickTolerance() const
{
    return m_pointPickTolerance;
}

bool OpenGLViewerWidget::setPointPickTolerance(float pixels)
{
    if (pixels <= 0.0f)
    {
        qWarning() << "OpenGLViewerWidget setPointPickTolerance failed: pixels must be greater than zero.";
        return false;
    }

    m_pointPickTolerance = pixels;
    return true;
}

/// 派生内容扩展

bool OpenGLViewerWidget::initializeViewerContentGL(QOpenGLFunctions_3_3_Core* gl)
{
    Q_UNUSED(gl);
    return true;
}

void OpenGLViewerWidget::buildViewerContentItems()
{
}

bool OpenGLViewerWidget::handleViewerKeyPress(QKeyEvent* event)
{
    Q_UNUSED(event);
    return false;
}

void OpenGLViewerWidget::populateViewerContextMenu(QMenu* menu, const QPoint& position)
{
    Q_UNUSED(menu);
    Q_UNUSED(position);
}

void OpenGLViewerWidget::viewerStateChanged()
{
}

void OpenGLViewerWidget::afterViewerGLReleased()
{
}

/// OpenGL 生命周期

void OpenGLViewerWidget::initializeGL()
{
    m_releasePerformed = false;
    m_glReady = false;

    if (!m_renderContext.initialize())
    {
        qWarning() << "OpenGLViewerWidget initializeGL failed: RenderContext initialization failed.";
        return;
    }

    if (!m_renderer.initialize(&m_renderContext))
    {
        qWarning() << "OpenGLViewerWidget initializeGL failed: Renderer initialization failed.";
        return;
    }

    QOpenGLFunctions_3_3_Core* gl = m_renderContext.gl();

    if (gl == 0)
    {
        qWarning() << "OpenGLViewerWidget initializeGL failed: OpenGL functions are unavailable.";
        releaseViewerGL();
        return;
    }

    // 派生 Viewer 可在这里创建 Shared Context、External GPU Resource 等必须等待 Widget Context ready 的内容。
    if (!initializeViewerContentGL(gl))
    {
        qWarning() << "OpenGLViewerWidget initializeGL failed: derived Viewer content initialization failed.";
        releaseViewerGL();
        return;
    }

    // 每次 Context 初始化都重建用户 RenderItem；Viewer 内部辅助模型始终由 Viewer 自己直接绘制。
    rebuildViewerSceneItems();

    // 构造阶段 CameraManager 使用默认原点单位盒；Scene Item 建立后，如果存在实际业务 Bounds，
    // 则显式用 Scene Bounds 替换当前 View Bounds，并保持 resetCamera() 设置的默认观察方向完成初始 Fit。
    AxisAlignedBoundingBox initialSceneBounds;

    if (m_scene.worldBounds(initialSceneBounds, true))
    {
        if (!m_cameraManager.fitBounds(initialSceneBounds, width(), height()))
        {
            qWarning() << "OpenGLViewerWidget initializeGL failed: unable to fit initial Scene Bounds.";
            releaseViewerGL();
            return;
        }
    }

    if (!m_resourceManager.syncAll(gl))
    {
        qWarning() << "OpenGLViewerWidget initializeGL failed: Resource synchronization failed.";
        releaseViewerGL();
        return;
    }

    m_renderer.setClearColor(QVector4D(0.86f, 0.91f, 0.97f, 1.0f));
    m_glReady = true;
    viewerStateChanged();
}

void OpenGLViewerWidget::resizeGL(int width, int height)
{
    Q_UNUSED(width);
    Q_UNUSED(height);

    // Viewport 由 Renderer::beginFrame() 根据当前 Widget 尺寸统一设置。
}

void OpenGLViewerWidget::paintGL()
{
    if (!m_glReady)
        return;

    QOpenGLFunctions_3_3_Core* gl = m_renderContext.gl();

    if (gl == 0)
        return;

    // 所有 Owned / External Resource 都统一通过 ResourceManager 完成当前帧前的状态同步。
    if (!m_resourceManager.syncAll(gl))
    {
        qWarning() << "OpenGLViewerWidget paintGL failed: Resource synchronization failed.";
        return;
    }

    const Camera* camera = m_cameraManager.activeCamera();

    if (camera == 0)
    {
        qWarning() << "OpenGLViewerWidget paintGL failed: active camera does not exist.";
        return;
    }

    if (!m_renderer.beginFrame(camera, width(), height()))
        return;

    QMatrix4x4 identityModel;

    // Viewer 内部世界辅助元素直接走低层 Renderer，不进入 Scene / RenderItem。
    // Axis 必须先于 Grid 绘制，使与 Grid 共面的 X / Z 轴在 GL_LESS 深度测试下优先保留。
    if (m_showAxes && m_axes != 0)
    {
        if (!m_renderer.drawVertexColorGeometry(m_axes, identityModel, true))
        {
            qWarning() << "OpenGLViewerWidget paintGL failed: World Axis drawing failed.";
            m_renderer.endFrame();
            return;
        }
    }

    if (m_showGrid && m_grid != 0)
    {
        if (!m_renderer.drawVertexColorGeometry(m_grid, identityModel, true))
        {
            qWarning() << "OpenGLViewerWidget paintGL failed: Grid drawing failed.";
            m_renderer.endFrame();
            return;
        }
    }

    // Scene 只绘制用户可操作 RenderItem。
    if (!m_renderer.drawScene(&m_scene, &m_resourceManager, &m_lightManager))
    {
        qWarning() << "OpenGLViewerWidget paintGL failed: user Scene drawing failed.";
        m_renderer.endFrame();
        return;
    }

    if (m_showCameraTarget && m_cameraTargetMarker != 0)
    {
        // CameraManager 的视图中心由 View Bounds Center 决定。
        // Marker 是 Viewer 内部辅助模型：世界位置取 View Bounds Center，屏幕视觉尺寸随 Camera Distance 近似保持稳定。
        const float targetMarkerScale = camera->distanceToTarget() * 0.02f;
        QMatrix4x4 targetModel;
        targetModel.translate(m_cameraManager.viewBounds().center());
        targetModel.scale(targetMarkerScale);

        if (!m_renderer.drawVertexColorGeometry(m_cameraTargetMarker, targetModel, false))
        {
            qWarning() << "OpenGLViewerWidget paintGL failed: Camera Target drawing failed.";
            m_renderer.endFrame();
            return;
        }
    }

    if (m_showBoundsHighlight && m_boundsHighlight != 0)
    {
        // Highlight 顶点已经是 World Space，因此 Model Matrix 保持 Identity；关闭 Depth Test 保证全部边可见。
        if (!m_renderer.drawVertexColorGeometry(m_boundsHighlight, identityModel, false))
        {
            qWarning() << "OpenGLViewerWidget paintGL failed: Bounds Highlight drawing failed.";
            m_renderer.endFrame();
            return;
        }
    }

    if (m_showPrimitiveHighlight && m_primitiveHighlight != 0)
    {
        // Primitive Highlight 直接保存明确 World Primitive Vertex，不经过 RenderItem Transform。
        if (!m_renderer.drawVertexColorGeometry(m_primitiveHighlight, identityModel, false))
        {
            qWarning() << "OpenGLViewerWidget paintGL failed: Primitive Highlight drawing failed.";
            m_renderer.endFrame();
            return;
        }
    }

    // Measurement Annotation 属于 Viewer 辅助层，不进入 Scene / RenderItem。
    // Marker 使用单位十字并按当前 Camera 视野动态缩放；Line 顶点直接保存明确 World Point。
    if (m_hasMeasurementPointA && m_measurementMarker != 0)
    {
        const float markerScale = camera->projectionType() == CameraProjectionPerspective
            ? qMax(camera->distanceToTarget() * 0.012f, 0.002f)
            : qMax(camera->orthographicHeight() * 0.012f, 0.002f);

        QMatrix4x4 markerModelA;
        markerModelA.translate(m_measurementPointA);
        markerModelA.scale(markerScale);

        if (!m_renderer.drawVertexColorGeometry(m_measurementMarker, markerModelA, false))
        {
            qWarning() << "OpenGLViewerWidget paintGL failed: Measurement Point A drawing failed.";
            m_renderer.endFrame();
            return;
        }

        if (m_hasMeasurementPointB)
        {
            QMatrix4x4 markerModelB;
            markerModelB.translate(m_measurementPointB);
            markerModelB.scale(markerScale);

            if (!m_renderer.drawVertexColorGeometry(m_measurementMarker, markerModelB, false))
            {
                qWarning() << "OpenGLViewerWidget paintGL failed: Measurement Point B drawing failed.";
                m_renderer.endFrame();
                return;
            }

            if (m_measurementLine != 0 && !m_renderer.drawVertexColorGeometry(m_measurementLine, identityModel, false))
            {
                qWarning() << "OpenGLViewerWidget paintGL failed: Measurement Line drawing failed.";
                m_renderer.endFrame();
                return;
            }
        }
    }

    // View Navigation 使用独立的屏幕角落 Viewport，同样不属于用户 Scene。
    if (m_showViewNavigation)
        m_renderer.drawViewNavigation(m_viewNavigation, camera);

    m_renderer.endFrame();

    // 普通 QLabel Child Overlay 不进入当前 OpenGL Paint Engine，只更新文字和 Widget Position。
    updateMeasurementLabels();
}

void OpenGLViewerWidget::releaseViewerGL()
{
    if (m_releasePerformed)
        return;

    m_releasePerformed = true;

    // 不能使用 m_glReady 作为 GPU Cleanup 的前置条件。
    // initializeGL() 可能在部分 Resource 或派生 Shared GPU 状态已经创建后失败。
    if (context() != 0)
    {
        makeCurrent();

        // makeCurrent() 没有返回值，因此显式确认当前 Context 是否真的成为 Current。
        if (QOpenGLContext::currentContext() == context())
        {
            QOpenGLFunctions_3_3_Core* gl = m_renderContext.gl();

            if (gl != 0)
            {
                if (!m_resourceManager.releaseGL(gl))
                    qWarning() << "OpenGLViewerWidget releaseViewerGL: one or more Resources failed to release GPU state.";

                m_renderer.release();
            }

            doneCurrent();
        }
        else
        {
            // 没有有效 Current Context 时不能伪装执行 glDelete*。
            // ResourceManager 析构仍会释放 CPU Resource；GPU Object 最终只能依赖 Context / Share Group 销毁。
            qWarning() << "OpenGLViewerWidget releaseViewerGL: unable to make Renderer OpenGL Context current.";
        }
    }

    // 派生 Shared GPU Storage 必须在 MyOpenGL Resource（尤其 ExternalGpuGeometry VAO）释放之后再销毁。
    afterViewerGLReleased();
    m_glReady = false;
}

/// 输入

void OpenGLViewerWidget::mousePressEvent(QMouseEvent* event)
{
    m_lastMousePosition = event->pos();

    if (event->button() == Qt::LeftButton)
    {
        // Left Button 专用于 Orbit。
        event->accept();
        return;
    }

    if (event->button() == Qt::MiddleButton)
    {
        // Middle Button 专用于 Pan。
        event->accept();
        return;
    }

    if (event->button() == Qt::RightButton)
    {
        m_rightPressPosition = event->pos();
        m_rightDragOccurred = false;

        // Right Button 按当前 Picking Mode 执行 Point / Triangle / Item Click；越过 Drag Threshold 时忽略。
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
        // 每个鼠标像素对应 0.3 度 Orbit，保持原有旋转灵敏度。
        const float orbitDegreesPerPixel = 0.3f;
        m_cameraManager.orbit(-delta.x() * orbitDegreesPerPixel, -delta.y() * orbitDegreesPerPixel);

        viewerStateChanged();
        update();
        event->accept();
        return;
    }

    if (event->buttons() & Qt::MiddleButton)
    {
        Camera* camera = m_cameraManager.activeCamera();

        if (camera == 0)
            return;

        // Pan 随 Camera 距离变化，同时保留最小速度保证近距离仍然容易操作。
        const float distanceScalePerPixel = camera->distanceToTarget() * 0.0015f;
        const float minimumPanScalePerPixel = 0.002f;
        const float panScalePerPixel = qMax(distanceScalePerPixel, minimumPanScalePerPixel);

        m_cameraManager.pan(-delta.x() * panScalePerPixel, delta.y() * panScalePerPixel);

        viewerStateChanged();
        update();
        event->accept();
        return;
    }

    if (event->buttons() & Qt::RightButton)
    {
        if (!m_rightDragOccurred)
        {
            const int dragDistance = (currentPosition - m_rightPressPosition).manhattanLength();

            if (dragDistance >= QApplication::startDragDistance())
                m_rightDragOccurred = true;
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
        event->accept();
        return;
    }

    if (event->button() == Qt::RightButton)
    {
        if (!m_rightDragOccurred)
        {
            if (event->modifiers() & Qt::ShiftModifier)
                handleRightClickPick(event->pos());
            else
                showViewerContextMenu(event->pos());
        }

        m_rightDragOccurred = false;
        event->accept();
        return;
    }

    QOpenGLWidget::mouseReleaseEvent(event);
}

void OpenGLViewerWidget::wheelEvent(QWheelEvent* event)
{
    // Qt 标准鼠标滚轮一格通常对应 120 Angle Delta。
    const float wheelSteps = static_cast<float>(event->angleDelta().y()) / 120.0f;
    const float zoomFactor = static_cast<float>(std::pow(1.15, wheelSteps));

    if (zoomFactor > 0.0f)
        m_cameraManager.zoom(zoomFactor);

    viewerStateChanged();
    update();
    event->accept();
}

void OpenGLViewerWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape)
    {
        // Esc 是 Viewer 交互组合：清除最近 Pick / Highlight，并结束当前 Measurement。
        clearPickedItem();
        clearBoundsHighlight();
        clearPrimitiveHighlight();
        clearMeasurement();
        viewerStateChanged();
        update();
        return;
    }

    if (event->key() == Qt::Key_Q)
    {
        cyclePickMode();
        return;
    }

    if (event->key() == Qt::Key_R)
    {
        resetCamera();
        viewerStateChanged();
        update();
        return;
    }

    if (event->key() == Qt::Key_H)
    {
        if (fitSceneToView())
        {
            viewerStateChanged();
            update();
        }

        return;
    }

    if (event->key() == Qt::Key_1)
    {
        // 标准方向只操作当前 View Bounds；是否观察整个 Scene、某个 Item 或其他 Bounds 由之前的显式操作决定。
        if (m_cameraManager.viewFront() && m_cameraManager.fitViewBounds(width(), height()))
        {
            logCameraView("Front");
            viewerStateChanged();
            update();
        }

        return;
    }

    if (event->key() == Qt::Key_2)
    {
        if (m_cameraManager.viewBack() && m_cameraManager.fitViewBounds(width(), height()))
        {
            logCameraView("Back");
            viewerStateChanged();
            update();
        }

        return;
    }

    if (event->key() == Qt::Key_3)
    {
        if (m_cameraManager.viewLeft() && m_cameraManager.fitViewBounds(width(), height()))
        {
            logCameraView("Left");
            viewerStateChanged();
            update();
        }

        return;
    }

    if (event->key() == Qt::Key_4)
    {
        if (m_cameraManager.viewRight() && m_cameraManager.fitViewBounds(width(), height()))
        {
            logCameraView("Right");
            viewerStateChanged();
            update();
        }

        return;
    }

    if (event->key() == Qt::Key_5)
    {
        if (m_cameraManager.viewTop() && m_cameraManager.fitViewBounds(width(), height()))
        {
            logCameraView("Top");
            viewerStateChanged();
            update();
        }

        return;
    }

    if (event->key() == Qt::Key_6)
    {
        if (m_cameraManager.viewBottom() && m_cameraManager.fitViewBounds(width(), height()))
        {
            logCameraView("Bottom");
            viewerStateChanged();
            update();
        }

        return;
    }

    if (event->key() == Qt::Key_7)
    {
        if (m_cameraManager.viewIsometric() && m_cameraManager.fitViewBounds(width(), height()))
        {
            logCameraView("Isometric");
            viewerStateChanged();
            update();
        }

        return;
    }

    if (event->key() == Qt::Key_F)
    {
        const RenderItem* currentPickedItem = pickedItem();

        if (currentPickedItem == 0)
        {
            // 没有当前 Pick 是正常交互状态，不作为程序错误输出 Warning。
            // qDebug() << "OpenGLViewerWidget Fit Picked Item ignored: no item is currently picked.";
            return;
        }

        if (fitItemToView(currentPickedItem))
        {
            const AxisAlignedBoundingBox bounds = currentPickedItem->worldBounds();
            const Camera* camera = m_cameraManager.activeCamera();

            // F 只属于 Viewer 快捷键交互语义；基础 fitItemToView() 只接收明确 Item。
            // qDebug() << "OpenGLViewerWidget Fit Picked Item:"
            //          << "Item=" << currentPickedItem->name()
            //          << "Minimum=" << bounds.minimum()
            //          << "Maximum=" << bounds.maximum()
            //          << "Center=" << bounds.center()
            //          << "CameraDistance=" << (camera != 0 ? camera->distanceToTarget() : 0.0f);

            viewerStateChanged();
            update();
        }

        return;
    }

    if (event->key() == Qt::Key_O)
    {
        // O 只平移当前 View Bounds，使其 Center 回到世界原点；Bounds 尺寸和观察方向保持不变。
        if (focusPoint(QVector3D(0.0f, 0.0f, 0.0f)))
        {
            viewerStateChanged();
            update();
        }

        return;
    }

    if (event->key() == Qt::Key_C)
    {
        setCameraTargetVisible(!m_showCameraTarget);
        return;
    }

    if (event->key() == Qt::Key_G)
    {
        setGridVisible(!m_showGrid);
        return;
    }

    if (event->key() == Qt::Key_A)
    {
        setAxesVisible(!m_showAxes);
        return;
    }

    if (event->key() == Qt::Key_V)
    {
        setViewNavigationVisible(!m_showViewNavigation);
        return;
    }

    if (handleViewerKeyPress(event))
        return;

    QOpenGLWidget::keyPressEvent(event);
}

bool OpenGLViewerWidget::setPickedItem(RenderItem* item)
{
    if (item == 0)
    {
        qWarning() << "OpenGLViewerWidget setPickedItem failed: item is null; use clearPickedItem() to clear current Pick.";
        return false;
    }

    bool belongsToScene = false;

    for (int i = 0; i < m_scene.itemCount(); ++i)
    {
        if (m_scene.item(i) == item)
        {
            belongsToScene = true;
            break;
        }
    }

    if (!belongsToScene)
    {
        qWarning() << "OpenGLViewerWidget setPickedItem failed: item does not belong to current Scene.";
        return false;
    }

    m_pickedItem = item;
    return true;
}

/// Viewer 创建

void OpenGLViewerWidget::buildViewerResources()
{
    /// Grid

    m_grid = new GridPlaneGeometry("WorldGrid");
    m_grid->setGrid(10.0f, 1.0f);
    m_resourceManager.add(m_grid);

    /// Coordinate System

    m_axes = new CoordinateSystemGeometry("WorldAxes");
    m_axes->setAxisLength(3.0f);
    m_resourceManager.add(m_axes);

    /// View Navigation

    m_viewNavigation = new ViewNavigationGeometry("ViewNavigation");
    m_viewNavigation->setAxisLength(1.0f);
    m_resourceManager.add(m_viewNavigation);

    /// Camera Target Marker

    m_cameraTargetMarker = createCameraTargetMarker();
    m_resourceManager.add(m_cameraTargetMarker);

    /// Highlight Overlays

    m_boundsHighlight = createBoundsHighlightGeometry();
    m_resourceManager.add(m_boundsHighlight);

    m_primitiveHighlight = createPrimitiveHighlightGeometry();
    m_resourceManager.add(m_primitiveHighlight);

    /// Measurement Annotation

    m_measurementMarker = createMeasurementMarkerGeometry();
    m_resourceManager.add(m_measurementMarker);

    m_measurementLine = createMeasurementLineGeometry();
    m_resourceManager.add(m_measurementLine);

    /// Camera

    Camera* camera = new Camera("MainCamera");
    camera->setPerspective(45.0f, 0.1f, 1000.0f);
    m_cameraManager.add(camera);
}

void OpenGLViewerWidget::rebuildViewerSceneItems()
{
    // Candidate / Pick State 都只借用用户 Scene Item；Scene 重建前先清除，避免保留失效指针。
    m_pickCandidates.clear();
    m_pickedItem = 0;
    m_showBoundsHighlight = false;
    m_showPrimitiveHighlight = false;
    m_hasMeasurementPointA = false;
    m_hasMeasurementPointB = false;

    // Scene 只拥有用户可操作 RenderItem。Grid / Axis / Camera Target / Highlight / ViewNavigation
    // 都是 Viewer 内部模型，不进入 Scene，也不占用 Item 操作语义。
    m_scene.clear();

    buildViewerContentItems();

    // qDebug() << "OpenGLViewerWidget User Scene built:"
    //          << "Items=" << m_scene.itemCount();
}

BufferGeometry* OpenGLViewerWidget::createCameraTargetMarker()
{
    BufferGeometry* marker = new BufferGeometry("CameraTargetMarker", ResourceUpdateStatic, Lines);

    std::vector<GeometryVertexAttribute> attributes;

    GeometryVertexAttribute positionAttribute;
    positionAttribute.location = 0;
    positionAttribute.componentCount = 3;
    positionAttribute.valueOffset = 0;
    attributes.push_back(positionAttribute);

    GeometryVertexAttribute colorAttribute;
    colorAttribute.location = 1;
    colorAttribute.componentCount = 3;
    colorAttribute.valueOffset = 3;
    attributes.push_back(colorAttribute);

    // Target Marker 使用 position.xyz + color.rgb，共 6 个 GLfloat。
    marker->setVertexLayout(6, attributes);

    // 单位十字分别沿 X/Y/Z 从 -1 到 +1；实际屏幕尺寸由 Camera Distance 动态缩放。
    const GLfloat vertices[] =
    {
        -1.0f,  0.0f,  0.0f,    1.0f, 1.0f, 0.0f,
         1.0f,  0.0f,  0.0f,    1.0f, 1.0f, 0.0f,

         0.0f, -1.0f,  0.0f,    1.0f, 1.0f, 0.0f,
         0.0f,  1.0f,  0.0f,    1.0f, 1.0f, 0.0f,

         0.0f,  0.0f, -1.0f,    1.0f, 1.0f, 0.0f,
         0.0f,  0.0f,  1.0f,    1.0f, 1.0f, 0.0f
    };

    const GLuint indices[] =
    {
        0, 1,
        2, 3,
        4, 5
    };

    const int vertexValueCount = sizeof(vertices) / sizeof(vertices[0]);
    const int indexCount = sizeof(indices) / sizeof(indices[0]);

    marker->setVertexData(std::vector<GLfloat>(vertices, vertices + vertexValueCount));
    marker->setIndexData(std::vector<GLuint>(indices, indices + indexCount));

    return marker;
}

BufferGeometry* OpenGLViewerWidget::createBoundsHighlightGeometry()
{
    BufferGeometry* boundsGeometry = new BufferGeometry("BoundsHighlight", ResourceUpdateDynamic, Lines);

    std::vector<GeometryVertexAttribute> attributes;

    GeometryVertexAttribute positionAttribute;
    positionAttribute.location = 0;
    positionAttribute.componentCount = 3;
    positionAttribute.valueOffset = 0;
    attributes.push_back(positionAttribute);

    GeometryVertexAttribute colorAttribute;
    colorAttribute.location = 1;
    colorAttribute.componentCount = 3;
    colorAttribute.valueOffset = 3;
    attributes.push_back(colorAttribute);

    boundsGeometry->setVertexLayout(6, attributes);

    // Resource 初始化阶段必须已经具有合法 Geometry 数据；初始单位盒不绘制，真正 Highlight 时再替换为明确 World AABB。
    const GLfloat vertices[] =
    {
        -0.5f, -0.5f, -0.5f,    1.0f, 1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,    1.0f, 1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,    1.0f, 1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,    1.0f, 1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,    1.0f, 1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,    1.0f, 1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,    1.0f, 1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,    1.0f, 1.0f, 0.0f
    };

    const GLuint indices[] =
    {
        0, 1,  1, 2,  2, 3,  3, 0,
        4, 5,  5, 6,  6, 7,  7, 4,
        0, 4,  1, 5,  2, 6,  3, 7
    };

    const int vertexValueCount = sizeof(vertices) / sizeof(vertices[0]);
    const int indexCount = sizeof(indices) / sizeof(indices[0]);

    boundsGeometry->setVertexData(std::vector<GLfloat>(vertices, vertices + vertexValueCount));
    boundsGeometry->setIndexData(std::vector<GLuint>(indices, indices + indexCount));
    return boundsGeometry;
}

BufferGeometry* OpenGLViewerWidget::createPrimitiveHighlightGeometry()
{
    BufferGeometry* primitiveGeometry = new BufferGeometry("PrimitiveHighlight", ResourceUpdateDynamic, Lines);

    std::vector<GeometryVertexAttribute> attributes;

    GeometryVertexAttribute positionAttribute;
    positionAttribute.location = 0;
    positionAttribute.componentCount = 3;
    positionAttribute.valueOffset = 0;
    attributes.push_back(positionAttribute);

    GeometryVertexAttribute colorAttribute;
    colorAttribute.location = 1;
    colorAttribute.componentCount = 3;
    colorAttribute.valueOffset = 3;
    attributes.push_back(colorAttribute);

    primitiveGeometry->setVertexLayout(6, attributes);

    // Resource 初始化需要合法数据；初始单位 Triangle 不绘制。
    // 同一动态 Geometry 同时承载 Triangle / Line Primitive Highlight；橙色与黄色 World AABB 区分。
    const GLfloat vertices[] =
    {
        0.0f, 0.0f, 0.0f,    1.0f, 0.35f, 0.0f,
        1.0f, 0.0f, 0.0f,    1.0f, 0.35f, 0.0f,
        0.0f, 1.0f, 0.0f,    1.0f, 0.35f, 0.0f
    };

    const GLuint indices[] =
    {
        0, 1,
        1, 2,
        2, 0
    };

    const int vertexValueCount = sizeof(vertices) / sizeof(vertices[0]);
    const int indexCount = sizeof(indices) / sizeof(indices[0]);

    primitiveGeometry->setVertexData(std::vector<GLfloat>(vertices, vertices + vertexValueCount));
    primitiveGeometry->setIndexData(std::vector<GLuint>(indices, indices + indexCount));
    return primitiveGeometry;
}

BufferGeometry* OpenGLViewerWidget::createMeasurementMarkerGeometry()
{
    BufferGeometry* marker = new BufferGeometry("MeasurementMarker", ResourceUpdateStatic, Lines);

    std::vector<GeometryVertexAttribute> attributes;

    GeometryVertexAttribute positionAttribute;
    positionAttribute.location = 0;
    positionAttribute.componentCount = 3;
    positionAttribute.valueOffset = 0;
    attributes.push_back(positionAttribute);

    GeometryVertexAttribute colorAttribute;
    colorAttribute.location = 1;
    colorAttribute.componentCount = 3;
    colorAttribute.valueOffset = 3;
    attributes.push_back(colorAttribute);

    marker->setVertexLayout(6, attributes);

    // 单位十字沿 X/Y/Z 三轴显示；实际 World 尺寸由当前 Camera 视野动态缩放。
    const GLfloat vertices[] =
    {
        -1.0f,  0.0f,  0.0f,    0.15f, 1.0f, 0.85f,
         1.0f,  0.0f,  0.0f,    0.15f, 1.0f, 0.85f,

         0.0f, -1.0f,  0.0f,    0.15f, 1.0f, 0.85f,
         0.0f,  1.0f,  0.0f,    0.15f, 1.0f, 0.85f,

         0.0f,  0.0f, -1.0f,    0.15f, 1.0f, 0.85f,
         0.0f,  0.0f,  1.0f,    0.15f, 1.0f, 0.85f
    };

    const GLuint indices[] =
    {
        0, 1,
        2, 3,
        4, 5
    };

    const int vertexValueCount = sizeof(vertices) / sizeof(vertices[0]);
    const int indexCount = sizeof(indices) / sizeof(indices[0]);

    marker->setVertexData(std::vector<GLfloat>(vertices, vertices + vertexValueCount));
    marker->setIndexData(std::vector<GLuint>(indices, indices + indexCount));
    return marker;
}

BufferGeometry* OpenGLViewerWidget::createMeasurementLineGeometry()
{
    BufferGeometry* line = new BufferGeometry("MeasurementLine", ResourceUpdateDynamic, Lines);

    std::vector<GeometryVertexAttribute> attributes;

    GeometryVertexAttribute positionAttribute;
    positionAttribute.location = 0;
    positionAttribute.componentCount = 3;
    positionAttribute.valueOffset = 0;
    attributes.push_back(positionAttribute);

    GeometryVertexAttribute colorAttribute;
    colorAttribute.location = 1;
    colorAttribute.componentCount = 3;
    colorAttribute.valueOffset = 3;
    attributes.push_back(colorAttribute);

    line->setVertexLayout(6, attributes);

    // Resource 初始化需要合法数据；真正完成 A-B 测量后会替换为明确 World Segment。
    const GLfloat vertices[] =
    {
        0.0f, 0.0f, 0.0f,    0.15f, 1.0f, 0.85f,
        1.0f, 0.0f, 0.0f,    0.15f, 1.0f, 0.85f
    };

    const GLuint indices[] =
    {
        0, 1
    };

    const int vertexValueCount = sizeof(vertices) / sizeof(vertices[0]);
    const int indexCount = sizeof(indices) / sizeof(indices[0]);

    line->setVertexData(std::vector<GLfloat>(vertices, vertices + vertexValueCount));
    line->setIndexData(std::vector<GLuint>(indices, indices + indexCount));
    return line;
}

/// Picking / 当前交互状态

bool OpenGLViewerWidget::pickAt(const QPoint& position, QVector3D* hitPosition)
{
    const Camera* camera = m_cameraManager.activeCamera();

    if (camera == 0)
    {
        qWarning() << "OpenGLViewerWidget pickAt failed: active camera does not exist.";
        return false;
    }

    QVector3D rayOrigin;
    QVector3D rayDirection;

    if (!camera->screenPointToRay(position.x(), position.y(), width(), height(), rayOrigin, rayDirection))
        return false;

    // Viewer 明确决定本次查询的 Candidate 和 Picking 策略：
    // 有 Primitive Picker 的 Candidate 只做精确 Primitive Picking；
    // 没有 Primitive Picker 的 Candidate 才进入 Bounds Fallback。
    RenderItemCandidates primitiveCandidates;
    RenderItemCandidates boundsCandidates;

    for (std::size_t i = 0; i < m_pickCandidates.size(); ++i)
    {
        RenderItem* candidate = m_pickCandidates[i];

        if (candidate == 0)
            continue;

        if (candidate->primitivePickSource() != 0)
            primitiveCandidates.push_back(candidate);
        else
            boundsCandidates.push_back(candidate);
    }

    ScenePrimitivePickQuery primitiveQuery;
    primitiveQuery.rayOrigin = rayOrigin;
    primitiveQuery.rayDirection = rayDirection;
    primitiveQuery.screenPosition = QPointF(position);
    primitiveQuery.viewportWidth = width();
    primitiveQuery.viewportHeight = height();
    primitiveQuery.pixelTolerance = m_primitivePickTolerance;

    const float aspect = height() > 0 ? static_cast<float>(width()) / static_cast<float>(height()) : 1.0f;
    primitiveQuery.viewProjection = camera->projectionMatrix(aspect) * camera->viewMatrix();

    ScenePrimitiveHit primitiveHit;

    if (m_scene.pickPrimitive(primitiveCandidates, primitiveQuery, primitiveHit, true))
    {
        if (!setPickedItem(primitiveHit.item))
            return false;

        const AxisAlignedBoundingBox bounds = primitiveHit.item->worldBounds();
        showBoundsHighlight(bounds);

        if (primitiveHit.type == PrimitivePickTriangle && primitiveHit.vertexCount == 3)
            showTriangleHighlight(primitiveHit.vertices[0], primitiveHit.vertices[1], primitiveHit.vertices[2]);
        else if (primitiveHit.type == PrimitivePickLine && primitiveHit.vertexCount == 2)
            showLineHighlight(primitiveHit.vertices[0], primitiveHit.vertices[1]);
        else
            clearPrimitiveHighlight();

        // qDebug() << "OpenGLViewerWidget Primitive Pick changed:"
        //          << "Item=" << primitiveHit.item->name()
        //          << "Type=" << primitivePickTypeName(primitiveHit.type)
        //          << "PrimitiveIndex=" << primitiveHit.primitiveIndex
        //          << "HitDistance=" << primitiveHit.distance
        //          << "HitPosition=" << primitiveHit.position
        //          << "Barycentric=" << primitiveHit.barycentric
        //          << "BoundsMinimum=" << bounds.minimum()
        //          << "BoundsMaximum=" << bounds.maximum();

        if (hitPosition != 0)
            *hitPosition = primitiveHit.position;

        viewerStateChanged();
        update();
        return true;
    }

    SceneRayHit objectHit;

    if (m_scene.raycast(boundsCandidates, rayOrigin, rayDirection, objectHit, true))
    {
        if (!setPickedItem(objectHit.item))
            return false;

        const AxisAlignedBoundingBox bounds = objectHit.item->worldBounds();
        showBoundsHighlight(bounds);
        clearPrimitiveHighlight();

        // qDebug() << "OpenGLViewerWidget Object Bounds Pick fallback:"
        //          << "Item=" << objectHit.item->name()
        //          << "HitDistance=" << objectHit.distance
        //          << "HitPosition=" << objectHit.position
        //          << "BoundsMinimum=" << bounds.minimum()
        //          << "BoundsMaximum=" << bounds.maximum();

        if (hitPosition != 0)
            *hitPosition = objectHit.position;

        viewerStateChanged();
        update();
        return true;
    }

    // Pick Miss 的交互策略由 Viewer 组合：状态和视觉分别显式清除。
    clearPickedItem();
    clearBoundsHighlight();
    clearPrimitiveHighlight();
    viewerStateChanged();
    update();
    return false;
}

bool OpenGLViewerWidget::pickPointAt(const QPoint& position, ScenePointHit& hit)
{
    hit = ScenePointHit();

    const Camera* camera = m_cameraManager.activeCamera();

    if (camera == 0)
    {
        qWarning() << "OpenGLViewerWidget pickPointAt failed: active camera does not exist.";
        return false;
    }

    QVector3D rayOrigin;
    QVector3D rayDirection;

    if (!camera->screenPointToRay(position.x(), position.y(), width(), height(), rayOrigin, rayDirection))
        return false;

    ScenePrimitivePickQuery query;
    query.rayOrigin = rayOrigin;
    query.rayDirection = rayDirection;
    query.screenPosition = QPointF(position);
    query.viewportWidth = width();
    query.viewportHeight = height();
    query.pixelTolerance = m_pointPickTolerance;

    const float aspect = height() > 0 ? static_cast<float>(width()) / static_cast<float>(height()) : 1.0f;
    query.viewProjection = camera->projectionMatrix(aspect) * camera->viewMatrix();

    if (!m_scene.pickPoint(m_pickCandidates, query, hit, true))
        return false;

    // if (!setPickedItem(hit.item))
    //     return false;

    // const AxisAlignedBoundingBox bounds = hit.item->worldBounds();
    // showBoundsHighlight(bounds);

    // Measurement Point Marker 会在 setMeasurementPointA/B() 后显示精确 Snap 位置；
    // 这里不复用 Triangle/Line Primitive Highlight，避免把 Vertex Snap 伪装成其他 Primitive。
    clearPrimitiveHighlight();

    // qDebug() << "OpenGLViewerWidget Point Pick changed:"
    //          << "Item=" << hit.item->name()
    //          << "PartId=" << static_cast<qulonglong>(hit.partId)
    //          << "VertexIndex=" << hit.vertexIndex
    //          << "ScreenDistance=" << hit.screenDistance
    //          << "HitDistance=" << hit.distance
    //          << "HitPosition=" << hit.position
    //          << "BoundsMinimum=" << bounds.minimum()
    //          << "BoundsMaximum=" << bounds.maximum();

    viewerStateChanged();
    update();
    return true;
}

bool OpenGLViewerWidget::pickTriangleAt(const QPoint& position, ScenePrimitiveHit& hit)
{
    hit = ScenePrimitiveHit();

    const Camera* camera = m_cameraManager.activeCamera();

    if (camera == 0)
    {
        qWarning() << "OpenGLViewerWidget pickTriangleAt failed: active camera does not exist.";
        return false;
    }

    QVector3D rayOrigin;
    QVector3D rayDirection;

    if (!camera->screenPointToRay(position.x(), position.y(), width(), height(), rayOrigin, rayDirection))
        return false;

    // Triangle Mode 不再依赖 Item 只有一个 Geometry 的旧假设。
    // ScenePrimitivePickQuery 会在 Item 的全部 RenderPart 中只接受 Triangle Primitive。
    const RenderItemCandidates& triangleCandidates = m_pickCandidates;

    ScenePrimitivePickQuery query;
    query.rayOrigin = rayOrigin;
    query.rayDirection = rayDirection;
    query.screenPosition = QPointF(position);
    query.viewportWidth = width();
    query.viewportHeight = height();
    query.pixelTolerance = m_primitivePickTolerance;
    query.filterPrimitiveType = true;
    query.requiredPrimitiveType = PrimitivePickTriangle;

    const float aspect = height() > 0 ? static_cast<float>(width()) / static_cast<float>(height()) : 1.0f;
    query.viewProjection = camera->projectionMatrix(aspect) * camera->viewMatrix();

    if (!m_scene.pickPrimitive(triangleCandidates, query, hit, true))
        return false;

    if (hit.type != PrimitivePickTriangle || hit.vertexCount != 3)
        return false;

    if (!setPickedItem(hit.item))
        return false;

    const AxisAlignedBoundingBox bounds = hit.item->worldBounds();
    showBoundsHighlight(bounds);
    showTriangleHighlight(hit.vertices[0], hit.vertices[1], hit.vertices[2]);

    // qDebug() << "OpenGLViewerWidget Triangle Pick changed:"
    //          << "Item=" << hit.item->name()
    //          << "PartId=" << static_cast<qulonglong>(hit.partId)
    //          << "PrimitiveIndex=" << hit.primitiveIndex
    //          << "HitDistance=" << hit.distance
    //          << "HitPosition=" << hit.position
    //          << "Barycentric=" << hit.barycentric
    //          << "BoundsMinimum=" << bounds.minimum()
    //          << "BoundsMaximum=" << bounds.maximum();

    viewerStateChanged();
    update();
    return true;
}

bool OpenGLViewerWidget::pickItemAt(const QPoint& position, SceneRayHit& hit)
{
    hit = SceneRayHit();

    const Camera* camera = m_cameraManager.activeCamera();

    if (camera == 0)
    {
        qWarning() << "OpenGLViewerWidget pickItemAt failed: active camera does not exist.";
        return false;
    }

    QVector3D rayOrigin;
    QVector3D rayDirection;

    if (!camera->screenPointToRay(position.x(), position.y(), width(), height(), rayOrigin, rayDirection))
        return false;

    // Item Mode 明确查询所有 Picking Candidate 的 World Bounds，
    // 不区分 Geometry 是否具有 PrimitivePickSource。
    if (!m_scene.raycast(m_pickCandidates, rayOrigin, rayDirection, hit, true))
        return false;

    if (!setPickedItem(hit.item))
        return false;

    const AxisAlignedBoundingBox bounds = hit.item->worldBounds();
    showBoundsHighlight(bounds);
    clearPrimitiveHighlight();

    // qDebug() << "OpenGLViewerWidget Item Pick changed:"
    //          << "Item=" << hit.item->name()
    //          << "HitDistance=" << hit.distance
    //          << "HitPosition=" << hit.position
    //          << "BoundsMinimum=" << bounds.minimum()
    //          << "BoundsMaximum=" << bounds.maximum();

    viewerStateChanged();
    update();
    return true;
}

void OpenGLViewerWidget::showViewerContextMenu(const QPoint& position)
{
    QMenu menu(this);

    /// Camera

    QAction* fitAllAction = menu.addAction(tr("Fit All"));
    connect(fitAllAction, &QAction::triggered, this, [this]()
    {
        if (fitSceneToView())
        {
            viewerStateChanged();
            update();
        }
    });

    QAction* fitPickedAction = menu.addAction(tr("Fit Picked"));
    fitPickedAction->setEnabled(m_pickedItem != 0);
    connect(fitPickedAction, &QAction::triggered, this, [this]()
    {
        if (m_pickedItem != 0 && fitItemToView(m_pickedItem))
        {
            viewerStateChanged();
            update();
        }
    });

    QMenu* viewMenu = menu.addMenu(tr("View"));

    QAction* isometricAction = viewMenu->addAction(tr("Isometric"));
    connect(isometricAction, &QAction::triggered, this, [this]()
    {
        if (m_cameraManager.viewIsometric() && m_cameraManager.fitViewBounds(width(), height()))
        {
            viewerStateChanged();
            update();
        }
    });

    QAction* frontAction = viewMenu->addAction(tr("Front"));
    connect(frontAction, &QAction::triggered, this, [this]()
    {
        if (m_cameraManager.viewFront() && m_cameraManager.fitViewBounds(width(), height()))
        {
            viewerStateChanged();
            update();
        }
    });

    QAction* backAction = viewMenu->addAction(tr("Back"));
    connect(backAction, &QAction::triggered, this, [this]()
    {
        if (m_cameraManager.viewBack() && m_cameraManager.fitViewBounds(width(), height()))
        {
            viewerStateChanged();
            update();
        }
    });

    QAction* leftAction = viewMenu->addAction(tr("Left"));
    connect(leftAction, &QAction::triggered, this, [this]()
    {
        if (m_cameraManager.viewLeft() && m_cameraManager.fitViewBounds(width(), height()))
        {
            viewerStateChanged();
            update();
        }
    });

    QAction* rightAction = viewMenu->addAction(tr("Right"));
    connect(rightAction, &QAction::triggered, this, [this]()
    {
        if (m_cameraManager.viewRight() && m_cameraManager.fitViewBounds(width(), height()))
        {
            viewerStateChanged();
            update();
        }
    });

    QAction* topAction = viewMenu->addAction(tr("Top"));
    connect(topAction, &QAction::triggered, this, [this]()
    {
        if (m_cameraManager.viewTop() && m_cameraManager.fitViewBounds(width(), height()))
        {
            viewerStateChanged();
            update();
        }
    });

    QAction* bottomAction = viewMenu->addAction(tr("Bottom"));
    connect(bottomAction, &QAction::triggered, this, [this]()
    {
        if (m_cameraManager.viewBottom() && m_cameraManager.fitViewBounds(width(), height()))
        {
            viewerStateChanged();
            update();
        }
    });

    /// Viewer Display

    QMenu* displayMenu = menu.addMenu(tr("Display"));

    // QAction* gridAction = displayMenu->addAction(tr("Grid"));
    // gridAction->setCheckable(true);
    // gridAction->setChecked(m_showGrid);
    // connect(gridAction, &QAction::toggled, this, [this](bool checked)
    // {
    //     setGridVisible(checked);
    // });

    QAction* axesAction = displayMenu->addAction(tr("Axes"));
    axesAction->setCheckable(true);
    axesAction->setChecked(m_showAxes);
    connect(axesAction, &QAction::toggled, this, [this](bool checked)
    {
        setAxesVisible(checked);
    });

    QAction* navigationAction = displayMenu->addAction(tr("View Navigation"));
    navigationAction->setCheckable(true);
    navigationAction->setChecked(m_showViewNavigation);
    connect(navigationAction, &QAction::toggled, this, [this](bool checked)
    {
        setViewNavigationVisible(checked);
    });

    QAction* targetAction = displayMenu->addAction(tr("Camera Target"));
    targetAction->setCheckable(true);
    targetAction->setChecked(m_showCameraTarget);
    connect(targetAction, &QAction::toggled, this, [this](bool checked)
    {
        setCameraTargetVisible(checked);
    });

    /// Picking

    QMenu* pickingMenu = menu.addMenu(tr("Picking"));

    // QAction* pickHereAction = pickingMenu->addAction(QString("Pick Here (%1)").arg(viewerPickModeName(m_pickMode)));
    // connect(pickHereAction, &QAction::triggered, this, [this, position]()
    // {
    //     handleRightClickPick(position);
    // });

    pickingMenu->addSeparator();

    QAction* pointModeAction = pickingMenu->addAction(tr("Point / Measure"));
    pointModeAction->setCheckable(true);
    pointModeAction->setChecked(m_pickMode == ViewerPickModePoint);
    connect(pointModeAction, &QAction::triggered, this, [this]()
    {
        setPickMode(ViewerPickModePoint);
    });

    // QAction* triangleModeAction = pickingMenu->addAction(tr("Triangle"));
    // triangleModeAction->setCheckable(true);
    // triangleModeAction->setChecked(m_pickMode == ViewerPickModeTriangle);
    // connect(triangleModeAction, &QAction::triggered, this, [this]()
    // {
    //     setPickMode(ViewerPickModeTriangle);
    // });

    QAction* itemModeAction = pickingMenu->addAction(tr("Item"));
    itemModeAction->setCheckable(true);
    itemModeAction->setChecked(m_pickMode == ViewerPickModeItem);
    connect(itemModeAction, &QAction::triggered, this, [this]()
    {
        setPickMode(ViewerPickModeItem);
    });

    /// Item Visibility

    menu.addSeparator();

    // QAction* hidePickedAction = menu.addAction(tr("Hide Picked"));
    // hidePickedAction->setEnabled(m_pickedItem != 0);
    // connect(hidePickedAction, &QAction::triggered, this, [this]()
    // {
    //     if (m_pickedItem != 0)
    //         setItemVisible(m_pickedItem, false);
    // });

    // QAction* isolatePickedAction = menu.addAction(tr("Isolate Picked"));
    // isolatePickedAction->setEnabled(m_pickedItem != 0);
    // connect(isolatePickedAction, &QAction::triggered, this, [this]()
    // {
    //     if (m_pickedItem == 0)
    //         return;

    //     std::vector<RenderItem*> items;
    //     items.push_back(m_pickedItem);
    //     isolateItems(items);
    // });

    // QAction* showAllAction = menu.addAction(tr("Show All"));
    // connect(showAllAction, &QAction::triggered, this, [this]()
    // {
    //     setAllItemsVisible(true);
    // });

    /// Clear

    menu.addSeparator();

    QAction* clearPickAction = menu.addAction(tr("Clear Pick"));
    clearPickAction->setEnabled(m_pickedItem != 0 || m_showBoundsHighlight || m_showPrimitiveHighlight);
    connect(clearPickAction, &QAction::triggered, this, [this]()
    {
        clearPickedItem();
        clearMeasurement();
        clearBoundsHighlight();
        clearPrimitiveHighlight();
        viewerStateChanged();
        update();
    });

    // QAction* clearMeasurementAction = menu.addAction(tr("Clear Measurement"));
    // clearMeasurementAction->setEnabled(m_hasMeasurementPointA || m_hasMeasurementPointB);
    // connect(clearMeasurementAction, &QAction::triggered, this, [this]()
    // {
    //     clearMeasurement();
    // });

    /// Derived Viewer

    const int baseActionCount = menu.actions().size();
    populateViewerContextMenu(&menu, position);

    if (menu.actions().size() > baseActionCount)
    {
        QAction* firstDerivedAction = menu.actions().at(baseActionCount);

        if (firstDerivedAction != 0 && !firstDerivedAction->isSeparator())
            menu.insertSeparator(firstDerivedAction);
    }

    menu.exec(mapToGlobal(position));
}

bool OpenGLViewerWidget::handleRightClickPick(const QPoint& position)
{
    bool hit = false;

    switch (m_pickMode)
    {
    case ViewerPickModePoint:
        hit = measureAt(position);
        break;

    case ViewerPickModeTriangle:
    {
        ScenePrimitiveHit triangleHit;
        hit = pickTriangleAt(position, triangleHit);
        break;
    }

    case ViewerPickModeItem:
    {
        SceneRayHit itemHit;
        hit = pickItemAt(position, itemHit);
        break;
    }
    }

    if (hit)
        return true;

    // Pick Miss 只清理当前 Pick/Highlight；已经形成的 Measurement World Geometry 保留。
    clearPickedItem();
    clearBoundsHighlight();
    clearPrimitiveHighlight();
    viewerStateChanged();
    update();
    return false;
}

bool OpenGLViewerWidget::measureAt(const QPoint& position)
{
    ScenePointHit pointHit;

    // Point Mode 只接受精确 Geometry Vertex / Endpoint Snap。
    // Triangle Surface、Line Segment 中间位置和 Item Bounds 不再作为 Point Mode 的隐式回退。
    if (!pickPointAt(position, pointHit))
        return false;

    if (!m_hasMeasurementPointA || m_hasMeasurementPointB)
    {
        setMeasurementPointA(pointHit.position);
        return true;
    }

    return setMeasurementPointB(pointHit.position);
}

bool OpenGLViewerWidget::projectWorldPointToScreen(const QVector3D& worldPoint, QPointF& screenPoint) const
{
    const Camera* camera = m_cameraManager.activeCamera();

    if (camera == 0 || width() <= 0 || height() <= 0)
        return false;

    const float aspect = static_cast<float>(width()) / static_cast<float>(height());
    const QVector4D clipPosition = camera->projectionMatrix(aspect) * camera->viewMatrix() * QVector4D(worldPoint, 1.0f);
    const float homogeneousEpsilon = 1.0e-7f;

    if (clipPosition.w() <= homogeneousEpsilon)
        return false;

    const float inverseW = 1.0f / clipPosition.w();
    const float ndcX = clipPosition.x() * inverseW;
    const float ndcY = clipPosition.y() * inverseW;
    const float ndcZ = clipPosition.z() * inverseW;

    if (ndcZ < -1.0f || ndcZ > 1.0f)
        return false;

    screenPoint.setX((ndcX * 0.5f + 0.5f) * static_cast<float>(width()));
    screenPoint.setY((1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(height()));
    return true;
}

void OpenGLViewerWidget::updateMeasurementLabels()
{
    if (m_measurementPointALabel == 0 || m_measurementPointBLabel == 0 || m_measurementLengthLabel == 0)
        return;

    if (!m_hasMeasurementPointA)
    {
        m_measurementPointALabel->hide();
        m_measurementPointBLabel->hide();
        m_measurementLengthLabel->hide();
        return;
    }

    QPointF pointAScreen;

    if (projectWorldPointToScreen(m_measurementPointA, pointAScreen))
    {
        m_measurementPointALabel->setText(QString("A (%1, %2, %3)")
            .arg(m_measurementPointA.x(), 0, 'f', 3)
            .arg(m_measurementPointA.y(), 0, 'f', 3)
            .arg(m_measurementPointA.z(), 0, 'f', 3));

        m_measurementPointALabel->adjustSize();
        m_measurementPointALabel->move(qRound(pointAScreen.x()) + 10, qRound(pointAScreen.y()) - m_measurementPointALabel->height() - 8);
        m_measurementPointALabel->show();
        m_measurementPointALabel->raise();
    }
    else
    {
        m_measurementPointALabel->hide();
    }

    if (!m_hasMeasurementPointB)
    {
        m_measurementPointBLabel->hide();
        m_measurementLengthLabel->hide();
        return;
    }

    QPointF pointBScreen;

    if (projectWorldPointToScreen(m_measurementPointB, pointBScreen))
    {
        m_measurementPointBLabel->setText(QString("B (%1, %2, %3)")
            .arg(m_measurementPointB.x(), 0, 'f', 3)
            .arg(m_measurementPointB.y(), 0, 'f', 3)
            .arg(m_measurementPointB.z(), 0, 'f', 3));

        m_measurementPointBLabel->adjustSize();
        m_measurementPointBLabel->move(qRound(pointBScreen.x()) + 10, qRound(pointBScreen.y()) - m_measurementPointBLabel->height() - 8);
        m_measurementPointBLabel->show();
        m_measurementPointBLabel->raise();
    }
    else
    {
        m_measurementPointBLabel->hide();
    }

    QPointF midpointScreen;

    if (projectWorldPointToScreen((m_measurementPointA + m_measurementPointB) * 0.5f, midpointScreen))
    {
        m_measurementLengthLabel->setText(QString("L = %1").arg(measurementLength(), 0, 'f', 3));
        m_measurementLengthLabel->adjustSize();
        m_measurementLengthLabel->move(qRound(midpointScreen.x()) + 10, qRound(midpointScreen.y()) + 10);
        m_measurementLengthLabel->show();
        m_measurementLengthLabel->raise();
    }
    else
    {
        m_measurementLengthLabel->hide();
    }
}

RenderItem* OpenGLViewerWidget::pickedItem()
{
    return m_pickedItem;
}

const RenderItem* OpenGLViewerWidget::pickedItem() const
{
    return m_pickedItem;
}

void OpenGLViewerWidget::clearPickedItem()
{
    if (m_pickedItem != 0)
        // qDebug() << "OpenGLViewerWidget Picked Item cleared:" << m_pickedItem->name();

    m_pickedItem = 0;
}

/// Highlight

bool OpenGLViewerWidget::showBoundsHighlight(const AxisAlignedBoundingBox& bounds)
{
    if (m_boundsHighlight == 0)
        return false;

    if (!bounds.isValid())
    {
        qWarning() << "OpenGLViewerWidget showBoundsHighlight failed: bounds are invalid.";
        clearBoundsHighlight();
        return false;
    }

    const QVector3D& minimum = bounds.minimum();
    const QVector3D& maximum = bounds.maximum();
    std::vector<GLfloat> vertices;
    vertices.reserve(8 * 6);

    const QVector3D corners[] =
    {
        QVector3D(minimum.x(), minimum.y(), minimum.z()),
        QVector3D(maximum.x(), minimum.y(), minimum.z()),
        QVector3D(maximum.x(), maximum.y(), minimum.z()),
        QVector3D(minimum.x(), maximum.y(), minimum.z()),
        QVector3D(minimum.x(), minimum.y(), maximum.z()),
        QVector3D(maximum.x(), minimum.y(), maximum.z()),
        QVector3D(maximum.x(), maximum.y(), maximum.z()),
        QVector3D(minimum.x(), maximum.y(), maximum.z())
    };

    for (int i = 0; i < 8; ++i)
    {
        vertices.push_back(corners[i].x());
        vertices.push_back(corners[i].y());
        vertices.push_back(corners[i].z());

        // 黄色只表达显式 Bounds Highlight，不修改任何业务对象自己的 Material。
        vertices.push_back(1.0f);
        vertices.push_back(1.0f);
        vertices.push_back(0.0f);
    }

    m_boundsHighlight->setVertexData(vertices);
    m_showBoundsHighlight = true;
    return true;
}

void OpenGLViewerWidget::clearBoundsHighlight()
{
    m_showBoundsHighlight = false;
}

bool OpenGLViewerWidget::showTriangleHighlight(const QVector3D& vertex0, const QVector3D& vertex1, const QVector3D& vertex2)
{
    if (m_primitiveHighlight == 0)
        return false;

    const QVector3D verticesWorld[] =
    {
        vertex0,
        vertex1,
        vertex2
    };

    std::vector<GLfloat> vertices;
    vertices.reserve(3 * 6);

    for (int i = 0; i < 3; ++i)
    {
        vertices.push_back(verticesWorld[i].x());
        vertices.push_back(verticesWorld[i].y());
        vertices.push_back(verticesWorld[i].z());

        // 橙色只表达显式 World Primitive Highlight，不依赖 PrimitiveIndex、Picking Hit 或 Selection。
        vertices.push_back(1.0f);
        vertices.push_back(0.35f);
        vertices.push_back(0.0f);
    }

    if (!m_primitiveHighlight->updateVertexData(0, &vertices[0], static_cast<int>(vertices.size())))
    {
        qWarning() << "OpenGLViewerWidget showTriangleHighlight failed: unable to update Primitive Highlight.";
        clearPrimitiveHighlight();
        return false;
    }

    m_showPrimitiveHighlight = true;
    return true;
}

bool OpenGLViewerWidget::showLineHighlight(const QVector3D& vertex0, const QVector3D& vertex1)
{
    if (m_primitiveHighlight == 0)
        return false;

    // Highlight Geometry 固定保留 3 个 Vertex / 3 条边。
    // Line 模式让第三个 Vertex 与第二个重合，因此另外两条边退化为零长度，不需要改变 Index Buffer。
    const QVector3D verticesWorld[] =
    {
        vertex0,
        vertex1,
        vertex1
    };

    std::vector<GLfloat> vertices;
    vertices.reserve(3 * 6);

    for (int i = 0; i < 3; ++i)
    {
        vertices.push_back(verticesWorld[i].x());
        vertices.push_back(verticesWorld[i].y());
        vertices.push_back(verticesWorld[i].z());

        vertices.push_back(1.0f);
        vertices.push_back(0.35f);
        vertices.push_back(0.0f);
    }

    if (!m_primitiveHighlight->updateVertexData(0, &vertices[0], static_cast<int>(vertices.size())))
    {
        qWarning() << "OpenGLViewerWidget showLineHighlight failed: unable to update Primitive Highlight.";
        clearPrimitiveHighlight();
        return false;
    }

    m_showPrimitiveHighlight = true;
    return true;
}

void OpenGLViewerWidget::clearPrimitiveHighlight()
{
    m_showPrimitiveHighlight = false;
}

/// Camera

void OpenGLViewerWidget::resetCamera()
{
    Camera* camera = m_cameraManager.activeCamera();

    if (camera == 0)
        return;

    // Projection 是 Camera 自身属性；视图空间状态全部由 CameraManager 的 View Bounds + Direction 管理。
    if (!camera->setPerspective(45.0f, 0.1f, 1000.0f))
        return;

    // Reset 首先恢复 CameraManager 默认原点单位盒。
    // 旧 Viewer 默认视角 Position=(8,5,9)、Target=(0,1,0)，对应 Forward=(-8,-4,-9)；
    // 新模型只保留这个观察方向，不再直接设置 Position / Target。
    m_cameraManager.clearViewBounds();

    const QVector3D defaultForward(-8.0f, -4.0f, -9.0f);
    const QVector3D defaultUp(0.0f, 1.0f, 0.0f);

    if (!m_cameraManager.setViewDirection(defaultForward, defaultUp))
        return;

    // 构造阶段也允许基于默认 View Bounds 建立稳定 Camera；
    // initializeGL() 在 Scene Item 建立后会用真实 Scene Bounds 再执行一次初始 Fit。
    if (width() > 0 && height() > 0)
        m_cameraManager.fitViewBounds(width(), height());
}

bool OpenGLViewerWidget::fitSceneToView()
{
    AxisAlignedBoundingBox bounds;

    if (!m_scene.worldBounds(bounds, true))
    {
        qWarning() << "OpenGLViewerWidget fitSceneToView failed: Scene contains no visible Bounds.";
        return false;
    }

    if (!m_cameraManager.fitBounds(bounds, width(), height()))
        return false;

    const Camera* camera = m_cameraManager.activeCamera();

    // qDebug() << "OpenGLViewerWidget Fit All:"
    //          << "Minimum=" << bounds.minimum()
    //          << "Maximum=" << bounds.maximum()
    //          << "Center=" << bounds.center()
    //          << "CameraDistance=" << (camera != 0 ? camera->distanceToTarget() : 0.0f);

    return true;
}

bool OpenGLViewerWidget::focusPoint(const QVector3D& worldPoint)
{
    // CameraManager 会平移整个当前 View Bounds，使其 Center 移到 worldPoint。
    return m_cameraManager.focusPoint(worldPoint);
}

bool OpenGLViewerWidget::focusBounds(const AxisAlignedBoundingBox& bounds)
{
    // 明确 Bounds 直接成为新的 CameraManager View Bounds。
    return m_cameraManager.focusBounds(bounds);
}

bool OpenGLViewerWidget::focusItem(const RenderItem* item)
{
    if (item == 0)
    {
        qWarning() << "OpenGLViewerWidget focusItem failed: item is null.";
        return false;
    }

    if (!item->hasLocalBounds())
    {
        qWarning() << "OpenGLViewerWidget focusItem failed: item has no Bounds:" << item->name();
        return false;
    }

    const AxisAlignedBoundingBox bounds = item->worldBounds();

    if (!bounds.isValid())
    {
        qWarning() << "OpenGLViewerWidget focusItem failed: item World Bounds are invalid:" << item->name();
        return false;
    }

    return focusBounds(bounds);
}

bool OpenGLViewerWidget::focusPrimitive(const ScenePrimitiveHit& hit)
{
    if (hit.item == 0 || hit.primitiveIndex < 0 || hit.vertexCount <= 0 || hit.vertexCount > 3)
    {
        qWarning() << "OpenGLViewerWidget focusPrimitive failed: primitive hit is invalid.";
        return false;
    }

    QVector3D center(0.0f, 0.0f, 0.0f);

    for (int vertexIndex = 0; vertexIndex < hit.vertexCount; ++vertexIndex)
        center += hit.vertices[vertexIndex];

    center /= static_cast<float>(hit.vertexCount);

    // focusPoint() 平移当前 View Bounds 到明确 Render Primitive 几何中心，不依赖当前 Picking State。
    return focusPoint(center);
}

bool OpenGLViewerWidget::fitBoundsToView(const AxisAlignedBoundingBox& bounds, float margin)
{
    return m_cameraManager.fitBounds(bounds, width(), height(), margin);
}

bool OpenGLViewerWidget::fitItemToView(const RenderItem* item, float margin)
{
    if (item == 0)
    {
        qWarning() << "OpenGLViewerWidget fitItemToView failed: item is null.";
        return false;
    }

    if (!item->hasLocalBounds())
    {
        qWarning() << "OpenGLViewerWidget fitItemToView failed: item has no Bounds:" << item->name();
        return false;
    }

    const AxisAlignedBoundingBox bounds = item->worldBounds();

    if (!bounds.isValid())
    {
        qWarning() << "OpenGLViewerWidget fitItemToView failed: item World Bounds are invalid:" << item->name();
        return false;
    }

    return fitBoundsToView(bounds, margin);
}

bool OpenGLViewerWidget::fitPrimitiveToView(const ScenePrimitiveHit& hit, float margin)
{
    if (hit.item == 0 || hit.primitiveIndex < 0 || hit.vertexCount <= 0 || hit.vertexCount > 3)
    {
        qWarning() << "OpenGLViewerWidget fitPrimitiveToView failed: primitive hit is invalid.";
        return false;
    }

    AxisAlignedBoundingBox bounds;

    for (int vertexIndex = 0; vertexIndex < hit.vertexCount; ++vertexIndex)
        bounds.expandToInclude(hit.vertices[vertexIndex]);

    return fitBoundsToView(bounds, margin);
}

void OpenGLViewerWidget::logCameraView(const char* viewName) const
{
    const Camera* camera = m_cameraManager.activeCamera();

    if (camera == 0)
        return;

    // qDebug() << "OpenGLViewerWidget Standard View:"
    //          << viewName
    //          << "ViewBoundsCenter=" << m_cameraManager.viewBounds().center()
    //          << "Position=" << camera->position()
    //          << "Target=" << camera->target()
    //          << "Forward=" << camera->forward()
    //          << "ViewUp=" << camera->viewUp()
    //          << "CameraDistance=" << camera->distanceToTarget();
}

/// 低层访问

RenderContext& OpenGLViewerWidget::renderContext()
{
    return m_renderContext;
}

const RenderContext& OpenGLViewerWidget::renderContext() const
{
    return m_renderContext;
}

Renderer& OpenGLViewerWidget::renderer()
{
    return m_renderer;
}

const Renderer& OpenGLViewerWidget::renderer() const
{
    return m_renderer;
}