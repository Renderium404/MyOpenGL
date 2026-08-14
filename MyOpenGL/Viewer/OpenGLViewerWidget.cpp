#include "OpenGLViewerWidget.h"

#include <QApplication>
#include <QDebug>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QWheelEvent>

#include <cmath>
#include <vector>

#include "Camera/Camera.h"
#include "Material/Material.h"
#include "Resource/CoordinateSystemResource.h"
#include "Resource/GridPlaneResource.h"
#include "Resource/MeshResource.h"
#include "Resource/ViewNavigationResource.h"
#include "Scene/AxisAlignedBoundingBox.h"
#include "Scene/RenderItem.h"

OpenGLViewerWidget::OpenGLViewerWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , m_grid(0)
    , m_axes(0)
    , m_viewNavigation(0)
    , m_cameraTargetMarker(0)
    , m_selectionBounds(0)
    , m_selectionPrimitive(0)
    , m_vertexColorMaterial(0)
    , m_axesItem(0)
    , m_gridItem(0)
    , m_cameraTargetItem(0)
    , m_selectionBoundsItem(0)
    , m_selectionPrimitiveItem(0)
    , m_leftDragOccurred(false)
    , m_glReady(false)
    , m_releasePerformed(false)
    , m_showGrid(true)
    , m_showAxes(true)
    , m_showViewNavigation(true)
    , m_showCameraTarget(true)
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

    if (m_gridItem != 0)
        m_gridItem->setVisible(visible);

    viewerStateChanged();
    update();
}

void OpenGLViewerWidget::setAxesVisible(bool visible)
{
    m_showAxes = visible;

    if (m_axesItem != 0)
        m_axesItem->setVisible(visible);

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

    if (m_cameraTargetItem != 0)
        m_cameraTargetItem->setVisible(visible);

    viewerStateChanged();
    update();
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

    // 每次 Context 初始化都重建 RenderItem 借用关系；Resource / Material 的所有权仍由 Manager 持有。
    rebuildViewerSceneItems();

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

    if (m_cameraTargetItem != 0)
    {
        // Marker 世界尺寸随 Camera Distance 等比例变化，因此屏幕视觉尺寸近似保持稳定。
        const float targetMarkerScale = camera->distanceToTarget() * 0.02f;
        m_cameraTargetItem->transform().setPosition(camera->target());
        m_cameraTargetItem->transform().setUniformScale(targetMarkerScale);
    }

    // 普通世界对象统一由 Scene 驱动；Scene Item 创建顺序同时定义当前基础绘制顺序。
    if (!m_renderer.drawScene(&m_scene, &m_resourceManager, &m_lightManager))
    {
        qWarning() << "OpenGLViewerWidget paintGL failed: Scene drawing failed.";
        m_renderer.endFrame();
        return;
    }

    // View Navigation 使用独立的屏幕角落 Viewport，不属于普通世界 Scene Item。
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

        // Left Button 同时承担 Click Selection 和 Drag Orbit。
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
            selectObjectAt(event->pos());

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
        clearObjectSelection();
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
        if (m_cameraManager.viewFront(m_scene, width(), height()))
        {
            logCameraView("Front");
            viewerStateChanged();
            update();
        }

        return;
    }

    if (event->key() == Qt::Key_2)
    {
        if (m_cameraManager.viewBack(m_scene, width(), height()))
        {
            logCameraView("Back");
            viewerStateChanged();
            update();
        }

        return;
    }

    if (event->key() == Qt::Key_3)
    {
        if (m_cameraManager.viewLeft(m_scene, width(), height()))
        {
            logCameraView("Left");
            viewerStateChanged();
            update();
        }

        return;
    }

    if (event->key() == Qt::Key_4)
    {
        if (m_cameraManager.viewRight(m_scene, width(), height()))
        {
            logCameraView("Right");
            viewerStateChanged();
            update();
        }

        return;
    }

    if (event->key() == Qt::Key_5)
    {
        if (m_cameraManager.viewTop(m_scene, width(), height()))
        {
            logCameraView("Top");
            viewerStateChanged();
            update();
        }

        return;
    }

    if (event->key() == Qt::Key_6)
    {
        if (m_cameraManager.viewBottom(m_scene, width(), height()))
        {
            logCameraView("Bottom");
            viewerStateChanged();
            update();
        }

        return;
    }

    if (event->key() == Qt::Key_7)
    {
        if (m_cameraManager.viewIsometric(m_scene, width(), height()))
        {
            logCameraView("Isometric");
            viewerStateChanged();
            update();
        }

        return;
    }

    if (event->key() == Qt::Key_F)
    {
        if (fitSelectionToView())
        {
            viewerStateChanged();
            update();
        }

        return;
    }

    if (event->key() == Qt::Key_O)
    {
        m_cameraManager.focus(QVector3D(0.0f, 0.0f, 0.0f));
        viewerStateChanged();
        update();
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

    /// Selection Overlays

    m_selectionBounds = createSelectionBoundsMesh();
    m_resourceManager.add(m_selectionBounds);

    m_selectionPrimitive = createSelectionPrimitiveMesh();
    m_resourceManager.add(m_selectionPrimitive);

    /// Viewer Material

    // Vertex Color Material 不保存额外表面参数，只用于告诉 Renderer 选择 VertexColor 管线。
    m_vertexColorMaterial = new Material("ViewerVertexColorMaterial");
    m_vertexColorMaterial->setVertexColor();
    m_materialManager.add(m_vertexColorMaterial);

    /// Camera

    Camera* camera = new Camera("MainCamera");
    camera->setPerspective(45.0f, 0.1f, 1000.0f);
    m_cameraManager.add(camera);
}

