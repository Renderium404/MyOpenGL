#include "ModelingGpuMesh.h"

#include <QDebug>

ModelingGpuMesh::ModelingGpuMesh(const ModelingMesh* mesh)
    : m_mesh(mesh)
    , m_indexCount(0)
    , m_sourceStructureRevision(0)
    , m_sourceContentRevision(0)
    , m_structureRevision(0)
{
}

ModelingGpuMesh::~ModelingGpuMesh()
{
    if (gpuInitialized() || !m_retiredBufferSets.empty())
        qWarning() << "ModelingGpuMesh destroyed while external GPU buffers are still initialized.";
}

/// Source

bool ModelingGpuMesh::setMesh(const ModelingMesh* mesh)
{
    if (gpuInitialized())
    {
        qWarning() << "ModelingGpuMesh setMesh failed: GPU buffers are already initialized.";
        return false;
    }

    if (mesh == 0)
    {
        qWarning() << "ModelingGpuMesh setMesh failed: mesh is null.";
        return false;
    }

    m_mesh = mesh;
    m_sourceStructureRevision = 0;
    m_sourceContentRevision = 0;

    return true;
}

const ModelingMesh* ModelingGpuMesh::mesh() const
{
    return m_mesh;
}

/// GPU 生命周期

bool ModelingGpuMesh::initializeGL(QOpenGLFunctions_3_3_Core* gl)
{
    if (gl == 0)
    {
        qWarning() << "ModelingGpuMesh initializeGL failed: OpenGL functions are null.";
        return false;
    }

    if (m_mesh == 0)
    {
        qWarning() << "ModelingGpuMesh initializeGL failed: ModelingMesh source is null.";
        return false;
    }

    if (gpuInitialized())
    {
        qWarning() << "ModelingGpuMesh initializeGL failed: GPU buffers are already initialized.";
        return false;
    }

    if (m_mesh->positions().empty() || m_mesh->normals().empty() || m_mesh->uvs().empty() || m_mesh->indices().empty())
    {
        qWarning() << "ModelingGpuMesh initializeGL failed: ModelingMesh source is empty.";
        return false;
    }

    if (!createBufferSetGL(gl, m_buffers))
        return false;

    if (!uploadFullGL(gl))
    {
        deleteBufferSetGL(gl, m_buffers);
        return false;
    }

    m_sourceStructureRevision = m_mesh->structureRevision();
    m_sourceContentRevision = m_mesh->contentRevision();

    // 第一次创建 VBO / EBO 后 GPU View 从不存在变为有效，因此建立第一个 Structure Revision。
    ++m_structureRevision;

    qDebug() << "ModelingGpuMesh GPU Storage initialized:"
             << "SourceStructureRevision=" << static_cast<qulonglong>(m_sourceStructureRevision)
             << "SourceContentRevision=" << static_cast<qulonglong>(m_sourceContentRevision)
             << "GpuStructureRevision=" << static_cast<qulonglong>(m_structureRevision)
             << "PositionVBO=" << m_buffers.positionBuffer
             << "NormalVBO=" << m_buffers.normalBuffer
             << "UVVBO=" << m_buffers.uvBuffer
             << "EBO=" << m_buffers.indexBuffer
             << "Vertices=" << static_cast<int>(m_mesh->positions().size())
             << "Indices=" << m_indexCount;

    return true;
}

