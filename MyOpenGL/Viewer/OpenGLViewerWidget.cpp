#include "OpenGLViewerWidget.h"

#include <QApplication>
#include <QDebug>
#include <QKeyEvent>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <vector>

#include "Camera/Camera.h"
#include "Resource/CoordinateSystemResource.h"
#include "Resource/GridPlaneResource.h"
#include "Resource/MeshResource.h"
#include "Resource/ViewNavigationResource.h"
#include "Scene/AxisAlignedBoundingBox.h"
#include "Scene/RenderItem.h"

OpenGLViewerWidget::OpenGLViewerWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , m_pickedItem(0)
    , m_grid(0)
    , m_axes(0)
    , m_viewNavigation(0)
    , m_cameraTargetMarker(0)
    , m_boundsHighlight(0)
    , m_primitiveHighlight(0)
    , m_leftDragOccurred(false)
    , m_glReady(false)
    , m_releasePerformed(false)
    , m_showGrid(true)
    , m_showAxes(true)
    , m_showViewNavigation(true)
    , m_showCameraTarget(true)
    , m_showBoundsHighlight(false)
    , m_showPrimitiveHighlight(false)
{
    setFocusPolicy(Qt::StrongFocus);

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

    m_renderer.setClearColor(QVector4D(0.08f, 0.08f, 0.1f, 1.0f));
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
        if (!m_renderer.drawVertexColorMesh(m_axes, identityModel, true))
        {
            qWarning() << "OpenGLViewerWidget paintGL failed: World Axis drawing failed.";
            m_renderer.endFrame();
            return;
        }
    }

    if (m_showGrid && m_grid != 0)
    {
        if (!m_renderer.drawVertexColorMesh(m_grid, identityModel, true))
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

        if (!m_renderer.drawVertexColorMesh(m_cameraTargetMarker, targetModel, false))
        {
            qWarning() << "OpenGLViewerWidget paintGL failed: Camera Target drawing failed.";
            m_renderer.endFrame();
            return;
        }
    }

    if (m_showBoundsHighlight && m_boundsHighlight != 0)
    {
        // Highlight 顶点已经是 World Space，因此 Model Matrix 保持 Identity；关闭 Depth Test 保证全部边可见。
        if (!m_renderer.drawVertexColorMesh(m_boundsHighlight, identityModel, false))
        {
            qWarning() << "OpenGLViewerWidget paintGL failed: Bounds Highlight drawing failed.";
            m_renderer.endFrame();
            return;
        }
    }

    if (m_showPrimitiveHighlight && m_primitiveHighlight != 0)
    {
        // Primitive Highlight 同样直接保存 World Triangle Vertex，不经过 RenderItem Transform。
        if (!m_renderer.drawVertexColorMesh(m_primitiveHighlight, identityModel, false))
        {
            qWarning() << "OpenGLViewerWidget paintGL failed: Primitive Highlight drawing failed.";
            m_renderer.endFrame();
            return;
        }
    }

    // View Navigation 使用独立的屏幕角落 Viewport，同样不属于用户 Scene。
    if (m_showViewNavigation)
        m_renderer.drawViewNavigation(m_viewNavigation, camera);

    m_renderer.endFrame();
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

    // 派生 Shared GPU Storage 必须在 MyOpenGL Resource（尤其 ExternalGpuMeshResource VAO）释放之后再销毁。
    afterViewerGLReleased();
    m_glReady = false;
}

/// 输入

void OpenGLViewerWidget::mousePressEvent(QMouseEvent* event)
{
    m_lastMousePosition = event->pos();

    if (event->button() == Qt::LeftButton)
    {
        m_leftPressPosition = event->pos();
        m_leftDragOccurred = false;

        // Left Button 同时承担 Click Picking 和 Drag Orbit。
        // 当前 Widget 主动接管这次手势，避免基类默认处理改变事件状态。
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
        // 使用 Qt 平台标准 Drag Threshold。
        // 在阈值以内完全保持 Click 候选状态，不执行 Orbit；只有真正越过阈值后才进入 Drag Orbit。
        if (!m_leftDragOccurred)
        {
            const int dragDistance = (currentPosition - m_leftPressPosition).manhattanLength();

            if (dragDistance < QApplication::startDragDistance())
            {
                event->accept();
                return;
            }

            m_leftDragOccurred = true;
        }

        // 每个鼠标像素对应 0.3 度 Orbit，作为当前桌面交互的基础灵敏度。
        const float orbitDegreesPerPixel = 0.3f;
        m_cameraManager.orbit(-delta.x() * orbitDegreesPerPixel, -delta.y() * orbitDegreesPerPixel);

        viewerStateChanged();
        update();
        event->accept();
        return;
    }

    if (event->buttons() & Qt::RightButton)
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

    QOpenGLWidget::mouseMoveEvent(event);
}

void OpenGLViewerWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        // 只要本次手势从未越过 Qt Drag Threshold，就按 Click 处理。
        // Release 位置直接用于生成 Picking Ray。
        if (!m_leftDragOccurred)
            pickAt(event->pos());

        m_leftDragOccurred = false;
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
        // Esc 是 Viewer 交互组合：清除最近 Pick 状态，并显式清除两种 Highlight。
        clearPickedItem();
        clearBoundsHighlight();
        clearPrimitiveHighlight();
        viewerStateChanged();
        update();
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
            qDebug() << "OpenGLViewerWidget Fit Picked Item ignored: no item is currently picked.";
            return;
        }

        if (fitItemToView(currentPickedItem))
        {
            const AxisAlignedBoundingBox bounds = currentPickedItem->worldBounds();
            const Camera* camera = m_cameraManager.activeCamera();

            // F 只属于 Viewer 快捷键交互语义；基础 fitItemToView() 只接收明确 Item。
            qDebug() << "OpenGLViewerWidget Fit Picked Item:"
                     << "Item=" << currentPickedItem->name()
                     << "Minimum=" << bounds.minimum()
                     << "Maximum=" << bounds.maximum()
                     << "Center=" << bounds.center()
                     << "CameraDistance=" << (camera != 0 ? camera->distanceToTarget() : 0.0f);

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

    m_grid = new GridPlaneResource("WorldGrid");
    m_grid->setGrid(10.0f, 1.0f);
    m_resourceManager.add(m_grid);

    /// Coordinate System

    m_axes = new CoordinateSystemResource("WorldAxes");
    m_axes->setAxisLength(3.0f);
    m_resourceManager.add(m_axes);

    /// View Navigation

    m_viewNavigation = new ViewNavigationResource("ViewNavigation");
    m_viewNavigation->setAxisLength(1.0f);
    m_resourceManager.add(m_viewNavigation);

    /// Camera Target Marker

    m_cameraTargetMarker = createCameraTargetMarker();
    m_resourceManager.add(m_cameraTargetMarker);

    /// Highlight Overlays

    m_boundsHighlight = createBoundsHighlightMesh();
    m_resourceManager.add(m_boundsHighlight);

    m_primitiveHighlight = createPrimitiveHighlightMesh();
    m_resourceManager.add(m_primitiveHighlight);

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

    // Scene 只拥有用户可操作 RenderItem。Grid / Axis / Camera Target / Highlight / ViewNavigation
    // 都是 Viewer 内部模型，不进入 Scene，也不占用 Item 操作语义。
    m_scene.clear();

    buildViewerContentItems();

    qDebug() << "OpenGLViewerWidget User Scene built:"
             << "Items=" << m_scene.itemCount();
}