void OpenGLViewerWidget::rebuildViewerSceneItems()
{
    // Context 重建时 Scene Item 没有 GPU 状态，因此只重建借用关系；Resource / Material CPU 对象继续由 Manager 持有。
    m_scene.clear();

    m_axesItem = 0;
    m_gridItem = 0;
    m_cameraTargetItem = 0;
    m_selectionBoundsItem = 0;
    m_selectionPrimitiveItem = 0;

    // Axis 必须先于 Grid 绘制，使与 Grid 共面的 X / Z 轴在 GL_LESS 深度测试下优先保留。
    if (m_axes != 0 && m_vertexColorMaterial != 0)
    {
        m_axesItem = m_scene.createItem("WorldAxesItem");
        m_axesItem->setMesh(m_axes);
        m_axesItem->setMaterial(m_vertexColorMaterial);
        m_axesItem->setVisible(m_showAxes);
    }

    if (m_grid != 0 && m_vertexColorMaterial != 0)
    {
        m_gridItem = m_scene.createItem("WorldGridItem");
        m_gridItem->setMesh(m_grid);
        m_gridItem->setMaterial(m_vertexColorMaterial);
        m_gridItem->setVisible(m_showGrid);
    }

    // 业务 / SandBox Item 插入在世界辅助线之后、Camera/Selection Overlay 之前。
    buildViewerContentItems();

    if (m_cameraTargetMarker != 0 && m_vertexColorMaterial != 0)
    {
        m_cameraTargetItem = m_scene.createItem("CameraTargetItem");
        m_cameraTargetItem->setMesh(m_cameraTargetMarker);
        m_cameraTargetItem->setMaterial(m_vertexColorMaterial);
        m_cameraTargetItem->setVisible(m_showCameraTarget);

        // Target 可能位于模型内部，因此调试标记关闭 Depth Test，并保持在普通 Scene 最后绘制。
        // 调试 Marker 不设置 Local Bounds，因此不会影响 Fit All / Picking。
        m_cameraTargetItem->setDepthTestEnabled(false);
    }

    if (m_selectionBounds != 0 && m_vertexColorMaterial != 0)
    {
        m_selectionBoundsItem = m_scene.createItem("SelectionBoundsItem");
        m_selectionBoundsItem->setMesh(m_selectionBounds);
        m_selectionBoundsItem->setMaterial(m_vertexColorMaterial);
        m_selectionBoundsItem->setVisible(false);

        // Selection Overlay 使用 World AABB 顶点并关闭 Depth Test，保证被模型遮挡的边仍然可见。
        // Overlay 不设置 Local Bounds，因此不会参与 Fit All，也不会被再次 Pick。
        m_selectionBoundsItem->setDepthTestEnabled(false);
    }

    if (m_selectionPrimitive != 0 && m_vertexColorMaterial != 0)
    {
        m_selectionPrimitiveItem = m_scene.createItem("SelectionPrimitiveItem");
        m_selectionPrimitiveItem->setMesh(m_selectionPrimitive);
        m_selectionPrimitiveItem->setMaterial(m_vertexColorMaterial);
        m_selectionPrimitiveItem->setVisible(false);

        // Primitive Overlay 使用命中 Triangle 的 World Vertex，并关闭 Depth Test 便于验证精确拾取结果。
        // 它没有 Bounds 和 PrimitivePickSource，因此不会参与 Fit / Picking。
        m_selectionPrimitiveItem->setDepthTestEnabled(false);
    }

    qDebug() << "OpenGLViewerWidget Render Scene built:"
             << "Items=" << m_scene.itemCount();
}