bool ModelingGpuMesh::syncGL(QOpenGLFunctions_3_3_Core* gl)
{
    if (gl == 0)
    {
        qWarning() << "ModelingGpuMesh syncGL failed: OpenGL functions are null.";
        return false;
    }

    if (m_mesh == 0)
    {
        qWarning() << "ModelingGpuMesh syncGL failed: ModelingMesh source is null.";
        return false;
    }

    if (!gpuInitialized())
    {
        qWarning() << "ModelingGpuMesh syncGL failed: GPU buffers are not initialized.";
        return false;
    }

    const unsigned long long currentStructureRevision = m_mesh->structureRevision();
    const unsigned long long currentContentRevision = m_mesh->contentRevision();

    // ModelingMesh Structure 变化后旧 Change History 和旧数据地址都不能再作为局部同步依据。
    if (currentStructureRevision != m_sourceStructureRevision)
    {
        const int previousIndexCount = m_indexCount;

        if (!uploadFullGL(gl))
            return false;

        m_sourceStructureRevision = currentStructureRevision;
        m_sourceContentRevision = currentContentRevision;

        // 当前 GPU Buffer ID 和 Vertex Layout 始终不变。
        // 因此只有 Index Count 变化时，ExternalGpuGeometry 才需要重新获取 GPU View 并重配 Draw State。
        const bool gpuViewStructureChanged = previousIndexCount != m_indexCount;

        if (gpuViewStructureChanged)
            ++m_structureRevision;

        qDebug() << "ModelingGpuMesh Full GPU Storage Sync:"
                 << "SourceStructureRevision=" << static_cast<qulonglong>(m_sourceStructureRevision)
                 << "SourceContentRevision=" << static_cast<qulonglong>(m_sourceContentRevision)
                 << "GpuStructureRevision=" << static_cast<qulonglong>(m_structureRevision)
                 << "GpuViewStructureChanged=" << gpuViewStructureChanged
                 << "Vertices=" << static_cast<int>(m_mesh->positions().size())
                 << "Indices=" << m_indexCount;

        return true;
    }

    if (currentContentRevision == m_sourceContentRevision)
        return true;

    std::vector<ModelingMeshChange> changes;

    if (!m_mesh->changesSince(m_sourceContentRevision, changes) || changes.empty())
    {
        // Change History 无法覆盖当前 Revision 时退化为 Full GPU Storage Sync，保证外部 GPU 数据正确。
        if (!uploadFullGL(gl))
            return false;

        m_sourceContentRevision = currentContentRevision;

        qDebug() << "ModelingGpuMesh Fallback Full GPU Storage Sync:"
                 << "SourceContentRevision=" << static_cast<qulonglong>(m_sourceContentRevision);

        return true;
    }

    std::size_t uploadedBytes = 0;
    int uploadCalls = 0;

    if (!uploadChangesGL(gl, changes, uploadedBytes, uploadCalls))
        return false;

    m_sourceContentRevision = currentContentRevision;

    qDebug() << "ModelingGpuMesh Partial GPU Storage Sync:"
             << "SourceContentRevision=" << static_cast<qulonglong>(m_sourceContentRevision)
             << "UploadedBytes=" << static_cast<qulonglong>(uploadedBytes)
             << "UploadCalls=" << uploadCalls
             << "GpuStructureRevision=" << static_cast<qulonglong>(m_structureRevision);

    return true;
}

void ModelingGpuMesh::releaseGL(QOpenGLFunctions_3_3_Core* gl)
{
    if (gl == 0)
        return;

    // 正常运行时 Retired Buffer Set 应已经通过 Structure Acknowledgment 逐步回收。
    // 退出时仍可能存在最后一版尚未来得及触发下一次 Worker Collect，因此这里统一兜底释放。
    for (std::size_t i = 0; i < m_retiredBufferSets.size(); ++i)
        deleteBufferSetGL(gl, m_retiredBufferSets[i].buffers);

    m_retiredBufferSets.clear();

    deleteBufferSetGL(gl, m_buffers);

    m_indexCount = 0;
    m_sourceStructureRevision = 0;
    m_sourceContentRevision = 0;
    m_structureRevision = 0;
}

/// GPU Buffer Replacement

