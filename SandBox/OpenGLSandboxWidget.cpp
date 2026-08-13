#include "OpenGLSandboxWidget.h"

#include <QColor>
#include <QDebug>
#include <QImage>
#include <QKeyEvent>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QWheelEvent>

#include <cmath>
#include <vector>

#include "Camera/Camera.h"
#include "Light/Light.h"
#include "Material/Material.h"
#include "Resource/CoordinateSystemResource.h"
#include "Resource/ExternalMeshResource.h"
#include "Resource/GridPlaneResource.h"
#include "Resource/MeshResource.h"
#include "Resource/TextureResource.h"
#include "Resource/ViewNavigationResource.h"

OpenGLSandboxWidget::OpenGLSandboxWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , m_modelingMeshAdapter(&m_modelingMesh)
    , m_grid(0)
    , m_axes(0)
    , m_viewNavigation(0)
    , m_cameraTargetMarker(0)
    , m_cube(0)
    , m_externalMesh(0)
    , m_cubeTexture(0)
    , m_cubeMaterial(0)
    , m_sun(0)
    , m_cubeTextureId(InvalidResourceId)
    , m_glReady(false)
    , m_showGrid(true)
    , m_showAxes(true)
    , m_showViewNavigation(true)
    , m_showCameraTarget(true)
    , m_externalWide(false)
{
    setFocusPolicy(Qt::StrongFocus);

    buildScene();
    resetCamera();
    updateWindowTitle();
}

OpenGLSandboxWidget::~OpenGLSandboxWidget()
{
    releaseGL();
}

/// OpenGL 生命周期

void OpenGLSandboxWidget::initializeGL()
{
    if (!m_renderContext.initialize())
    {
        qWarning() << "OpenGLSandboxWidget initializeGL failed: RenderContext initialization failed.";
        return;
    }

    if (!m_renderer.initialize(&m_renderContext))
    {
        qWarning() << "OpenGLSandboxWidget initializeGL failed: Renderer initialization failed.";
        return;
    }

    QOpenGLFunctions_3_3_Core* gl = m_renderContext.gl();

    if (gl == 0)
        return;

    if (!m_resourceManager.syncAll(gl))
    {
        qWarning() << "OpenGLSandboxWidget initializeGL failed: Resource synchronization failed.";
        return;
    }

    m_renderer.setClearColor(QVector4D(0.08f, 0.08f, 0.1f, 1.0f));
    m_glReady = true;

    updateWindowTitle();
}

void OpenGLSandboxWidget::resizeGL(int width, int height)
{
    Q_UNUSED(width);
    Q_UNUSED(height);

    // Viewport 由 Renderer::beginFrame() 统一设置。
}

void OpenGLSandboxWidget::paintGL()
{
    if (!m_glReady)
        return;

    QOpenGLFunctions_3_3_Core* gl = m_renderContext.gl();

    if (gl == 0)
        return;

    // ExternalMeshResource 会在 syncAll() 内部自动比较 Revision，不需要 SandBox 手工标记 Dirty。
    if (!m_resourceManager.syncAll(gl))
    {
        qWarning() << "OpenGLSandboxWidget paintGL failed: Resource synchronization failed.";
        return;
    }

    const Camera* camera = m_cameraManager.activeCamera();

    if (camera == 0)
    {
        qWarning() << "OpenGLSandboxWidget paintGL failed: active camera does not exist.";
        return;
    }

    if (!m_renderer.beginFrame(camera, width(), height()))
        return;

    QMatrix4x4 identityModel;

    // Axis 先绘制，使与 Grid 共面的 X / Z 轴在 GL_LESS 深度测试下优先保留。
    if (m_showAxes)
        m_renderer.drawVertexColorMesh(m_axes, identityModel);

    if (m_showGrid)
        m_renderer.drawVertexColorMesh(m_grid, identityModel);

    // Cube 边长为 2，向上移动 1 个单位后底面位于 Y=0。
    QMatrix4x4 cubeModel;
    cubeModel.translate(0.0f, 1.0f, 0.0f);
    m_renderer.drawLitMesh(m_cube, m_cubeMaterial, &m_resourceManager, &m_lightManager, cubeModel);

    // External Mesh 放在 Cube 右侧；Renderer 不区分它来自 Owned Mesh 还是 External Mesh。
    QMatrix4x4 externalModel;
    externalModel.translate(3.5f, 0.0f, 0.0f);
    m_renderer.drawLitMesh(m_externalMesh, m_cubeMaterial, &m_resourceManager, &m_lightManager, externalModel);

    if (m_showCameraTarget)
    {
        // Marker 世界尺寸随 Camera Distance 等比例变化，因此屏幕视觉尺寸近似保持稳定。
        const float targetMarkerScale = camera->distanceToTarget() * 0.02f;

        QMatrix4x4 targetModel;
        targetModel.translate(camera->target());
        targetModel.scale(targetMarkerScale);

        // Target 可能位于模型内部，因此调试标记关闭 Depth Test。
        m_renderer.drawVertexColorMesh(m_cameraTargetMarker, targetModel, false);
    }

    if (m_showViewNavigation)
        m_renderer.drawViewNavigation(m_viewNavigation, camera);

    m_renderer.endFrame();

    // GPU Sync 统计在 paintGL() 中更新，因此每帧绘制结束后刷新标题中的最后同步状态。
    updateWindowTitle();
}