MeshResource* OpenGLViewerWidget::createCameraTargetMarker()
{
    MeshResource* marker = new MeshResource("CameraTargetMarker", ResourceUpdateStatic, MeshPrimitiveLines);

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

MeshResource* OpenGLViewerWidget::createSelectionBoundsMesh()
{
    MeshResource* boundsMesh = new MeshResource("SelectionBounds", ResourceUpdateDynamic, MeshPrimitiveLines);

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

    // Resource 初始化阶段必须已经具有合法 Mesh 数据；初始单位盒保持隐藏，真正 Selection 后再替换为 World AABB。
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

MeshResource* OpenGLViewerWidget::createSelectionPrimitiveMesh()
{
    MeshResource* primitiveMesh = new MeshResource("SelectionPrimitive", ResourceUpdateDynamic, MeshPrimitiveLines);

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

    // Resource 初始化需要合法数据；初始单位 Triangle 保持隐藏。
    // 橙色专门表示精确 Render Triangle，和黄色 Object AABB 区分。
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

/// Selection

bool OpenGLViewerWidget::selectObjectAt(const QPoint& position)
{
    const Camera* camera = m_cameraManager.activeCamera();

    if (camera == 0)
    {
        qWarning() << "OpenGLViewerWidget selectObjectAt failed: active camera does not exist.";
        return false;
    }

    QVector3D rayOrigin;
    QVector3D rayDirection;

    if (!camera->screenPointToRay(position.x(), position.y(), width(), height(), rayOrigin, rayDirection))
        return false;

    ScenePrimitiveHit primitiveHit;

    // 优先使用精确 Render Primitive Picking。
    if (m_scene.raycastPrimitive(rayOrigin, rayDirection, primitiveHit, true))
    {
        if (!m_scene.setSelectedItem(primitiveHit.item))
            return false;

        updateSelectionBoundsMesh();
        updateSelectionPrimitiveMesh(primitiveHit);

        const AxisAlignedBoundingBox bounds = primitiveHit.item->worldBounds();

        qDebug() << "OpenGLViewerWidget Primitive Selection changed:"
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

    // 没有精确 Primitive 命中时仍保留 Object Picking Fallback，
    // 但只处理没有 Primitive Picker 的 Item，避免 Triangle Miss 被同一对象 AABB 重新误选中。
    SceneRayHit objectHit;

    if (m_scene.raycast(rayOrigin, rayDirection, objectHit, true, true))
    {
        if (!m_scene.setSelectedItem(objectHit.item))
            return false;

        updateSelectionBoundsMesh();
        clearPrimitiveSelectionVisual();

        const AxisAlignedBoundingBox bounds = objectHit.item->worldBounds();

        qDebug() << "OpenGLViewerWidget Object Selection fallback:"
                 << "Item=" << objectHit.item->name()
                 << "HitDistance=" << objectHit.distance
                 << "HitPosition=" << objectHit.position
                 << "BoundsMinimum=" << bounds.minimum()
                 << "BoundsMaximum=" << bounds.maximum();

        viewerStateChanged();
        update();
        return true;
    }

    clearObjectSelection();
    viewerStateChanged();
    update();
    return true;
}

void OpenGLViewerWidget::clearObjectSelection()
{
    const RenderItem* previousSelection = m_scene.selectedItem();

    if (previousSelection != 0)
        qDebug() << "OpenGLViewerWidget Selection cleared:" << previousSelection->name();

    m_scene.clearSelection();

    if (m_selectionBoundsItem != 0)
        m_selectionBoundsItem->setVisible(false);

    clearPrimitiveSelectionVisual();
}

void OpenGLViewerWidget::refreshSelectionBounds()
{
    updateSelectionBoundsMesh();
}

void OpenGLViewerWidget::updateSelectionBoundsMesh()
{
    if (m_selectionBounds == 0 || m_selectionBoundsItem == 0)
        return;

    const RenderItem* selectedItem = m_scene.selectedItem();

    if (selectedItem == 0 || !selectedItem->hasLocalBounds())
    {
        m_selectionBoundsItem->setVisible(false);
        return;
    }

    const AxisAlignedBoundingBox bounds = selectedItem->worldBounds();

    if (!bounds.isValid())
    {
        m_selectionBoundsItem->setVisible(false);
        return;
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

        // 黄色仅作为 Selection 状态语义，不修改被选中对象自己的 Material。
        vertices.push_back(1.0f);
        vertices.push_back(1.0f);
        vertices.push_back(0.0f);
    }

    m_selectionBounds->setVertexData(vertices);
    m_selectionBoundsItem->transform().reset();
    m_selectionBoundsItem->setVisible(true);
}

void OpenGLViewerWidget::updateSelectionPrimitiveMesh(const ScenePrimitiveHit& hit)
{
    if (m_selectionPrimitive == 0 || m_selectionPrimitiveItem == 0 || hit.item == 0 || hit.primitiveIndex < 0)
    {
        clearPrimitiveSelectionVisual();
        return;
    }

    std::vector<GLfloat> vertices;
    vertices.reserve(3 * 6);

    for (int i = 0; i < 3; ++i)
    {
        vertices.push_back(hit.vertices[i].x());
        vertices.push_back(hit.vertices[i].y());
        vertices.push_back(hit.vertices[i].z());

        // 精确 Render Triangle 使用橙色，和黄色 Object AABB 区分。
        vertices.push_back(1.0f);
        vertices.push_back(0.35f);
        vertices.push_back(0.0f);
    }

    if (!m_selectionPrimitive->updateVertexData(0, &vertices[0], static_cast<int>(vertices.size())))
    {
        qWarning() << "OpenGLViewerWidget updateSelectionPrimitiveMesh failed: unable to update Primitive Overlay.";
        m_selectionPrimitiveItem->setVisible(false);
        return;
    }

    m_selectionPrimitiveItem->setVisible(true);
}

void OpenGLViewerWidget::clearPrimitiveSelectionVisual()
{
    if (m_selectionPrimitiveItem != 0)
        m_selectionPrimitiveItem->setVisible(false);
}

/// Camera

void OpenGLViewerWidget::resetCamera()
{
    Camera* camera = m_cameraManager.activeCamera();

    if (camera == 0)
        return;

    // 默认 Camera 位于 X/Y/Z 正方向，适合作为通用三维 Viewer 初始观察方向。
    camera->setView(QVector3D(8.0f, 5.0f, 9.0f), QVector3D(0.0f, 1.0f, 0.0f), QVector3D(0.0f, 1.0f, 0.0f));
    camera->setPerspective(45.0f, 0.1f, 1000.0f);
}

bool OpenGLViewerWidget::fitSceneToView()
{
    AxisAlignedBoundingBox bounds;

    if (!m_scene.worldBounds(bounds, true))
    {
        qWarning() << "OpenGLViewerWidget fitSceneToView failed: Scene contains no visible Bounds.";
        return false;
    }

    if (!m_cameraManager.fitAll(m_scene, width(), height()))
        return false;

    const Camera* camera = m_cameraManager.activeCamera();

    qDebug() << "OpenGLViewerWidget Fit All:"
             << "Minimum=" << bounds.minimum()
             << "Maximum=" << bounds.maximum()
             << "Center=" << bounds.center()
             << "CameraDistance=" << (camera != 0 ? camera->distanceToTarget() : 0.0f);

    return true;
}

bool OpenGLViewerWidget::fitSelectionToView()
{
    const RenderItem* selectedItem = m_scene.selectedItem();

    if (selectedItem == 0)
    {
        // 没有 Selection 是正常交互状态，不作为程序错误输出 Warning。
        qDebug() << "OpenGLViewerWidget Fit Selection ignored: no object is selected.";
        return false;
    }

    if (!selectedItem->hasLocalBounds())
    {
        qWarning() << "OpenGLViewerWidget fitSelectionToView failed: selected Item has no Bounds:" << selectedItem->name();
        return false;
    }

    const AxisAlignedBoundingBox bounds = selectedItem->worldBounds();

    if (!bounds.isValid())
    {
        qWarning() << "OpenGLViewerWidget fitSelectionToView failed: selected Item World Bounds are invalid:" << selectedItem->name();
        return false;
    }

    if (!m_cameraManager.fitBounds(bounds, width(), height()))
        return false;

    const Camera* camera = m_cameraManager.activeCamera();

    qDebug() << "OpenGLViewerWidget Fit Selection:"
             << "Item=" << selectedItem->name()
             << "Minimum=" << bounds.minimum()
             << "Maximum=" << bounds.maximum()
             << "Center=" << bounds.center()
             << "CameraDistance=" << (camera != 0 ? camera->distanceToTarget() : 0.0f);

    return true;
}

void OpenGLViewerWidget::logCameraView(const char* viewName) const
{
    const Camera* camera = m_cameraManager.activeCamera();

    if (camera == 0)
        return;

    qDebug() << "OpenGLViewerWidget Standard View:"
             << viewName
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