bool ModelingGpuMesh::replaceBuffersGL(QOpenGLFunctions_3_3_Core* gl)
{
    if (gl == 0)
    {
        qWarning() << "ModelingGpuMesh replaceBuffersGL failed: OpenGL functions are null.";
        return false;
    }

    if (m_mesh == 0)
    {
        qWarning() << "ModelingGpuMesh replaceBuffersGL failed: ModelingMesh source is null.";
        return false;
    }

    if (!gpuInitialized())
    {
        qWarning() << "ModelingGpuMesh replaceBuffersGL failed: current GPU Buffer Set is not initialized.";
        return false;
    }

    BufferSet newBuffers;

    if (!createBufferSetGL(gl, newBuffers))
        return false;

    // 新 Buffer Set 使用和当前 GPU Storage 完全相同的 ModelingMesh Source。
    // 因此本次操作只改变 GPU Object Identity，不改变任何 Modeling Data。
    if (!uploadFullToBufferSetGL(gl, newBuffers))
    {
        deleteBufferSetGL(gl, newBuffers);
        return false;
    }

    const BufferSet oldBuffers = m_buffers;

    m_buffers = newBuffers;
    m_indexCount = static_cast<int>(m_mesh->indices().size());

    // Buffer ID 已改变，即使 Layout 和 Index Count 完全相同，也必须通知 ExternalGpuGeometry 重新配置 VAO。
    ++m_structureRevision;

    RetiredBufferSet retiredBuffers;
    retiredBuffers.buffers = oldBuffers;

    // 当 Renderer 成功采用当前新 Structure Revision 后，
    // 它的 VAO 已经不再引用 oldBuffers，因此旧 Buffer Set 才具备退休条件。
    retiredBuffers.retireAfterStructureRevision = m_structureRevision;
    m_retiredBufferSets.push_back(retiredBuffers);

    qDebug() << "ModelingGpuMesh GPU Buffer Set replaced:"
             << "GpuStructureRevision=" << static_cast<qulonglong>(m_structureRevision)
             << "OldPositionVBO=" << oldBuffers.positionBuffer
             << "NewPositionVBO=" << m_buffers.positionBuffer
             << "OldNormalVBO=" << oldBuffers.normalBuffer
             << "NewNormalVBO=" << m_buffers.normalBuffer
             << "OldUVVBO=" << oldBuffers.uvBuffer
             << "NewUVVBO=" << m_buffers.uvBuffer
             << "OldEBO=" << oldBuffers.indexBuffer
             << "NewEBO=" << m_buffers.indexBuffer
             << "IndexCount=" << m_indexCount
             << "RetireAfterRevision=" << static_cast<qulonglong>(retiredBuffers.retireAfterStructureRevision)
             << "RetiredBufferSets=" << static_cast<int>(m_retiredBufferSets.size());

    return true;
}

int ModelingGpuMesh::collectRetiredBufferSetsGL(QOpenGLFunctions_3_3_Core* gl, unsigned long long synchronizedStructureRevision)
{
    if (gl == 0)
    {
        qWarning() << "ModelingGpuMesh collectRetiredBufferSetsGL failed: OpenGL functions are null.";
        return 0;
    }

    if (synchronizedStructureRevision == 0 || m_retiredBufferSets.empty())
        return 0;

    int releasedBufferSets = 0;
    std::size_t index = 0;

    while (index < m_retiredBufferSets.size())
    {
        RetiredBufferSet& retiredBuffers = m_retiredBufferSets[index];

        if (retiredBuffers.retireAfterStructureRevision > synchronizedStructureRevision)
        {
            ++index;
            continue;
        }

        // Renderer 已明确确认 VAO 至少同步到了 retireAfterStructureRevision，
        // 因此该旧 Buffer Set 已经不再作为当前 VAO Attachment 使用。
        deleteBufferSetGL(gl, retiredBuffers.buffers);

        m_retiredBufferSets.erase(m_retiredBufferSets.begin() + index);
        ++releasedBufferSets;
    }

    if (releasedBufferSets > 0)
    {
        qDebug() << "ModelingGpuMesh Retired GPU Buffer Sets collected:"
                 << "RendererStructureRevision=" << static_cast<qulonglong>(synchronizedStructureRevision)
                 << "ReleasedBufferSets=" << releasedBufferSets
                 << "RemainingBufferSets=" << static_cast<int>(m_retiredBufferSets.size());
    }

    return releasedBufferSets;
}