MeshResource* OpenGLViewerWidget::createCameraTargetMarker()
{
    MeshResource* marker = new MeshResource("CameraTargetMarker", ResourceUpdateStatic, Lines);

    std::vector<MeshVertexAttribute> attributes;

    MeshVertexAttribute positionAttribute;
    positionAttribute.location = 0;
    positionAttribute.componentCount = 3;
    positionAttribute.valueOffset = 0;
    attributes.push_back(positionAttribute);

    MeshVertexAttribute colorAttribute;
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

MeshResource* OpenGLViewerWidget::createBoundsHighlightMesh()
{
    MeshResource* boundsMesh = new MeshResource("BoundsHighlight", ResourceUpdateDynamic, Lines);

    std::vector<MeshVertexAttribute> attributes;

    MeshVertexAttribute positionAttribute;
    positionAttribute.location = 0;
    positionAttribute.componentCount = 3;
    positionAttribute.valueOffset = 0;
    attributes.push_back(positionAttribute);

    MeshVertexAttribute colorAttribute;
    colorAttribute.location = 1;
    colorAttribute.componentCount = 3;
    colorAttribute.valueOffset = 3;
    attributes.push_back(colorAttribute);

    boundsMesh->setVertexLayout(6, attributes);

    // Resource 初始化阶段必须已经具有合法 Mesh 数据；初始单位盒不绘制，真正 Highlight 时再替换为明确 World AABB。
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

    boundsMesh->setVertexData(std::vector<GLfloat>(vertices, vertices + vertexValueCount));
    boundsMesh->setIndexData(std::vector<GLuint>(indices, indices + indexCount));
    return boundsMesh;
}

MeshResource* OpenGLViewerWidget::createPrimitiveHighlightMesh()
{
    MeshResource* primitiveMesh = new MeshResource("PrimitiveHighlight", ResourceUpdateDynamic, Lines);

    std::vector<MeshVertexAttribute> attributes;

    MeshVertexAttribute positionAttribute;
    positionAttribute.location = 0;
    positionAttribute.componentCount = 3;
    positionAttribute.valueOffset = 0;
    attributes.push_back(positionAttribute);

    MeshVertexAttribute colorAttribute;
    colorAttribute.location = 1;
    colorAttribute.componentCount = 3;
    colorAttribute.valueOffset = 3;
    attributes.push_back(colorAttribute);

    primitiveMesh->setVertexLayout(6, attributes);

    // Resource 初始化需要合法数据；初始单位 Triangle 不绘制。
    // 橙色专门表示明确 World Triangle，和黄色 World AABB 区分。
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

    primitiveMesh->setVertexData(std::vector<GLfloat>(vertices, vertices + vertexValueCount));
    primitiveMesh->setIndexData(std::vector<GLuint>(indices, indices + indexCount));
    return primitiveMesh;
}

/// Picking / 当前交互状态

bool OpenGLViewerWidget::pickAt(const QPoint& position)
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

    ScenePrimitiveHit primitiveHit;

    if (m_scene.raycastPrimitive(primitiveCandidates, rayOrigin, rayDirection, primitiveHit, true))
    {
        if (!setPickedItem(primitiveHit.item))
            return false;

        const AxisAlignedBoundingBox bounds = primitiveHit.item->worldBounds();
        showBoundsHighlight(bounds);
        showPrimitiveHighlight(primitiveHit.vertices[0], primitiveHit.vertices[1], primitiveHit.vertices[2]);

        qDebug() << "OpenGLViewerWidget Primitive Pick changed:"
                 << "Item=" << primitiveHit.item->name()
                 << "PrimitiveIndex=" << primitiveHit.primitiveIndex
                 << "HitDistance=" << primitiveHit.distance
                 << "HitPosition=" << primitiveHit.position
                 << "Barycentric=" << primitiveHit.barycentric
                 << "BoundsMinimum=" << bounds.minimum()
                 << "BoundsMaximum=" << bounds.maximum();

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

        qDebug() << "OpenGLViewerWidget Object Bounds Pick fallback:"
                 << "Item=" << objectHit.item->name()
                 << "HitDistance=" << objectHit.distance
                 << "HitPosition=" << objectHit.position
                 << "BoundsMinimum=" << bounds.minimum()
                 << "BoundsMaximum=" << bounds.maximum();

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
    return true;
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
        qDebug() << "OpenGLViewerWidget Picked Item cleared:" << m_pickedItem->name();

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

bool OpenGLViewerWidget::showPrimitiveHighlight(const QVector3D& vertex0, const QVector3D& vertex1, const QVector3D& vertex2)
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

        // 橙色只表达显式 World Triangle Highlight，不依赖 PrimitiveIndex、Picking Hit 或 Selection。
        vertices.push_back(1.0f);
        vertices.push_back(0.35f);
        vertices.push_back(0.0f);
    }

    if (!m_primitiveHighlight->updateVertexData(0, &vertices[0], static_cast<int>(vertices.size())))
    {
        qWarning() << "OpenGLViewerWidget showPrimitiveHighlight failed: unable to update Primitive Highlight.";
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

    qDebug() << "OpenGLViewerWidget Fit All:"
             << "Minimum=" << bounds.minimum()
             << "Maximum=" << bounds.maximum()
             << "Center=" << bounds.center()
             << "CameraDistance=" << (camera != 0 ? camera->distanceToTarget() : 0.0f);

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
    if (hit.item == 0 || hit.primitiveIndex < 0)
    {
        qWarning() << "OpenGLViewerWidget focusPrimitive failed: primitive hit is invalid.";
        return false;
    }

    // Render Primitive 当前为 Triangle；几何中心只由三个 World Vertex 决定，不使用鼠标 HitPosition。
    // focusPoint() 会把当前 View Bounds 整体平移到这个中心，而不是创建一个零尺寸 Point Bounds。
    const QVector3D center = (hit.vertices[0] + hit.vertices[1] + hit.vertices[2]) / 3.0f;
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
    if (hit.item == 0 || hit.primitiveIndex < 0)
    {
        qWarning() << "OpenGLViewerWidget fitPrimitiveToView failed: primitive hit is invalid.";
        return false;
    }

    AxisAlignedBoundingBox bounds;
    bounds.expandToInclude(hit.vertices[0]);
    bounds.expandToInclude(hit.vertices[1]);
    bounds.expandToInclude(hit.vertices[2]);

    return fitBoundsToView(bounds, margin);
}

void OpenGLViewerWidget::logCameraView(const char* viewName) const
{
    const Camera* camera = m_cameraManager.activeCamera();

    if (camera == 0)
        return;

    qDebug() << "OpenGLViewerWidget Standard View:"
             << viewName
             << "ViewBoundsCenter=" << m_cameraManager.viewBounds().center()
             << "Position=" << camera->position()
             << "Target=" << camera->target()
             << "Forward=" << camera->forward()
             << "ViewUp=" << camera->viewUp()
             << "CameraDistance=" << camera->distanceToTarget();
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