/// 输入

void OpenGLSandboxWidget::mousePressEvent(QMouseEvent* event)
{
    m_lastMousePosition = event->pos();
    QOpenGLWidget::mousePressEvent(event);
}

void OpenGLSandboxWidget::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint currentPosition = event->pos();
    const QPoint delta = currentPosition - m_lastMousePosition;
    m_lastMousePosition = currentPosition;

    if (event->buttons() & Qt::LeftButton)
    {
        // 每个鼠标像素对应 0.3 度 Orbit，作为当前桌面交互的基础灵敏度。
        const float orbitDegreesPerPixel = 0.3f;
        m_cameraManager.orbit(-delta.x() * orbitDegreesPerPixel, -delta.y() * orbitDegreesPerPixel);

        update();
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

        updateWindowTitle();
        update();
        return;
    }

    QOpenGLWidget::mouseMoveEvent(event);
}

void OpenGLSandboxWidget::wheelEvent(QWheelEvent* event)
{
    // Qt 标准鼠标滚轮一格通常对应 120 Angle Delta。
    const float wheelSteps = static_cast<float>(event->angleDelta().y()) / 120.0f;
    const float zoomFactor = static_cast<float>(std::pow(1.15, wheelSteps));

    if (zoomFactor > 0.0f)
        m_cameraManager.zoom(zoomFactor);

    updateWindowTitle();
    update();

    event->accept();
}

void OpenGLSandboxWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_R)
    {
        resetCamera();
        updateWindowTitle();
        update();
        return;
    }

    if (event->key() == Qt::Key_F)
    {
        // 当前 Cube 从 Y=0 到 Y=2，因此几何中心位于世界坐标 (0, 1, 0)。
        m_cameraManager.focus(QVector3D(0.0f, 1.0f, 0.0f));
        updateWindowTitle();
        update();
        return;
    }

    if (event->key() == Qt::Key_O)
    {
        m_cameraManager.focus(QVector3D(0.0f, 0.0f, 0.0f));
        updateWindowTitle();
        update();
        return;
    }

    if (event->key() == Qt::Key_C)
    {
        m_showCameraTarget = !m_showCameraTarget;
        updateWindowTitle();
        update();
        return;
    }

    if (event->key() == Qt::Key_G)
    {
        m_showGrid = !m_showGrid;
        updateWindowTitle();
        update();
        return;
    }

    if (event->key() == Qt::Key_A)
    {
        m_showAxes = !m_showAxes;
        updateWindowTitle();
        update();
        return;
    }

    if (event->key() == Qt::Key_V)
    {
        m_showViewNavigation = !m_showViewNavigation;
        updateWindowTitle();
        update();
        return;
    }

    if (event->key() == Qt::Key_L)
    {
        if (m_sun != 0)
            m_sun->setEnabled(!m_sun->isEnabled());

        updateWindowTitle();
        update();
        return;
    }

    if (event->key() == Qt::Key_T)
    {
        if (m_cubeMaterial != 0)
        {
            if (m_cubeMaterial->hasDiffuseTexture())
                m_cubeMaterial->clearDiffuseTexture();
            else
                m_cubeMaterial->setDiffuseTexture(m_cubeTextureId);
        }

        updateWindowTitle();
        update();
        return;
    }

    if (event->key() == Qt::Key_M)
    {
        // 只修改建模库自己的第 2 个 Position，不调用任何 MyOpenGL Dirty API。
        m_modelingMesh.raiseVertex(2, 0.25);

        updateWindowTitle();
        update();
        return;
    }

    if (event->key() == Qt::Key_B)
    {
        // 模拟建模库重新生成 Mesh；vector 地址可能改变，因此递增 Structure Revision。
        m_externalWide = !m_externalWide;
        m_modelingMesh.buildQuad(m_externalWide ? 1.5 : 1.0);

        updateWindowTitle();
        update();
        return;
    }

    QOpenGLWidget::keyPressEvent(event);
}

