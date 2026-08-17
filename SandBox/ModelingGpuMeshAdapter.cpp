#include "ModelingGpuMeshAdapter.h"

#include <QDebug>

ModelingGpuMeshAdapter::ModelingGpuMeshAdapter(const ModelingGpuMesh* mesh, ModelingGpuMeshSync* sync)
    : m_mesh(mesh)
    , m_sync(sync)
{
}

/// Source

void ModelingGpuMeshAdapter::setMesh(const ModelingGpuMesh* mesh)
{
    m_mesh = mesh;
}

const ModelingGpuMesh* ModelingGpuMeshAdapter::mesh() const
{
    return m_mesh;
}

/// GPU Synchronization

void ModelingGpuMeshAdapter::setSync(ModelingGpuMeshSync* sync)
{
    m_sync = sync;
}

ModelingGpuMeshSync* ModelingGpuMeshAdapter::sync() const
{
    return m_sync;
}

/// ExternalGpuMeshDataSource

bool ModelingGpuMeshAdapter::gpuView(ExternalGpuMeshView& view) const
{
    if (m_mesh == 0)
        return false;

    if (m_mesh->positionBuffer() == 0 || m_mesh->normalBuffer() == 0 || m_mesh->uvBuffer() == 0 || m_mesh->indexBuffer() == 0)
        return false;

    if (m_mesh->indexCount() <= 0)
        return false;

    view.vertexBuffers.clear();
    view.attributes.clear();

    view.renderType = Triangles;

    ExternalGpuVertexBufferView positionBuffer;
    positionBuffer.bufferId = m_mesh->positionBuffer();
    positionBuffer.stride = sizeof(ModelingPoint);
    view.vertexBuffers.push_back(positionBuffer);

    ExternalGpuVertexBufferView normalBuffer;
    normalBuffer.bufferId = m_mesh->normalBuffer();
    normalBuffer.stride = sizeof(ModelingNormal);
    view.vertexBuffers.push_back(normalBuffer);

    ExternalGpuVertexBufferView uvBuffer;
    uvBuffer.bufferId = m_mesh->uvBuffer();
    uvBuffer.stride = sizeof(ModelingUV);
    view.vertexBuffers.push_back(uvBuffer);

    ExternalGpuVertexAttribute positionAttribute;
    positionAttribute.location = 0;
    positionAttribute.componentCount = 3;
    positionAttribute.bufferIndex = 0;
    positionAttribute.byteOffset = 0;
    positionAttribute.componentType = GL_DOUBLE;
    positionAttribute.normalized = false;
    view.attributes.push_back(positionAttribute);

    ExternalGpuVertexAttribute normalAttribute;
    normalAttribute.location = 1;
    normalAttribute.componentCount = 3;
    normalAttribute.bufferIndex = 1;
    normalAttribute.byteOffset = 0;
    normalAttribute.componentType = GL_DOUBLE;
    normalAttribute.normalized = false;
    view.attributes.push_back(normalAttribute);

    ExternalGpuVertexAttribute uvAttribute;
    uvAttribute.location = 2;
    uvAttribute.componentCount = 2;
    uvAttribute.bufferIndex = 2;
    uvAttribute.byteOffset = 0;
    uvAttribute.componentType = GL_FLOAT;
    uvAttribute.normalized = false;
    view.attributes.push_back(uvAttribute);

    view.indices.bufferId = m_mesh->indexBuffer();
    view.indices.indexCount = m_mesh->indexCount();
    view.indices.indexType = GL_UNSIGNED_INT;

    return true;
}

ExternalGpuMeshRevision ModelingGpuMeshAdapter::structureRevision() const
{
    if (m_mesh == 0)
        return 0;

    return m_mesh->structureRevision();
}

/// GPU View 同步

bool ModelingGpuMeshAdapter::prepareGpuViewGL(QOpenGLFunctions_3_3_Core* gl) const
{
    if (m_sync == 0)
        return true;

    // Structure Revision 可能代表 Buffer ID、Data Store 或 Index Count 变化。
    // MyOpenGL 在读取新 View、glIsBuffer() 和重新 Attach VAO 之前先等待 Worker Write Fence。
    return m_sync->waitExternalWriteCompleteGL(gl);
}

void ModelingGpuMeshAdapter::acknowledgeStructureRevision(ExternalGpuMeshRevision revision) const
{
    if (m_sync == 0)
        return;

    m_sync->acknowledgeStructureRevision(revision);
}

/// GPU 读取同步

bool ModelingGpuMeshAdapter::beginReadGL(QOpenGLFunctions_3_3_Core* gl) const
{
    if (m_sync == 0)
        return true;

    // 如果本帧 Structure Sync 已经消费了 Write Fence，这里会直接返回 true。
    // 如果只是相同 Buffer ID 的 Content / Data Store Update，则由这里负责等待 Write Fence。
    return m_sync->waitExternalWriteCompleteGL(gl);
}

void ModelingGpuMeshAdapter::endReadGL(QOpenGLFunctions_3_3_Core* gl) const
{
    if (m_sync == 0)
        return;

    if (!m_sync->publishRendererReadCompleteGL(gl))
        qWarning() << "ModelingGpuMeshAdapter endReadGL failed: unable to publish Renderer Read Fence.";
}