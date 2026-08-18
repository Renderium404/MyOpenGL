#include "OpenGLSandboxWidget.h"

#include <QColor>
#include <QDebug>
#include <QImage>
#include <QKeyEvent>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QThread>

#include <cstdint>
#include <vector>

#include "Camera/Camera.h"
#include "Light/Light.h"
#include "Material/Material.h"
#include "Resource/ExternalGpuGeometry.h"
#include "Resource/ExternalGeometry.h"
#include "Resource/BufferGeometry.h"
#include "Resource/Curve.h"
#include "Resource/Texture.h"
#include "Scene/AxisAlignedBoundingBox.h"
#include "Scene/BufferGeometryPickSource.h"
#include "Scene/PrimitivePicking.h"
#include "Scene/RenderItem.h"
#include "Scene/RenderPart.h"
#include "Scene/RenderPartUpdate.h"

namespace
{

const RenderPartId MultiPartId101 = static_cast<RenderPartId>(101);
const RenderPartId MultiPartId102 = static_cast<RenderPartId>(102);
const RenderPartId MultiPartId103 = static_cast<RenderPartId>(103);
const RenderPartId MultiPartId104 = static_cast<RenderPartId>(104);

const QVector3D MultiPartCenter101(-2.4f, 0.0f, 0.0f);
const QVector3D MultiPartCenter102( 2.4f, 0.0f, 0.0f);
const QVector3D MultiPartCenter103( 0.0f, 0.0f, 0.0f);
const QVector3D MultiPartCenter103Replacement(1.8f, 1.2f, 0.0f);
const QVector3D MultiPartCenter104(3.6f, -0.4f, 0.0f);
const float MultiPartHalfExtent = 0.6f; // 每个验证 Box 边长为 1.2，彼此分离以便独立 Picking。

AxisAlignedBoundingBox multiPartBoxBounds(const QVector3D& center)
{
    const QVector3D extent(MultiPartHalfExtent, MultiPartHalfExtent, MultiPartHalfExtent);
    return AxisAlignedBoundingBox(center - extent, center + extent);
}

BufferGeometry* createMultiPartBoxGeometry(const QString& name, const QVector3D& center)
{
    BufferGeometry* geometry = new BufferGeometry(name, ResourceUpdateStatic, Triangles);

    std::vector<GeometryVertexAttribute> attributes;

    GeometryVertexAttribute positionAttribute;
    positionAttribute.location = 0;
    positionAttribute.componentCount = 3;
    positionAttribute.valueOffset = 0;
    attributes.push_back(positionAttribute);

    GeometryVertexAttribute normalAttribute;
    normalAttribute.location = 1;
    normalAttribute.componentCount = 3;
    normalAttribute.valueOffset = 3;
    attributes.push_back(normalAttribute);

    GeometryVertexAttribute uvAttribute;
    uvAttribute.location = 2;
    uvAttribute.componentCount = 2;
    uvAttribute.valueOffset = 6;
    attributes.push_back(uvAttribute);

    // 与 SandBox LitCube 保持相同 Lit Vertex Layout：position3 + normal3 + uv2。
    geometry->setVertexLayout(8, attributes);

    const float h = MultiPartHalfExtent;
    const float cx = center.x();
    const float cy = center.y();
    const float cz = center.z();

    const GLfloat vertices[] =
    {
        // Front +Z
        cx-h, cy-h, cz+h,    0.0f,  0.0f,  1.0f,    0.0f, 0.0f,
        cx+h, cy-h, cz+h,    0.0f,  0.0f,  1.0f,    1.0f, 0.0f,
        cx+h, cy+h, cz+h,    0.0f,  0.0f,  1.0f,    1.0f, 1.0f,
        cx-h, cy+h, cz+h,    0.0f,  0.0f,  1.0f,    0.0f, 1.0f,

        // Back -Z
        cx+h, cy-h, cz-h,    0.0f,  0.0f, -1.0f,    0.0f, 0.0f,
        cx-h, cy-h, cz-h,    0.0f,  0.0f, -1.0f,    1.0f, 0.0f,
        cx-h, cy+h, cz-h,    0.0f,  0.0f, -1.0f,    1.0f, 1.0f,
        cx+h, cy+h, cz-h,    0.0f,  0.0f, -1.0f,    0.0f, 1.0f,

        // Right +X
        cx+h, cy-h, cz+h,    1.0f,  0.0f,  0.0f,    0.0f, 0.0f,
        cx+h, cy-h, cz-h,    1.0f,  0.0f,  0.0f,    1.0f, 0.0f,
        cx+h, cy+h, cz-h,    1.0f,  0.0f,  0.0f,    1.0f, 1.0f,
        cx+h, cy+h, cz+h,    1.0f,  0.0f,  0.0f,    0.0f, 1.0f,

        // Left -X
        cx-h, cy-h, cz-h,   -1.0f,  0.0f,  0.0f,    0.0f, 0.0f,
        cx-h, cy-h, cz+h,   -1.0f,  0.0f,  0.0f,    1.0f, 0.0f,
        cx-h, cy+h, cz+h,   -1.0f,  0.0f,  0.0f,    1.0f, 1.0f,
        cx-h, cy+h, cz-h,   -1.0f,  0.0f,  0.0f,    0.0f, 1.0f,

        // Top +Y
        cx-h, cy+h, cz+h,    0.0f,  1.0f,  0.0f,    0.0f, 0.0f,
        cx+h, cy+h, cz+h,    0.0f,  1.0f,  0.0f,    1.0f, 0.0f,
        cx+h, cy+h, cz-h,    0.0f,  1.0f,  0.0f,    1.0f, 1.0f,
        cx-h, cy+h, cz-h,    0.0f,  1.0f,  0.0f,    0.0f, 1.0f,

        // Bottom -Y
        cx-h, cy-h, cz-h,    0.0f, -1.0f,  0.0f,    0.0f, 0.0f,
        cx+h, cy-h, cz-h,    0.0f, -1.0f,  0.0f,    1.0f, 0.0f,
        cx+h, cy-h, cz+h,    0.0f, -1.0f,  0.0f,    1.0f, 1.0f,
        cx-h, cy-h, cz+h,    0.0f, -1.0f,  0.0f,    0.0f, 1.0f
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

    geometry->setVertexData(std::vector<GLfloat>(vertices, vertices + sizeof(vertices) / sizeof(vertices[0])));
    geometry->setIndexData(std::vector<GLuint>(indices, indices + sizeof(indices) / sizeof(indices[0])));
    return geometry;
}

bool itemHasTriangleGeometry(const RenderItem* item)
{
    if (item == 0)
        return false;

    for (int partIndex = 0; partIndex < item->partCount(); ++partIndex)
    {
        const RenderPart* currentPart = item->partAt(partIndex);

        if (currentPart != 0 && currentPart->geometry() != 0 &&
            currentPart->geometry()->renderType() == Triangles)
        {
            return true;
        }
    }

    return false;
}

/// SandBox ModelingMesh Primitive Picker。
/// External CPU / GPU 两条路径各自绑定对应 ModelingMesh，因此 Picking 不依赖任何 GPU Readback。
class SandboxModelingMeshPrimitivePicker : public PrimitivePickSource
{
public:
    explicit SandboxModelingMeshPrimitivePicker(const ModelingMesh* mesh)
        : m_mesh(mesh)
    {
    }

    bool pickPrimitive(const PrimitivePickContext& context, PrimitivePickHit& hit) const override
    {
        if (m_mesh == 0)
            return false;

        const std::vector<ModelingPoint>& positions = m_mesh->positions();
        const std::vector<std::uint32_t>& indices = m_mesh->indices();

        if (positions.empty() || indices.empty())
            return false;

        TrianglePickView view;
        view.vertexData = &positions[0];
        view.vertexByteSize = positions.size() * sizeof(ModelingPoint);
        view.vertexCount = static_cast<int>(positions.size());
        view.vertexStride = sizeof(ModelingPoint);
        view.positionByteOffset = 0;
        view.positionType = GL_DOUBLE;
        view.indexData = &indices[0];
        view.indexByteSize = indices.size() * sizeof(std::uint32_t);
        view.indexCount = static_cast<int>(indices.size());
        view.indexType = GL_UNSIGNED_INT;

        return raycastTriangles(view, context.rayOrigin, context.rayDirection, hit);
    }

    bool pickPoint(const PrimitivePickContext& context, PointPickHit& hit) const override
    {
        if (m_mesh == 0)
            return false;

        const std::vector<ModelingPoint>& positions = m_mesh->positions();
        const std::vector<std::uint32_t>& indices = m_mesh->indices();

        if (positions.empty() || indices.empty())
            return false;

        PointPickView view;
        view.vertexData = &positions[0];
        view.vertexByteSize = positions.size() * sizeof(ModelingPoint);
        view.vertexCount = static_cast<int>(positions.size());
        view.vertexStride = sizeof(ModelingPoint);
        view.positionByteOffset = 0;
        view.positionType = GL_DOUBLE;
        view.indexData = &indices[0];
        view.indexByteSize = indices.size() * sizeof(std::uint32_t);
        view.indexCount = static_cast<int>(indices.size());
        view.indexType = GL_UNSIGNED_INT;

        return pickPoints(view, context, hit);
    }

private:
    const ModelingMesh* m_mesh; // 借用 Application Modeling CPU Geometry；Vector 重建后每次点击重新获取当前地址。
};



/// SandBox 专用故障注入 Resource。
/// 第一次 GPU 初始化会在真实创建 Buffer 后故意失败，用于验证 Resource::initializeGL() 的部分初始化回滚。
class SandboxFaultResource : public Resource
{
public:
    SandboxFaultResource()
        // 该对象只测试 Resource GPU 生命周期；使用 Geometry 类别，不参与 Renderer Draw。
        : Resource("SandboxFaultResource", ResourceTypeGeometry, ResourceUpdateStatic)
        , m_buffer(0)
        , m_initializeAttempt(0)
        , m_failNextInitialize(true)
    {
        markFullDirty();
    }

    ~SandboxFaultResource() override
    {
        if (m_buffer != 0)
            qWarning() << "SandboxFaultResource destroyed while test GPU Buffer is still alive:" << m_buffer;
    }

    /// 调试状态
    GLuint bufferId() const
    {
        return m_buffer;
    }

protected:
    /// GPU 实现
    bool onInitializeGL(QOpenGLFunctions_3_3_Core* gl) override
    {
        ++m_initializeAttempt;

        if (m_buffer != 0)
        {
            qWarning() << "SandboxFaultResource initialize failed: previous GPU Buffer was not released:" << m_buffer;
            return false;
        }

        gl->glGenBuffers(1, &m_buffer);

        if (m_buffer == 0)
        {
            qWarning() << "SandboxFaultResource initialize failed: glGenBuffers returned zero.";
            return false;
        }

        // glGenBuffers() 只分配 Object Name。
        // 第一次 Bind + BufferData 后才确保这个 Name 对应一个真实 Buffer Object，可由 glIsBuffer() 验证。
        const GLfloat testData[] =
        {
            0.0f, 1.0f, 2.0f, 3.0f
        };

        gl->glBindBuffer(GL_COPY_WRITE_BUFFER, m_buffer);
        gl->glBufferData(GL_COPY_WRITE_BUFFER, sizeof(testData), testData, GL_STATIC_DRAW);
        gl->glBindBuffer(GL_COPY_WRITE_BUFFER, 0);

        if (gl->glIsBuffer(m_buffer) != GL_TRUE)
        {
            qWarning() << "SandboxFaultResource initialize failed: generated Buffer is not a valid OpenGL Buffer:" << m_buffer;
            return false;
        }

        qDebug() << "SandboxFaultResource GPU Buffer created:"
                 << "Attempt=" << m_initializeAttempt
                 << "Buffer=" << m_buffer;

        if (m_failNextInitialize)
        {
            // 故意让第一次初始化在已经创建真实 GPU Object 后失败。
            // Resource::initializeGL() 应立即调用 onReleaseGL() 回滚这个 Buffer。
            m_failNextInitialize = false;

            qDebug() << "SandboxFaultResource intentional initialization failure:"
                     << "Attempt=" << m_initializeAttempt
                     << "Buffer=" << m_buffer;

            return false;
        }

        qDebug() << "SandboxFaultResource initialization succeeded:"
                 << "Attempt=" << m_initializeAttempt
                 << "Buffer=" << m_buffer;

        return true;
    }

    bool onUpdateFullGL(QOpenGLFunctions_3_3_Core* gl) override
    {
        Q_UNUSED(gl);

        // 本测试只验证 Initialization Transaction，不测试 Update。
        return true;
    }

    bool onUpdatePartialGL(QOpenGLFunctions_3_3_Core* gl) override
    {
        Q_UNUSED(gl);

        // 本测试只验证 Initialization Transaction，不测试 Update。
        return true;
    }

    void onReleaseGL(QOpenGLFunctions_3_3_Core* gl) override
    {
        if (m_buffer == 0)
            return;

        const GLuint releasedBuffer = m_buffer;

        gl->glDeleteBuffers(1, &m_buffer);
        m_buffer = 0;

        const bool stillValid = gl->glIsBuffer(releasedBuffer) == GL_TRUE;

        qDebug() << "SandboxFaultResource GPU Buffer released:"
                 << "Buffer=" << releasedBuffer
                 << "StillValidAfterDelete=" << stillValid;
    }

private:
    GLuint m_buffer;          // 当前测试 Buffer；第一次初始化失败后必须被自动回滚为 0。
    int m_initializeAttempt;  // 当前 Resource 生命周期内累计初始化次数。
    bool m_failNextInitialize; // 第一次 initializeGL() 是否执行故意失败。
};

/// 验证 Resource 初始化失败后的事务回滚和再次初始化能力。
bool runResourceInitializationRollbackTest(QOpenGLFunctions_3_3_Core* gl)
{
    if (gl == 0)
    {
        qWarning() << "Resource rollback test failed: OpenGL functions are null.";
        return false;
    }

    qDebug() << "========== Resource Initialization Rollback Test Begin ==========";

    // 使用完全独立的临时 ResourceManager。
    // 测试失败不会污染 SandBox 主 Scene 的 ResourceManager 和绘制状态。
    ResourceManager testResourceManager;

    SandboxFaultResource* faultResource = new SandboxFaultResource();
    const ResourceId resourceId = testResourceManager.add(faultResource);

    if (resourceId == InvalidResourceId)
    {
        qWarning() << "Resource rollback test failed: unable to add SandboxFaultResource.";
        delete faultResource;
        return false;
    }

    /// 第一次初始化：必须故意失败并自动回滚

    const bool firstInitializeResult = testResourceManager.syncResource(resourceId, gl);

    if (firstInitializeResult)
    {
        qWarning() << "Resource rollback test failed: first initialization unexpectedly succeeded.";

        testResourceManager.releaseGL(gl);
        testResourceManager.clear();
        return false;
    }

    if (faultResource->isInitialized())
    {
        qWarning() << "Resource rollback test failed: Resource remained initialized after failed initialization.";

        testResourceManager.releaseGL(gl);
        testResourceManager.clear();
        return false;
    }

    if (faultResource->bufferId() != 0)
    {
        qWarning() << "Resource rollback test failed: partial GPU Buffer survived rollback:"
                   << faultResource->bufferId();

        // 此状态表示 Resource 基类回滚契约已经失败。
        // isInitialized()==false 时 ResourceManager 无法再通过标准 releaseGL() 清理该半初始化对象，
        // SandboxFaultResource 析构时会额外给出泄漏警告。
        testResourceManager.clear();
        return false;
    }

    if (faultResource->dirtyState() != ResourceFullDirty)
    {
        qWarning() << "Resource rollback test failed: failed Resource did not return to FullDirty:"
                   << resourceDirtyStateName(faultResource->dirtyState());

        testResourceManager.clear();
        return false;
    }

    qDebug() << "Resource rollback test first failure verified:"
             << "Initialized=" << faultResource->isInitialized()
             << "DirtyState=" << resourceDirtyStateName(faultResource->dirtyState())
             << "Buffer=" << faultResource->bufferId();

    /// 第二次初始化：同一个 Resource 必须能够重新成功初始化

    const bool secondInitializeResult = testResourceManager.syncResource(resourceId, gl);

    if (!secondInitializeResult)
    {
        qWarning() << "Resource rollback test failed: second initialization did not recover.";

        testResourceManager.releaseGL(gl);
        testResourceManager.clear();
        return false;
    }

    if (!faultResource->isInitialized())
    {
        qWarning() << "Resource rollback test failed: Resource is not initialized after successful retry.";

        testResourceManager.releaseGL(gl);
        testResourceManager.clear();
        return false;
    }

    const GLuint successfulBuffer = faultResource->bufferId();

    if (successfulBuffer == 0 || gl->glIsBuffer(successfulBuffer) != GL_TRUE)
    {
        qWarning() << "Resource rollback test failed: retry did not create a valid GPU Buffer:"
                   << successfulBuffer;

        testResourceManager.releaseGL(gl);
        testResourceManager.clear();
        return false;
    }

    if (faultResource->dirtyState() != ResourceClean)
    {
        qWarning() << "Resource rollback test failed: successful initialization did not leave Resource Clean:"
                   << resourceDirtyStateName(faultResource->dirtyState());

        testResourceManager.releaseGL(gl);
        testResourceManager.clear();
        return false;
    }

    qDebug() << "Resource rollback test retry verified:"
             << "Initialized=" << faultResource->isInitialized()
             << "DirtyState=" << resourceDirtyStateName(faultResource->dirtyState())
             << "Buffer=" << successfulBuffer;

    /// 正常释放：成功初始化后的 GPU Object 必须再次完整释放

    if (!testResourceManager.releaseGL(gl))
    {
        qWarning() << "Resource rollback test failed: normal Resource release failed.";

        testResourceManager.clear();
        return false;
    }

    if (faultResource->isInitialized())
    {
        qWarning() << "Resource rollback test failed: Resource remained initialized after releaseGL().";

        testResourceManager.clear();
        return false;
    }

    if (faultResource->bufferId() != 0)
    {
        qWarning() << "Resource rollback test failed: GPU Buffer remained after releaseGL():"
                   << faultResource->bufferId();

        testResourceManager.clear();
        return false;
    }

    if (gl->glIsBuffer(successfulBuffer) == GL_TRUE)
    {
        qWarning() << "Resource rollback test failed: released GPU Buffer is still valid:"
                   << successfulBuffer;

        testResourceManager.clear();
        return false;
    }

    qDebug() << "Resource rollback test final release verified:"
             << "Initialized=" << faultResource->isInitialized()
             << "Buffer=" << faultResource->bufferId();

    // Resource 已经没有 GPU State，因此临时 Manager 可以只执行 CPU Object 清理。
    testResourceManager.clear();

    qDebug() << "========== Resource Initialization Rollback Test Passed ==========";

    return true;
}

}

OpenGLSandboxWidget::OpenGLSandboxWidget(QWidget* parent)
    : OpenGLViewerWidget(parent)
    , m_modelingMeshAdapter(&m_modelingMesh)
    , m_modelingGpuMesh(&m_modelingGpuSourceMesh)
    , m_modelingGpuMeshAdapter(&m_modelingGpuMesh, &m_modelingGpuMeshSync)
    , m_externalGpuContext(0)
    , m_externalGpuSurface(0)
    , m_cube(0)
    , m_curve(0)
    , m_multiPartGeometry101(0)
    , m_multiPartGeometry102(0)
    , m_multiPartGeometry103(0)
    , m_multiPartGeometry103Replacement(0)
    , m_multiPartGeometry104(0)
    , m_externalGeometry(0)
    , m_externalGpuGeometry(0)
    , m_cubeTexture(0)
    , m_cubeMaterial(0)
    , m_curveMaterial(0)
    , m_sun(0)
    , m_fillLight(0)
    , m_pointLight(0)
    , m_spotLight(0)
    , m_cubeTextureId(InvalidResourceId)
    , m_cubeItem(0)
    , m_curveItem(0)
    , m_multiPartItem(0)
    , m_externalGeometryItem(0)
    , m_externalGpuGeometryItem(0)
    , m_cubePrimitivePicker(0)
    , m_curvePrimitivePicker(0)
    , m_multiPartPicker101(0)
    , m_multiPartPicker102(0)
    , m_multiPartPicker103(0)
    , m_multiPartPicker103Replacement(0)
    , m_multiPartPicker104(0)
    , m_externalCpuPrimitivePicker(0)
    , m_externalGpuPrimitivePicker(0)
    , m_externalWide(false)
    , m_externalSplit(false)
    , m_lightPreset(1)
    , m_multiPartState(0)
{
    buildSandboxContent();
    updateWindowTitle();
}

OpenGLSandboxWidget::~OpenGLSandboxWidget()
{
    // 必须在派生类成员析构前让 Base Viewer 释放 MyOpenGL VAO / Resource；
    // releaseViewerGL() 随后会通过 afterViewerGLReleased() 停止 External GPU Worker。
    releaseViewerGL();

    // RenderItem 只借用 Primitive Picker；先清除 Scene 借用关系，再释放 SandBox 自己拥有的 Picker Adapter。
    scene().clear();

    delete m_cubePrimitivePicker;
    delete m_curvePrimitivePicker;
    delete m_multiPartPicker101;
    delete m_multiPartPicker102;
    delete m_multiPartPicker103;
    delete m_multiPartPicker103Replacement;
    delete m_multiPartPicker104;
    delete m_externalCpuPrimitivePicker;
    delete m_externalGpuPrimitivePicker;

    m_cubePrimitivePicker = 0;
    m_curvePrimitivePicker = 0;
    m_multiPartPicker101 = 0;
    m_multiPartPicker102 = 0;
    m_multiPartPicker103 = 0;
    m_multiPartPicker103Replacement = 0;
    m_multiPartPicker104 = 0;
    m_externalCpuPrimitivePicker = 0;
    m_externalGpuPrimitivePicker = 0;
}

/// Viewer 扩展

bool OpenGLSandboxWidget::initializeViewerContentGL(QOpenGLFunctions_3_3_Core* gl)
{
    Q_UNUSED(gl);

    // External GPU Geometry 使用真正的第二 OpenGL Context，并在独立 Worker Thread 中拥有和更新 VBO / EBO。
    if (!initializeExternalGpuGeometry())
    {
        qWarning() << "OpenGLSandboxWidget initializeViewerContentGL failed: External GPU Geometry initialization failed.";
        return false;
    }

    return true;
}

void OpenGLSandboxWidget::buildViewerContentItems()
{
    m_cubeItem = 0;
    m_curveItem = 0;
    m_multiPartItem = 0;
    m_externalGeometryItem = 0;
    m_externalGpuGeometryItem = 0;

    if (m_cube != 0 && m_cubeMaterial != 0)
    {
        m_cubeItem = scene().createItem("LitCubeItem");
        m_cubeItem->setGeometry(m_cube);
        m_cubeItem->setMaterial(m_cubeMaterial);
        m_cubeItem->setPrimitivePickSource(m_cubePrimitivePicker);

        // Cube 边长为 2，局部 Bounds 固定为 [-1, +1]；向上移动 1 个单位后底面位于 Y=0。
        m_cubeItem->setLocalBounds(AxisAlignedBoundingBox(QVector3D(-1.0f, -1.0f, -1.0f), QVector3D(1.0f, 1.0f, 1.0f)));
        m_cubeItem->transform().setPosition(QVector3D(0.0f, 1.0f, 0.0f));
        addPickCandidate(m_cubeItem);
    }

    if (m_curve != 0 && m_curveMaterial != 0)
    {
        m_curveItem = scene().createItem("CurveItem");
        m_curveItem->setGeometry(m_curve);
        m_curveItem->setMaterial(m_curveMaterial);
        m_curveItem->setPrimitivePickSource(m_curvePrimitivePicker);

        AxisAlignedBoundingBox curveBounds;
        const std::vector<QVector3D>& points = m_curve->points();

        for (std::size_t pointIndex = 0; pointIndex < points.size(); ++pointIndex)
            curveBounds.expandToInclude(points[pointIndex]);

        m_curveItem->setLocalBounds(curveBounds);
        addPickCandidate(m_curveItem);
    }

    buildMultiPartValidationItem();

    if (m_externalGeometry != 0 && m_cubeMaterial != 0)
    {
        m_externalGeometryItem = scene().createItem("ExternalCpuGeometryItem");
        m_externalGeometryItem->setGeometry(m_externalGeometry);
        m_externalGeometryItem->setMaterial(m_cubeMaterial);
        m_externalGeometryItem->setPrimitivePickSource(m_externalCpuPrimitivePicker);
        m_externalGeometryItem->transform().setPosition(QVector3D(3.5f, 0.0f, 0.0f));
        addPickCandidate(m_externalGeometryItem);
    }

    if (m_externalGpuGeometry != 0 && m_cubeMaterial != 0)
    {
        m_externalGpuGeometryItem = scene().createItem("ExternalGpuGeometryItem");
        m_externalGpuGeometryItem->setGeometry(m_externalGpuGeometry);
        m_externalGpuGeometryItem->setMaterial(m_cubeMaterial);
        m_externalGpuGeometryItem->setPrimitivePickSource(m_externalGpuPrimitivePicker);
        m_externalGpuGeometryItem->transform().setPosition(QVector3D(-3.5f, 0.0f, 0.0f));
        addPickCandidate(m_externalGpuGeometryItem);
    }

    // SandBox 当前 User Scene 包含四个原有测试 Item 和一个显式 MultiPartItem，并全部注册为 Picking Candidate。
    // Viewer Grid / Axis / Camera Target / Highlight / ViewNavigation 不进入 Scene；Scene Bounds 只聚合用户 Item。
    updateExternalRenderItemBounds();
}

bool OpenGLSandboxWidget::handleViewerKeyPress(QKeyEvent* event)
{
    if (event->key() == Qt::Key_L)
    {
        m_lightPreset = (m_lightPreset + 1) % 5;
        applyLightPreset();

        qDebug() << "OpenGLSandboxWidget Light Preset changed:"
                 << lightPresetName();

        updateWindowTitle();
        update();
        return true;
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
        return true;
    }

    if (event->key() == Qt::Key_M)
    {
        // 两份 ModelingMesh 同时执行完全相同的 Content Change。
        if (applyMatchedContentChange())
            update();

        return true;
    }

    if (event->key() == Qt::Key_B)
    {
        // Width 是独立测试维度：Topology 保持当前状态，只在 Narrow / Wide 之间切换。
        m_externalWide = !m_externalWide;

        if (rebuildMatchedExternalMeshes())
            update();

        updateWindowTitle();
        return true;
    }

    if (event->key() == Qt::Key_K)
    {
        // Topology 是独立测试维度：Width 保持当前状态，只在 Quad / Split Quad 之间切换。
        m_externalSplit = !m_externalSplit;

        if (rebuildMatchedExternalMeshes())
            update();

        updateWindowTitle();
        return true;
    }

    if (event->key() == Qt::Key_P)
    {
        // ModelingMesh 完全不变，只让 External GPU Library 替换自己的整套 VBO / EBO。
        if (replaceExternalGpuBuffers())
            update();

        return true;
    }

    if (event->key() == Qt::Key_W)
    {
        RenderItem* currentPickedItem = pickedItem();

        if (currentPickedItem == 0)
        {
            qDebug() << "OpenGLSandboxWidget Display Mode ignored: no item is currently picked.";
            return true;
        }

        if (!itemHasTriangleGeometry(currentPickedItem))
        {
            qDebug() << "OpenGLSandboxWidget Display Mode ignored: at least one Triangle RenderPart is required:"
                     << currentPickedItem->name();
            return true;
        }

        RenderItemDisplayMode nextMode = RenderItemDisplayShaded;

        switch (currentPickedItem->displayMode())
        {
        case RenderItemDisplayShaded:
            nextMode = RenderItemDisplayWireframe;
            break;

        case RenderItemDisplayWireframe:
            nextMode = RenderItemDisplayShadedWithEdges;
            break;

        case RenderItemDisplayShadedWithEdges:
            nextMode = RenderItemDisplayShaded;
            break;
        }

        if (!currentPickedItem->setDisplayMode(nextMode))
            return true;

        qDebug() << "OpenGLSandboxWidget Display Mode changed:"
                 << "Item=" << currentPickedItem->name()
                 << "Mode=" << renderItemDisplayModeName(nextMode);

        updateWindowTitle();
        update();
        return true;
    }

    if (event->key() == Qt::Key_J)
    {
        RenderItem* currentPickedItem = pickedItem();

        if (currentPickedItem == 0)
        {
            qDebug() << "OpenGLSandboxWidget Hide Picked ignored: no item is currently picked.";
            return true;
        }

        setItemVisible(currentPickedItem, false);
        return true;
    }

    if (event->key() == Qt::Key_I)
    {
        RenderItem* currentPickedItem = pickedItem();

        if (currentPickedItem == 0)
        {
            qDebug() << "OpenGLSandboxWidget Isolate Picked ignored: no item is currently picked.";
            return true;
        }

        std::vector<RenderItem*> items;
        items.push_back(currentPickedItem);
        isolateItems(items);
        return true;
    }

    if (event->key() == Qt::Key_U)
    {
        setAllItemsVisible(true);
        return true;
    }

    if (event->key() == Qt::Key_N)
    {
        cycleMultiPartValidation();
        return true;
    }

    if (event->key() == Qt::Key_X)
    {
        // 故障注入测试使用独立临时 ResourceManager，不修改当前 Viewer Scene Resource。
        QOpenGLFunctions_3_3_Core* gl = renderContext().gl();

        if (!runResourceInitializationRollbackTest(gl))
            qWarning() << "OpenGLSandboxWidget Resource Initialization Rollback Test failed.";

        return true;
    }

    return false;
}

void OpenGLSandboxWidget::viewerStateChanged()
{
    updateWindowTitle();
}

void OpenGLSandboxWidget::afterViewerGLReleased()
{
    // Base Viewer 已先释放 MyOpenGL Resource / VAO，此时才能停止 Worker 并释放 External VBO / EBO。
    shutdownExternalGpuWorker();
}

/// SandBox Scene 创建

void OpenGLSandboxWidget::buildSandboxContent()
{
    /// MyOpenGL Owned Cube

    m_cube = createCube();
    resourceManager().add(m_cube);

    /// MyOpenGL Owned Curve

    m_curve = createDemoCurve();
    resourceManager().add(m_curve);

    /// Explicit Multi-Part Item Geometry
    // 五个 Geometry 全部预注册，Part 结构变化时不引入新的 Resource 首次初始化变量。
    m_multiPartGeometry101 = createMultiPartBoxGeometry("MultiPartGeometry101", MultiPartCenter101);
    m_multiPartGeometry102 = createMultiPartBoxGeometry("MultiPartGeometry102", MultiPartCenter102);
    m_multiPartGeometry103 = createMultiPartBoxGeometry("MultiPartGeometry103", MultiPartCenter103);
    m_multiPartGeometry103Replacement = createMultiPartBoxGeometry("MultiPartGeometry103Replacement", MultiPartCenter103Replacement);
    m_multiPartGeometry104 = createMultiPartBoxGeometry("MultiPartGeometry104", MultiPartCenter104);

    resourceManager().add(m_multiPartGeometry101);
    resourceManager().add(m_multiPartGeometry102);
    resourceManager().add(m_multiPartGeometry103);
    resourceManager().add(m_multiPartGeometry103Replacement);
    resourceManager().add(m_multiPartGeometry104);

    /// External CPU / GPU Modeling Sources

    buildExternalGeometry();

    /// Primitive Picking Sources

    // BufferGeometry Picker 已经成为 MyOpenGL 通用 Adapter；External 两条 Modeling Path 仍由 SandBox Adapter 验证。
    m_cubePrimitivePicker = new BufferGeometryPickSource(m_cube);
    m_curvePrimitivePicker = new BufferGeometryPickSource(m_curve);
    m_multiPartPicker101 = new BufferGeometryPickSource(m_multiPartGeometry101);
    m_multiPartPicker102 = new BufferGeometryPickSource(m_multiPartGeometry102);
    m_multiPartPicker103 = new BufferGeometryPickSource(m_multiPartGeometry103);
    m_multiPartPicker103Replacement = new BufferGeometryPickSource(m_multiPartGeometry103Replacement);
    m_multiPartPicker104 = new BufferGeometryPickSource(m_multiPartGeometry104);
    m_externalCpuPrimitivePicker = new SandboxModelingMeshPrimitivePicker(&m_modelingMesh);
    m_externalGpuPrimitivePicker = new SandboxModelingMeshPrimitivePicker(&m_modelingGpuSourceMesh);

    /// Diffuse Texture

    m_cubeTexture = createCheckerTexture();
    m_cubeTextureId = resourceManager().add(m_cubeTexture);

    /// Material

    m_cubeMaterial = new Material("CubeMaterial");
    m_cubeMaterial->setLit();
    m_cubeMaterial->setBaseColor(QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
    m_cubeMaterial->setSpecular(QVector3D(0.35f, 0.35f, 0.35f), 32.0f);
    m_cubeMaterial->setDiffuseTexture(m_cubeTextureId);
    materialManager().add(m_cubeMaterial);

    m_curveMaterial = new Material("CurveMaterial");
    m_curveMaterial->setVertexColor();
    materialManager().add(m_curveMaterial);

    /// Lighting

    // 环境光只负责提供最低可见度；主要立体感由多光源 Diffuse 贡献产生。
    lightManager().setAmbientColor(QVector3D(1.0f, 1.0f, 1.0f));
    lightManager().setAmbientIntensity(0.08f);

    m_sun = new Light("Sun");
    m_sun->setDirectional(QVector3D(-1.0f, -1.0f, -1.0f));
    m_sun->setColor(QVector3D(1.0f, 0.98f, 0.94f));
    m_sun->setIntensity(0.80f);
    lightManager().add(m_sun);

    // 与主光方向明显不同的弱补光，避免背光面完全压暗，同时保持明暗层次。
    m_fillLight = new Light("Fill");
    m_fillLight->setDirectional(QVector3D(1.0f, -0.35f, 0.8f));
    m_fillLight->setColor(QVector3D(0.82f, 0.90f, 1.0f));
    m_fillLight->setIntensity(0.28f);
    lightManager().add(m_fillLight);

    // Point Light 用于验证 Position + Range Diffuse Attenuation。
    m_pointLight = new Light("Point");
    m_pointLight->setPoint(QVector3D(0.0f, 4.5f, 3.5f), 8.5f);
    m_pointLight->setColor(QVector3D(1.0f, 0.78f, 0.58f));
    m_pointLight->setIntensity(0.50f);
    lightManager().add(m_pointLight);

    // Spot Light 使用现有半锥角定义，验证 Range + Cone Diffuse Attenuation。
    m_spotLight = new Light("Spot");
    m_spotLight->setSpot(QVector3D(0.0f, 6.0f, 6.0f), QVector3D(0.0f, -0.65f, -1.0f), 14.0f, 18.0f, 32.0f);
    m_spotLight->setColor(QVector3D(0.62f, 0.78f, 1.0f));
    m_spotLight->setIntensity(0.65f);
    lightManager().add(m_spotLight);

    applyLightPreset();
}

/// Multi-Part RenderItem Test

void OpenGLSandboxWidget::buildMultiPartValidationItem()
{
    if (m_cubeMaterial == 0 || m_multiPartGeometry101 == 0 || m_multiPartGeometry102 == 0 ||
        m_multiPartGeometry103 == 0 || m_multiPartPicker101 == 0 || m_multiPartPicker102 == 0 ||
        m_multiPartPicker103 == 0)
    {
        qWarning() << "OpenGLSandboxWidget buildMultiPartValidationItem failed: validation resources are incomplete.";
        return;
    }

    m_multiPartItem = scene().createItem("MultiPartItem");
    m_multiPartItem->setMaterial(m_cubeMaterial);

    std::vector<RenderPartUpdate> initialUpdates;
    initialUpdates.push_back(RenderPartUpdate::replacement(MultiPartId101, m_multiPartGeometry101));
    initialUpdates.push_back(RenderPartUpdate::replacement(MultiPartId102, m_multiPartGeometry102));
    initialUpdates.push_back(RenderPartUpdate::replacement(MultiPartId103, m_multiPartGeometry103));

    if (!m_multiPartItem->applyPartUpdates(initialUpdates))
    {
        qWarning() << "OpenGLSandboxWidget buildMultiPartValidationItem failed: baseline RenderPartUpdate batch was rejected.";
        return;
    }

    // Picking / Bounds 是测试所需的可选能力，基础 RenderPartUpdate 不要求调用方提供。
    m_multiPartItem->part(MultiPartId101)->setPrimitivePickSource(m_multiPartPicker101);
    m_multiPartItem->part(MultiPartId101)->setLocalBounds(multiPartBoxBounds(MultiPartCenter101));
    m_multiPartItem->part(MultiPartId102)->setPrimitivePickSource(m_multiPartPicker102);
    m_multiPartItem->part(MultiPartId102)->setLocalBounds(multiPartBoxBounds(MultiPartCenter102));
    m_multiPartItem->part(MultiPartId103)->setPrimitivePickSource(m_multiPartPicker103);
    m_multiPartItem->part(MultiPartId103)->setLocalBounds(multiPartBoxBounds(MultiPartCenter103));

    // 整个 MultiPartItem 使用一个 Item Transform，三个 Geometry 都位于 Item Local Space。
    m_multiPartItem->transform().setPosition(QVector3D(0.0f, -2.0f, 0.0f));
    addPickCandidate(m_multiPartItem);
    m_multiPartState = 0;

    const AxisAlignedBoundingBox bounds = m_multiPartItem->worldBounds();

    qDebug() << "OpenGLSandboxWidget Multi-Part Item built through minimal RenderPartUpdate:"
             << "Item=" << m_multiPartItem->name()
             << "Parts=" << m_multiPartItem->partCount()
             << "Part101ResourceId=" << m_multiPartGeometry101->id()
             << "Part102ResourceId=" << m_multiPartGeometry102->id()
             << "Part103ResourceId=" << m_multiPartGeometry103->id()
             << "BoundsMinimum=" << bounds.minimum()
             << "BoundsMaximum=" << bounds.maximum();
}

bool OpenGLSandboxWidget::cycleMultiPartValidation()
{
    if (m_multiPartItem == 0)
    {
        qWarning() << "OpenGLSandboxWidget Multi-Part mutation failed: MultiPartItem does not exist.";
        return false;
    }

    // Primitive / Bounds Highlight 可能引用即将移除或替换的 Part 几何，结构变化前主动清除交互缓存。
    if (pickedItem() == m_multiPartItem)
        clearPickedItem();

    clearBoundsHighlight();
    clearPrimitiveHighlight();

    switch (m_multiPartState)
    {
    case 0:
        if (!m_multiPartItem->applyPartUpdate(RenderPartUpdate::removal(MultiPartId102)))
            return false;

        m_multiPartState = 1;
        break;

    case 1:
        if (!m_multiPartItem->applyPartUpdate(
                RenderPartUpdate::replacement(MultiPartId103, m_multiPartGeometry103Replacement)))
        {
            return false;
        }

        m_multiPartItem->part(MultiPartId103)->setPrimitivePickSource(m_multiPartPicker103Replacement);
        m_multiPartItem->part(MultiPartId103)->setLocalBounds(multiPartBoxBounds(MultiPartCenter103Replacement));
        m_multiPartState = 2;
        break;

    case 2:
        if (!m_multiPartItem->applyPartUpdate(
                RenderPartUpdate::replacement(MultiPartId104, m_multiPartGeometry104)))
        {
            return false;
        }

        m_multiPartItem->part(MultiPartId104)->setPrimitivePickSource(m_multiPartPicker104);
        m_multiPartItem->part(MultiPartId104)->setLocalBounds(multiPartBoxBounds(MultiPartCenter104));
        m_multiPartState = 3;
        break;

    case 3:
        if (!resetMultiPartValidation())
            return false;
        break;

    default:
        qWarning() << "OpenGLSandboxWidget Multi-Part mutation failed: unsupported state:" << m_multiPartState;
        return false;
    }

    const RenderPart* unchangedPart101 = m_multiPartItem->part(MultiPartId101);
    const AxisAlignedBoundingBox bounds = m_multiPartItem->worldBounds();

    qDebug() << "OpenGLSandboxWidget Multi-Part state changed through minimal RenderPartUpdate:"
             << "State=" << multiPartStateName()
             << "Parts=" << m_multiPartItem->partCount()
             << "Part101GeometryUnchanged=" << (unchangedPart101 != 0 && unchangedPart101->geometry() == m_multiPartGeometry101)
             << "Part101ResourceId=" << m_multiPartGeometry101->id()
             << "Part101VAO=" << m_multiPartGeometry101->vao()
             << "BoundsMinimum=" << bounds.minimum()
             << "BoundsMaximum=" << bounds.maximum();

    viewerStateChanged();
    update();
    return true;
}

bool OpenGLSandboxWidget::resetMultiPartValidation()
{
    if (m_multiPartItem == 0)
        return false;

    std::vector<RenderPartUpdate> updates;
    updates.push_back(RenderPartUpdate::replacement(MultiPartId101, m_multiPartGeometry101));
    updates.push_back(RenderPartUpdate::replacement(MultiPartId102, m_multiPartGeometry102));
    updates.push_back(RenderPartUpdate::replacement(MultiPartId103, m_multiPartGeometry103));
    updates.push_back(RenderPartUpdate::removal(MultiPartId104));

    if (!m_multiPartItem->applyPartUpdates(updates))
        return false;

    // Reset 后按需恢复测试用 Picking / Bounds；未进入基础 Update 接口。
    m_multiPartItem->part(MultiPartId101)->setPrimitivePickSource(m_multiPartPicker101);
    m_multiPartItem->part(MultiPartId101)->setLocalBounds(multiPartBoxBounds(MultiPartCenter101));
    m_multiPartItem->part(MultiPartId102)->setPrimitivePickSource(m_multiPartPicker102);
    m_multiPartItem->part(MultiPartId102)->setLocalBounds(multiPartBoxBounds(MultiPartCenter102));
    m_multiPartItem->part(MultiPartId103)->setPrimitivePickSource(m_multiPartPicker103);
    m_multiPartItem->part(MultiPartId103)->setLocalBounds(multiPartBoxBounds(MultiPartCenter103));

    m_multiPartState = 0;
    return true;
}

const char* OpenGLSandboxWidget::multiPartStateName() const
{
    switch (m_multiPartState)
    {
    case 0:
        return "Base";
    case 1:
        return "Remove102";
    case 2:
        return "Replace103";
    case 3:
        return "Add104";
    }

    return "Unknown";
}

void OpenGLSandboxWidget::buildExternalGeometry()
{
    // 两条 External Path 从完全相同的 ModelingMesh 数据开始。
    m_modelingMesh.buildQuad(1.0);
    m_modelingGpuSourceMesh.buildQuad(1.0);

    verifyModelingMeshConsistency("Initial Build");

    m_externalGeometry = new ExternalGeometry("ModelingLibraryCpuGeometry", ResourceUpdateDynamic);

    // CPU Adapter 只提供 DataView / Revision / ChangeSet，CPU Mesh 所有权仍然属于 ModelingMesh。
    if (!m_externalGeometry->setDataSource(&m_modelingMeshAdapter))
    {
        qWarning() << "OpenGLSandboxWidget buildExternalGeometry failed: unable to bind ModelingMeshAdapter.";

        delete m_externalGeometry;
        m_externalGeometry = 0;
        return;
    }

    resourceManager().add(m_externalGeometry);
}


void OpenGLSandboxWidget::updateExternalRenderItemBounds()
{
    AxisAlignedBoundingBox bounds;
    const std::vector<ModelingPoint>& positions = m_modelingMesh.positions();

    for (std::size_t i = 0; i < positions.size(); ++i)
    {
        const ModelingPoint& point = positions[i];
        bounds.expandToInclude(QVector3D(static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z)));
    }

    if (m_externalGeometryItem != 0)
        m_externalGeometryItem->setLocalBounds(bounds);

    if (m_externalGpuGeometryItem != 0)
        m_externalGpuGeometryItem->setLocalBounds(bounds);

    // 当前 Viewer Pick 只有指向 External Item 时，黄色 Bounds Highlight 才需要随 Modeling 数据同步更新。
    // Highlight 基础能力仍只接收明确 World Bounds，不自行查询 Picking 状态。
    const RenderItem* currentPickedItem = pickedItem();

    if (currentPickedItem == m_externalGeometryItem || currentPickedItem == m_externalGpuGeometryItem)
        showBoundsHighlight(currentPickedItem->worldBounds());
}


bool OpenGLSandboxWidget::initializeExternalGpuGeometry()
{
    QOpenGLContext* renderContext = context();

    if (renderContext == 0)
    {
        qWarning() << "OpenGLSandboxWidget initializeExternalGpuGeometry failed: Renderer OpenGL Context does not exist.";
        return false;
    }

    if (!QOpenGLContext::supportsThreadedOpenGL())
    {
        qWarning() << "OpenGLSandboxWidget initializeExternalGpuGeometry failed: current Qt platform does not support threaded OpenGL.";
        return false;
    }

    // QOffscreenSurface 必须在 GUI Thread 创建。
    // 使用 QOpenGLWidget 实际创建成功后的 Context Format，而不是最初 requestedFormat。
    m_externalGpuSurface = new QOffscreenSurface();
    m_externalGpuSurface->setScreen(renderContext->screen());
    m_externalGpuSurface->setFormat(renderContext->format());
    m_externalGpuSurface->create();

    if (!m_externalGpuSurface->isValid())
    {
        qWarning() << "OpenGLSandboxWidget initializeExternalGpuGeometry failed: Offscreen Surface creation failed.";
        shutdownExternalGpuWorker();
        return false;
    }

    // Shared Context 同样在 GUI Thread 创建，并且必须在 create() 之前指定 Share Context。
    m_externalGpuContext = new QOpenGLContext();
    m_externalGpuContext->setScreen(renderContext->screen());
    m_externalGpuContext->setFormat(renderContext->format());
    m_externalGpuContext->setShareContext(renderContext);

    if (!m_externalGpuContext->create() || !m_externalGpuContext->isValid())
    {
        qWarning() << "OpenGLSandboxWidget initializeExternalGpuGeometry failed: Shared OpenGL Context creation failed.";
        shutdownExternalGpuWorker();
        return false;
    }

    if (!QOpenGLContext::areSharing(renderContext, m_externalGpuContext))
    {
        qWarning() << "OpenGLSandboxWidget initializeExternalGpuGeometry failed: Renderer and Worker Context are not in the same Share Group.";
        shutdownExternalGpuWorker();
        return false;
    }

    if (!m_modelingGpuMeshWorker.configure(m_externalGpuContext, m_externalGpuSurface, &m_modelingGpuMesh, &m_modelingGpuMeshSync, thread()))
    {
        shutdownExternalGpuWorker();
        return false;
    }

    // Context 还没有在任何线程 current，因此可以安全地把 QObject Thread Affinity 转移到 Worker。
    m_externalGpuContext->moveToThread(&m_modelingGpuMeshWorker);

    if (!m_modelingGpuMeshWorker.startAndWait())
    {
        qWarning() << "OpenGLSandboxWidget initializeExternalGpuGeometry failed: External GPU Worker initialization failed.";
        shutdownExternalGpuWorker();
        return false;
    }

    qDebug() << "OpenGLSandboxWidget Shared Context created:"
             << "RendererContext=" << renderContext
             << "WorkerContext=" << m_externalGpuContext
             << "AreSharing=" << QOpenGLContext::areSharing(renderContext, m_externalGpuContext);

    m_externalGpuGeometry = new ExternalGpuGeometry("ModelingLibraryGpuGeometry");

    // ExternalGpuGeometry 只借用 Worker Context 创建的共享 VBO / EBO。
    // Worker 初始 GPU Storage 的 Write Fence 不再由 SandBox 手工等待；
    // ResourceManager 第一次 initializeGL() 时会通过 DataSource::prepareGpuViewGL() 完成该同步。
    if (!m_externalGpuGeometry->setDataSource(&m_modelingGpuMeshAdapter))
    {
        qWarning() << "OpenGLSandboxWidget initializeExternalGpuGeometry failed: unable to bind ModelingGpuMeshAdapter.";

        delete m_externalGpuGeometry;
        m_externalGpuGeometry = 0;
        shutdownExternalGpuWorker();
        return false;
    }

    if (resourceManager().add(m_externalGpuGeometry) == InvalidResourceId)
    {
        qWarning() << "OpenGLSandboxWidget initializeExternalGpuGeometry failed: ResourceManager rejected ExternalGpuGeometry.";

        delete m_externalGpuGeometry;
        m_externalGpuGeometry = 0;
        shutdownExternalGpuWorker();
        return false;
    }

    return true;
}


BufferGeometry* OpenGLSandboxWidget::createCube()
{
    BufferGeometry* cube = new BufferGeometry("LitCube", ResourceUpdateStatic, Triangles);

    std::vector<GeometryVertexAttribute> attributes;

    GeometryVertexAttribute positionAttribute;
    positionAttribute.location = 0;
    positionAttribute.componentCount = 3;
    positionAttribute.valueOffset = 0;
    attributes.push_back(positionAttribute);

    GeometryVertexAttribute normalAttribute;
    normalAttribute.location = 1;
    normalAttribute.componentCount = 3;
    normalAttribute.valueOffset = 3;
    attributes.push_back(normalAttribute);

    GeometryVertexAttribute uvAttribute;
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


Curve* OpenGLSandboxWidget::createDemoCurve()
{
    Curve* curve = new Curve("DemoCurve");

    std::vector<QVector3D> points;
    points.push_back(QVector3D(-4.5f, 3.0f, 0.0f));
    points.push_back(QVector3D(-2.25f, 3.8f, 0.0f));
    points.push_back(QVector3D(0.0f, 3.0f, 0.0f));
    points.push_back(QVector3D(2.25f, 3.8f, 0.0f));
    points.push_back(QVector3D(4.5f, 3.0f, 0.0f));

    curve->setPoints(points);
    curve->setColor(QVector3D(0.2f, 0.9f, 1.0f));
    return curve;
}


Texture* OpenGLSandboxWidget::createCheckerTexture()
{
    Texture* texture = new Texture("CubeCheckerTexture", ResourceUpdateStatic);

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


/// External Mesh A/B Test

bool OpenGLSandboxWidget::applyMatchedContentChange()
{
    // Vertex 2 在 Quad 和 Split Quad 两种 Topology 下都固定表示右上角。
    const bool cpuResult = m_modelingMesh.raiseVertex(2, 0.25);
    const bool gpuResult = m_modelingGpuSourceMesh.raiseVertex(2, 0.25);

    if (cpuResult != gpuResult || !cpuResult)
    {
        qWarning() << "OpenGLSandboxWidget matched content change failed: ModelingMesh operations returned different results.";
        return false;
    }

    if (!verifyModelingMeshConsistency("M Content Change"))
        return false;

    // 两条 External Path 的源数据已经验证一致，因此两个 Scene Item 使用同一份 Local Bounds。
    updateExternalRenderItemBounds();

    // External Triangle 顶点可能已经变化；保留当前 Picked Item，但清除旧 Primitive Highlight，要求下一次 Click 重新取得明确 Primitive。
    clearPrimitiveHighlight();

    return syncExternalGpuWorker("M Content Change");
}

bool OpenGLSandboxWidget::rebuildMatchedExternalMeshes()
{
    const double halfWidth = m_externalWide ? 1.5 : 1.0;

    if (m_externalSplit)
    {
        m_modelingMesh.buildSplitQuad(halfWidth);
        m_modelingGpuSourceMesh.buildSplitQuad(halfWidth);
    }
    else
    {
        m_modelingMesh.buildQuad(halfWidth);
        m_modelingGpuSourceMesh.buildQuad(halfWidth);
    }

    qDebug() << "OpenGLSandboxWidget matched ModelingMesh rebuild:"
             << "Width=" << (m_externalWide ? "Wide" : "Narrow")
             << "Topology=" << (m_externalSplit ? "SplitQuad" : "Quad")
             << "Vertices=" << static_cast<int>(m_modelingMesh.positions().size())
             << "Indices=" << static_cast<int>(m_modelingMesh.indices().size());

    if (!verifyModelingMeshConsistency("Structure Change"))
        return false;

    updateExternalRenderItemBounds();

    // Topology / Width 重建后旧 External Primitive Vertex 不再作为当前 Primitive Highlight 的有效几何输入。
    clearPrimitiveHighlight();

    return syncExternalGpuWorker("Structure Change");
}

bool OpenGLSandboxWidget::verifyModelingMeshConsistency(const char* operation) const
{
    if (m_modelingMesh.structureRevision() != m_modelingGpuSourceMesh.structureRevision() ||
        m_modelingMesh.contentRevision() != m_modelingGpuSourceMesh.contentRevision())
    {
        qWarning() << "External A/B source consistency failed:"
                   << operation
                   << "Revision mismatch.";
        return false;
    }

    if (m_modelingMesh.positions().size() != m_modelingGpuSourceMesh.positions().size() ||
        m_modelingMesh.normals().size() != m_modelingGpuSourceMesh.normals().size() ||
        m_modelingMesh.uvs().size() != m_modelingGpuSourceMesh.uvs().size() ||
        m_modelingMesh.indices().size() != m_modelingGpuSourceMesh.indices().size())
    {
        qWarning() << "External A/B source consistency failed:"
                   << operation
                   << "Buffer size mismatch.";
        return false;
    }

    for (std::size_t i = 0; i < m_modelingMesh.positions().size(); ++i)
    {
        const ModelingPoint& cpu = m_modelingMesh.positions()[i];
        const ModelingPoint& gpu = m_modelingGpuSourceMesh.positions()[i];

        if (cpu.x != gpu.x || cpu.y != gpu.y || cpu.z != gpu.z)
        {
            qWarning() << "External A/B source consistency failed:"
                       << operation
                       << "Position mismatch at Vertex" << static_cast<int>(i);
            return false;
        }
    }

    for (std::size_t i = 0; i < m_modelingMesh.normals().size(); ++i)
    {
        const ModelingNormal& cpu = m_modelingMesh.normals()[i];
        const ModelingNormal& gpu = m_modelingGpuSourceMesh.normals()[i];

        if (cpu.x != gpu.x || cpu.y != gpu.y || cpu.z != gpu.z)
        {
            qWarning() << "External A/B source consistency failed:"
                       << operation
                       << "Normal mismatch at Vertex" << static_cast<int>(i);
            return false;
        }
    }

    for (std::size_t i = 0; i < m_modelingMesh.uvs().size(); ++i)
    {
        const ModelingUV& cpu = m_modelingMesh.uvs()[i];
        const ModelingUV& gpu = m_modelingGpuSourceMesh.uvs()[i];

        if (cpu.u != gpu.u || cpu.v != gpu.v)
        {
            qWarning() << "External A/B source consistency failed:"
                       << operation
                       << "UV mismatch at Vertex" << static_cast<int>(i);
            return false;
        }
    }

    for (std::size_t i = 0; i < m_modelingMesh.indices().size(); ++i)
    {
        if (m_modelingMesh.indices()[i] != m_modelingGpuSourceMesh.indices()[i])
        {
            qWarning() << "External A/B source consistency failed:"
                       << operation
                       << "Index mismatch at" << static_cast<int>(i);
            return false;
        }
    }

    qDebug() << "External A/B source consistency passed:"
             << operation
             << "StructureRevision=" << static_cast<qulonglong>(m_modelingMesh.structureRevision())
             << "ContentRevision=" << static_cast<qulonglong>(m_modelingMesh.contentRevision())
             << "Vertices=" << static_cast<int>(m_modelingMesh.positions().size())
             << "Indices=" << static_cast<int>(m_modelingMesh.indices().size());

    return true;
}

bool OpenGLSandboxWidget::syncExternalGpuWorker(const char* operation)
{
    if (!viewerGLReady())
    {
        qWarning() << "OpenGLSandboxWidget syncExternalGpuWorker failed: OpenGL Scene is not ready:" << operation;
        return false;
    }

    if (!m_modelingGpuMeshWorker.requestSyncAndWait())
    {
        qWarning() << "OpenGLSandboxWidget syncExternalGpuWorker failed:" << operation;
        return false;
    }

    // 这里只表示 Worker 已经发布 Write Fence，不表示 SandBox 自己需要等待 Fence。
    // 真正的等待位置由 ExternalGpuGeometry 根据 Structure Sync 或 Draw Sync 的语义决定。
    qDebug() << "OpenGLSandboxWidget External GPU Worker Fence ready:" << operation;

    return true;
}

bool OpenGLSandboxWidget::replaceExternalGpuBuffers()
{
    if (!viewerGLReady())
    {
        qWarning() << "OpenGLSandboxWidget replaceExternalGpuBuffers failed: OpenGL Scene is not ready.";
        return false;
    }

    // Buffer Replacement 不属于建模操作。
    // 两份 ModelingMesh 在整个操作前后必须保持完全一致，并且 Revision 不发生变化。
    if (!verifyModelingMeshConsistency("P Before GPU Buffer Replacement"))
        return false;

    const unsigned long long sourceStructureRevision = m_modelingGpuSourceMesh.structureRevision();
    const unsigned long long sourceContentRevision = m_modelingGpuSourceMesh.contentRevision();

    if (!m_modelingGpuMeshWorker.requestBufferReplacementAndWait())
    {
        qWarning() << "OpenGLSandboxWidget replaceExternalGpuBuffers failed: Worker Buffer Replacement failed.";
        return false;
    }

    if (m_modelingGpuSourceMesh.structureRevision() != sourceStructureRevision ||
        m_modelingGpuSourceMesh.contentRevision() != sourceContentRevision)
    {
        qWarning() << "OpenGLSandboxWidget replaceExternalGpuBuffers failed: GPU Buffer Replacement unexpectedly modified ModelingMesh Revision.";
        return false;
    }

    if (!verifyModelingMeshConsistency("P After GPU Buffer Replacement"))
        return false;

    qDebug() << "OpenGLSandboxWidget External GPU Buffer Replacement Fence ready:"
             << "GpuStructureRevision=" << static_cast<qulonglong>(m_modelingGpuMesh.structureRevision())
             << "PositionVBO=" << m_modelingGpuMesh.positionBuffer()
             << "NormalVBO=" << m_modelingGpuMesh.normalBuffer()
             << "UVVBO=" << m_modelingGpuMesh.uvBuffer()
             << "EBO=" << m_modelingGpuMesh.indexBuffer();

    return true;
}


/// Lighting Test

void OpenGLSandboxWidget::applyLightPreset()
{
    if (m_sun == 0 || m_fillLight == 0 || m_pointLight == 0 || m_spotLight == 0)
        return;

    m_sun->setEnabled(false);
    m_fillLight->setEnabled(false);
    m_pointLight->setEnabled(false);
    m_spotLight->setEnabled(false);

    switch (m_lightPreset)
    {
    case 0: // Main
        m_sun->setEnabled(true);
        break;

    case 1: // Main + Fill
        m_sun->setEnabled(true);
        m_fillLight->setEnabled(true);
        break;

    case 2: // Main + Fill + Point
        m_sun->setEnabled(true);
        m_fillLight->setEnabled(true);
        m_pointLight->setEnabled(true);
        break;

    case 3: // All
        m_sun->setEnabled(true);
        m_fillLight->setEnabled(true);
        m_pointLight->setEnabled(true);
        m_spotLight->setEnabled(true);
        break;

    case 4: // Off
        break;

    default:
        qWarning() << "OpenGLSandboxWidget applyLightPreset failed: unsupported preset:" << m_lightPreset;
        m_lightPreset = 1;
        m_sun->setEnabled(true);
        m_fillLight->setEnabled(true);
        break;
    }
}

const char* OpenGLSandboxWidget::lightPresetName() const
{
    switch (m_lightPreset)
    {
    case 0:
        return "Main";
    case 1:
        return "Main+Fill";
    case 2:
        return "Main+Fill+Point";
    case 3:
        return "All";
    case 4:
        return "Off";
    }

    return "Unknown";
}

/// SandBox 状态

void OpenGLSandboxWidget::updateWindowTitle()
{
    const bool textureEnabled = m_cubeMaterial != 0 && m_cubeMaterial->hasDiffuseTexture();
    const Camera* camera = cameraManager().activeCamera();

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

    const RenderItem* currentPickedItem = pickedItem();
    cameraText += QString(" PickMode:%1").arg(viewerPickModeName(pickMode()));
    cameraText += QString(" Pick:%1").arg(currentPickedItem != 0 ? currentPickedItem->name() : "None");

    if (currentPickedItem != 0)
        cameraText += QString(" Mode:%1").arg(renderItemDisplayModeName(currentPickedItem->displayMode()));

    setWindowTitle(QString("OpenGL SandBox | Viewer Module + Test Harness | %1 | Left Drag Orbit | Middle Drag Pan | Right Click Pick | Q Pick Mode | Esc Clear Pick/Measure | F Fit Picked | 1 Front | 2 Back | 3 Left | 4 Right | 5 Top | 6 Bottom | 7 Iso | H Fit All | W Display Mode | J Hide Picked | I Isolate Picked | U Show All | N MultiPart:%2 | M Matched Content | B Width:%3 | K Topology:%4 | P Replace GPU Buffers | X Resource Rollback | R Reset | O Origin | C Target:%5 | G Grid:%6 | A Axis:%7 | V Gizmo:%8 | L Light:%9 | T Texture:%10")
        .arg(cameraText)
        .arg(multiPartStateName())
        .arg(m_externalWide ? "Wide" : "Narrow")
        .arg(m_externalSplit ? "Split" : "Quad")
        .arg(cameraTargetVisible() ? "On" : "Off")
        .arg(gridVisible() ? "On" : "Off")
        .arg(axesVisible() ? "On" : "Off")
        .arg(viewNavigationVisible() ? "On" : "Off")
        .arg(lightPresetName())
        .arg(textureEnabled ? "On" : "Off"));
}

/// External GPU 生命周期

void OpenGLSandboxWidget::shutdownExternalGpuWorker()
{
    // Worker 自己在 Shared Context 中释放 External GPU Storage，并把 Context 移回 GUI Thread。
    m_modelingGpuMeshWorker.stopAndWait();

    if (m_externalGpuContext != 0)
    {
        if (m_externalGpuContext->thread() != thread())
        {
            qWarning() << "OpenGLSandboxWidget shutdownExternalGpuWorker: Shared Context did not return to GUI Thread.";
        }
        else
        {
            delete m_externalGpuContext;
            m_externalGpuContext = 0;
        }
    }

    // QOffscreenSurface 的销毁必须留在 GUI Thread。
    if (m_externalGpuSurface != 0)
    {
        m_externalGpuSurface->destroy();
        delete m_externalGpuSurface;
        m_externalGpuSurface = 0;
    }
}