/// Scene 创建

void OpenGLSandboxWidget::buildScene()
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

    /// MyOpenGL Owned Cube

    m_cube = createCube();
    m_resourceManager.add(m_cube);

    /// External Modeling Mesh

    buildExternalMesh();

    /// Diffuse Texture

    m_cubeTexture = createCheckerTexture();
    m_cubeTextureId = m_resourceManager.add(m_cubeTexture);

    /// Material

    m_cubeMaterial = new Material("CubeMaterial");
    m_cubeMaterial->setLit();
    m_cubeMaterial->setBaseColor(QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
    m_cubeMaterial->setSpecular(QVector3D(0.35f, 0.35f, 0.35f), 32.0f);
    m_cubeMaterial->setDiffuseTexture(m_cubeTextureId);
    m_materialManager.add(m_cubeMaterial);

    /// Lighting

    m_lightManager.setAmbientColor(QVector3D(1.0f, 1.0f, 1.0f));
    m_lightManager.setAmbientIntensity(0.18f);

    m_sun = new Light("Sun");
    m_sun->setDirectional(QVector3D(-1.0f, -1.0f, -1.0f));
    m_sun->setColor(QVector3D(1.0f, 1.0f, 1.0f));
    m_sun->setIntensity(0.9f);
    m_lightManager.add(m_sun);

    /// Camera

    Camera* camera = new Camera("MainCamera");
    camera->setPerspective(45.0f, 0.1f, 1000.0f);
    m_cameraManager.add(camera);
}

void OpenGLSandboxWidget::buildExternalMesh()
{
    // 建模库首先创建自己的 Mesh；MyOpenGL 不参与该建模过程。
    m_modelingMesh.buildQuad(1.0);

    m_externalMesh = new ExternalMeshResource("ModelingLibraryMesh", ResourceUpdateDynamic);

    // Adapter 只提供 DataView / Revision / ChangeSet，CPU Mesh 所有权仍然属于 ModelingMesh。
    if (!m_externalMesh->setDataSource(&m_modelingMeshAdapter))
    {
        qWarning() << "OpenGLSandboxWidget buildExternalMesh failed: unable to bind ModelingMeshAdapter.";

        delete m_externalMesh;
        m_externalMesh = 0;
        return;
    }

    m_resourceManager.add(m_externalMesh);
}