/// GPU Buffer

GLuint ModelingGpuMesh::positionBuffer() const
{
    return m_buffers.positionBuffer;
}

GLuint ModelingGpuMesh::normalBuffer() const
{
    return m_buffers.normalBuffer;
}

GLuint ModelingGpuMesh::uvBuffer() const
{
    return m_buffers.uvBuffer;
}

GLuint ModelingGpuMesh::indexBuffer() const
{
    return m_buffers.indexBuffer;
}

int ModelingGpuMesh::indexCount() const
{
    return m_indexCount;
}

/// GPU View Structure Revision

unsigned long long ModelingGpuMesh::structureRevision() const
{
    return m_structureRevision;
}

/// GPU Buffer Set

bool ModelingGpuMesh::createBufferSetGL(QOpenGLFunctions_3_3_Core* gl, BufferSet& buffers)
{
    gl->glGenBuffers(1, &buffers.positionBuffer);
    gl->glGenBuffers(1, &buffers.normalBuffer);
    gl->glGenBuffers(1, &buffers.uvBuffer);
    gl->glGenBuffers(1, &buffers.indexBuffer);

    if (!bufferSetInitialized(buffers))
    {
        qWarning() << "ModelingGpuMesh createBufferSetGL failed: external GPU buffer creation failed.";
        deleteBufferSetGL(gl, buffers);
        return false;
    }

    return true;
}

void ModelingGpuMesh::deleteBufferSetGL(QOpenGLFunctions_3_3_Core* gl, BufferSet& buffers)
{
    if (buffers.indexBuffer != 0)
    {
        gl->glDeleteBuffers(1, &buffers.indexBuffer);
        buffers.indexBuffer = 0;
    }

    if (buffers.uvBuffer != 0)
    {
        gl->glDeleteBuffers(1, &buffers.uvBuffer);
        buffers.uvBuffer = 0;
    }

    if (buffers.normalBuffer != 0)
    {
        gl->glDeleteBuffers(1, &buffers.normalBuffer);
        buffers.normalBuffer = 0;
    }

    if (buffers.positionBuffer != 0)
    {
        gl->glDeleteBuffers(1, &buffers.positionBuffer);
        buffers.positionBuffer = 0;
    }
}

bool ModelingGpuMesh::bufferSetInitialized(const BufferSet& buffers) const
{
    return buffers.positionBuffer != 0 &&
           buffers.normalBuffer != 0 &&
           buffers.uvBuffer != 0 &&
           buffers.indexBuffer != 0;
}

/// GPU 上传

bool ModelingGpuMesh::uploadFullGL(QOpenGLFunctions_3_3_Core* gl)
{
    if (!uploadFullToBufferSetGL(gl, m_buffers))
        return false;

    m_indexCount = static_cast<int>(m_mesh->indices().size());
    return true;
}