MeshResource* OpenGLSandboxWidget::createCube()
{
    MeshResource* cube = new MeshResource("LitCube", ResourceUpdateStatic, MeshPrimitiveTriangles);

    std::vector<MeshVertexAttribute> attributes;

    MeshVertexAttribute positionAttribute;
    positionAttribute.location = 0;
    positionAttribute.componentCount = 3;
    positionAttribute.valueOffset = 0;
    attributes.push_back(positionAttribute);

    MeshVertexAttribute normalAttribute;
    normalAttribute.location = 1;
    normalAttribute.componentCount = 3;
    normalAttribute.valueOffset = 3;
    attributes.push_back(normalAttribute);

    MeshVertexAttribute uvAttribute;
    uvAttribute.location = 2;
    uvAttribute.componentCount = 2;
    uvAttribute.valueOffset = 6;
    attributes.push_back(uvAttribute);

    // Lit Vertex 固定使用 position.xyz + normal.xyz + uv.xy，共 8 个 GLfloat。
    cube->setVertexLayout(8, attributes);

    const GLfloat vertices[] =
    {
        // Front +Z
        -1.0f, -1.0f,  1.0f,    0.0f,  0.0f,  1.0f,    0.0f, 0.0f,
         1.0f, -1.0f,  1.0f,    0.0f,  0.0f,  1.0f,    1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    0.0f,  0.0f,  1.0f,    1.0f, 1.0f,
        -1.0f,  1.0f,  1.0f,    0.0f,  0.0f,  1.0f,    0.0f, 1.0f,

        // Back -Z
         1.0f, -1.0f, -1.0f,    0.0f,  0.0f, -1.0f,    0.0f, 0.0f,
        -1.0f, -1.0f, -1.0f,    0.0f,  0.0f, -1.0f,    1.0f, 0.0f,
        -1.0f,  1.0f, -1.0f,    0.0f,  0.0f, -1.0f,    1.0f, 1.0f,
         1.0f,  1.0f, -1.0f,    0.0f,  0.0f, -1.0f,    0.0f, 1.0f,

        // Right +X
         1.0f, -1.0f,  1.0f,    1.0f,  0.0f,  0.0f,    0.0f, 0.0f,
         1.0f, -1.0f, -1.0f,    1.0f,  0.0f,  0.0f,    1.0f, 0.0f,
         1.0f,  1.0f, -1.0f,    1.0f,  0.0f,  0.0f,    1.0f, 1.0f,
         1.0f,  1.0f,  1.0f,    1.0f,  0.0f,  0.0f,    0.0f, 1.0f,

        // Left -X
        -1.0f, -1.0f, -1.0f,   -1.0f,  0.0f,  0.0f,    0.0f, 0.0f,
        -1.0f, -1.0f,  1.0f,   -1.0f,  0.0f,  0.0f,    1.0f, 0.0f,
        -1.0f,  1.0f,  1.0f,   -1.0f,  0.0f,  0.0f,    1.0f, 1.0f,
        -1.0f,  1.0f, -1.0f,   -1.0f,  0.0f,  0.0f,    0.0f, 1.0f,

        // Top +Y
        -1.0f,  1.0f,  1.0f,    0.0f,  1.0f,  0.0f,    0.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    0.0f,  1.0f,  0.0f,    1.0f, 0.0f,
         1.0f,  1.0f, -1.0f,    0.0f,  1.0f,  0.0f,    1.0f, 1.0f,
        -1.0f,  1.0f, -1.0f,    0.0f,  1.0f,  0.0f,    0.0f, 1.0f,

        // Bottom -Y
        -1.0f, -1.0f, -1.0f,    0.0f, -1.0f,  0.0f,    0.0f, 0.0f,
         1.0f, -1.0f, -1.0f,    0.0f, -1.0f,  0.0f,    1.0f, 0.0f,
         1.0f, -1.0f,  1.0f,    0.0f, -1.0f,  0.0f,    1.0f, 1.0f,
        -1.0f, -1.0f,  1.0f,    0.0f, -1.0f,  0.0f,    0.0f, 1.0f
    };

    const GLuint indices[] =
    {
         0,  1,  2,   0,  2,  3,
         4,  5,  6,   4,  6,  7,
         8,  9, 10,   8, 10, 11,
        12, 13, 14,  12, 14, 15,
        16, 17, 18,  16, 18, 19,
        20, 21, 22,  20, 22, 23
    };

    const int vertexValueCount = sizeof(vertices) / sizeof(vertices[0]);
    const int indexCount = sizeof(indices) / sizeof(indices[0]);

    cube->setVertexData(std::vector<GLfloat>(vertices, vertices + vertexValueCount));
    cube->setIndexData(std::vector<GLuint>(indices, indices + indexCount));

    return cube;
}

MeshResource* OpenGLSandboxWidget::createCameraTargetMarker()
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