bool ModelingGpuMesh::uploadFullToBufferSetGL(QOpenGLFunctions_3_3_Core* gl, const BufferSet& buffers)
{
    if (m_mesh == 0)
        return false;

    if (m_mesh->positions().empty() || m_mesh->normals().empty() || m_mesh->uvs().empty() || m_mesh->indices().empty())
    {
        qWarning() << "ModelingGpuMesh uploadFullToBufferSetGL failed: ModelingMesh source is empty.";
        return false;
    }

    // GL_COPY_WRITE_BUFFER 不属于 VAO State。
    // 外部 GPU Storage 使用该通用 Buffer Target 更新数据，避免修改 Renderer 或 ExternalGpuGeometry 的 VAO / EBO Binding。
    gl->glBindBuffer(GL_COPY_WRITE_BUFFER, buffers.positionBuffer);
    gl->glBufferData(GL_COPY_WRITE_BUFFER, static_cast<GLsizeiptr>(m_mesh->positions().size() * sizeof(ModelingPoint)), &m_mesh->positions()[0], GL_DYNAMIC_DRAW);

    gl->glBindBuffer(GL_COPY_WRITE_BUFFER, buffers.normalBuffer);
    gl->glBufferData(GL_COPY_WRITE_BUFFER, static_cast<GLsizeiptr>(m_mesh->normals().size() * sizeof(ModelingNormal)), &m_mesh->normals()[0], GL_STATIC_DRAW);

    gl->glBindBuffer(GL_COPY_WRITE_BUFFER, buffers.uvBuffer);
    gl->glBufferData(GL_COPY_WRITE_BUFFER, static_cast<GLsizeiptr>(m_mesh->uvs().size() * sizeof(ModelingUV)), &m_mesh->uvs()[0], GL_STATIC_DRAW);

    gl->glBindBuffer(GL_COPY_WRITE_BUFFER, buffers.indexBuffer);
    gl->glBufferData(GL_COPY_WRITE_BUFFER, static_cast<GLsizeiptr>(m_mesh->indices().size() * sizeof(std::uint32_t)), &m_mesh->indices()[0], GL_STATIC_DRAW);

    gl->glBindBuffer(GL_COPY_WRITE_BUFFER, 0);

    return true;
}

bool ModelingGpuMesh::uploadChangesGL(QOpenGLFunctions_3_3_Core* gl, const std::vector<ModelingMeshChange>& changes, std::size_t& uploadedBytes, int& uploadCalls)
{
    uploadedBytes = 0;
    uploadCalls = 0;

    for (std::size_t i = 0; i < changes.size(); ++i)
    {
        const ModelingMeshChange& change = changes[i];

        GLuint bufferId = 0;
        const void* sourceData = 0;
        std::size_t sourceByteSize = 0;

        switch (change.buffer)
        {
        case ModelingMeshBufferPosition:
            bufferId = m_buffers.positionBuffer;
            sourceData = m_mesh->positions().empty() ? 0 : &m_mesh->positions()[0];
            sourceByteSize = m_mesh->positions().size() * sizeof(ModelingPoint);
            break;

        case ModelingMeshBufferNormal:
            bufferId = m_buffers.normalBuffer;
            sourceData = m_mesh->normals().empty() ? 0 : &m_mesh->normals()[0];
            sourceByteSize = m_mesh->normals().size() * sizeof(ModelingNormal);
            break;

        case ModelingMeshBufferUV:
            bufferId = m_buffers.uvBuffer;
            sourceData = m_mesh->uvs().empty() ? 0 : &m_mesh->uvs()[0];
            sourceByteSize = m_mesh->uvs().size() * sizeof(ModelingUV);
            break;

        case ModelingMeshBufferIndex:
            bufferId = m_buffers.indexBuffer;
            sourceData = m_mesh->indices().empty() ? 0 : &m_mesh->indices()[0];
            sourceByteSize = m_mesh->indices().size() * sizeof(std::uint32_t);
            break;
        }

        if (bufferId == 0 || sourceData == 0)
        {
            qWarning() << "ModelingGpuMesh uploadChangesGL failed: change references unavailable buffer data.";
            gl->glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
            return false;
        }

        if (change.byteOffset > sourceByteSize || change.byteSize > sourceByteSize - change.byteOffset)
        {
            qWarning() << "ModelingGpuMesh uploadChangesGL failed: change range exceeds source buffer.";
            gl->glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
            return false;
        }

        const char* sourceBytes = static_cast<const char*>(sourceData);

        gl->glBindBuffer(GL_COPY_WRITE_BUFFER, bufferId);
        gl->glBufferSubData(GL_COPY_WRITE_BUFFER, static_cast<GLintptr>(change.byteOffset), static_cast<GLsizeiptr>(change.byteSize), sourceBytes + change.byteOffset);

        uploadedBytes += change.byteSize;
        ++uploadCalls;
    }

    gl->glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    return true;
}

/// 状态

bool ModelingGpuMesh::gpuInitialized() const
{
    return bufferSetInitialized(m_buffers);
}