TextureResource* OpenGLSandboxWidget::createCheckerTexture()
{
    TextureResource* texture = new TextureResource("CubeCheckerTexture", ResourceUpdateStatic);

    const int textureSize = 128; // 128 × 128 足够观察 UV，同时保持测试纹理数据较小。
    const int cellSize = 16;     // 每个 Checker Cell 为 16 × 16 Pixel，共形成 8 × 8 格。
    QImage image(textureSize, textureSize, QImage::Format_ARGB32);

    for (int y = 0; y < textureSize; ++y)
    {
        for (int x = 0; x < textureSize; ++x)
        {
            const bool alternate = ((x / cellSize) + (y / cellSize)) % 2 != 0;

            if (alternate)
                image.setPixel(x, y, QColor(55, 120, 220, 255).rgba());
            else
                image.setPixel(x, y, QColor(225, 225, 225, 255).rgba());
        }
    }

    texture->setImage(image);
    return texture;
}

/// Scene 状态

void OpenGLSandboxWidget::resetCamera()
{
    Camera* camera = m_cameraManager.activeCamera();

    if (camera == 0)
        return;

    // 初始 Camera 位于 X/Y/Z 正方向，可以同时观察 Cube、External Mesh 和 Grid。
    camera->setView(QVector3D(7.5f, 5.0f, 8.0f), QVector3D(1.0f, 1.0f, 0.0f), QVector3D(0.0f, 1.0f, 0.0f));
    camera->setPerspective(45.0f, 0.1f, 1000.0f);
}

void OpenGLSandboxWidget::updateWindowTitle()
{
    const bool lightEnabled = m_sun != 0 && m_sun->isEnabled();
    const bool textureEnabled = m_cubeMaterial != 0 && m_cubeMaterial->hasDiffuseTexture();
    const Camera* camera = m_cameraManager.activeCamera();

    QString cameraText = "Camera:None";

    if (camera != 0)
    {
        const QVector3D& target = camera->target();

        cameraText = QString("Target:(%1,%2,%3) D:%4 Near:%5")
            .arg(target.x(), 0, 'f', 2)
            .arg(target.y(), 0, 'f', 2)
            .arg(target.z(), 0, 'f', 2)
            .arg(camera->distanceToTarget(), 0, 'f', 3)
            .arg(camera->nearPlane(), 0, 'f', 3);
    }

    const QString revisionText = QString("External S:%1 C:%2")
        .arg(static_cast<qulonglong>(m_modelingMesh.structureRevision()))
        .arg(static_cast<qulonglong>(m_modelingMesh.contentRevision()));

    QString syncText = "GPU:None";

    if (m_externalMesh != 0)
    {
        const ExternalMeshSyncStatistics& statistics = m_externalMesh->syncStatistics();

        syncText = QString("GPU Full:%1 Partial:%2 Last:%3 %4B Total:%5B")
            .arg(static_cast<qulonglong>(statistics.fullSyncCount))
            .arg(static_cast<qulonglong>(statistics.partialSyncCount))
            .arg(externalMeshSyncTypeName(statistics.lastSyncType))
            .arg(static_cast<qulonglong>(statistics.lastUploadedBytes))
            .arg(static_cast<qulonglong>(statistics.totalUploadedBytes));
    }

    setWindowTitle(QString("OpenGL SandBox | %1 | %2 | %3 | M Partial | B Structure | R Reset | F Cube | O Origin | C Target:%4 | G Grid:%5 | A Axis:%6 | V Gizmo:%7 | L Light:%8 | T Texture:%9")
        .arg(cameraText)
        .arg(revisionText)
        .arg(syncText)
        .arg(m_showCameraTarget ? "On" : "Off")
        .arg(m_showGrid ? "On" : "Off")
        .arg(m_showAxes ? "On" : "Off")
        .arg(m_showViewNavigation ? "On" : "Off")
        .arg(lightEnabled ? "On" : "Off")
        .arg(textureEnabled ? "On" : "Off"));
}

/// GPU 生命周期

void OpenGLSandboxWidget::releaseGL()
{
    if (!m_glReady || context() == 0)
        return;

    makeCurrent();

    QOpenGLFunctions_3_3_Core* gl = m_renderContext.gl();

    if (gl != 0)
    {
        m_renderer.release();
        m_resourceManager.releaseGL(gl);
    }

    doneCurrent();
    m_glReady = false;
}