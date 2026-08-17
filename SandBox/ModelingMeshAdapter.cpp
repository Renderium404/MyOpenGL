#include "ModelingMeshAdapter.h"

ModelingMeshAdapter::ModelingMeshAdapter(const ModelingMesh* mesh)
    : m_mesh(mesh)
{
}

/// Source

void ModelingMeshAdapter::setMesh(const ModelingMesh* mesh)
{
    m_mesh = mesh;
}

const ModelingMesh* ModelingMeshAdapter::mesh() const
{
    return m_mesh;
}

/// ExternalMeshDataSource

bool ModelingMeshAdapter::dataView(ExternalMeshDataView& view) const
{
    if (m_mesh == 0)
        return false;

    if (m_mesh->positions().empty() || m_mesh->normals().empty() || m_mesh->uvs().empty() || m_mesh->indices().empty())
        return false;

    view.vertexBuffers.clear();
    view.attributes.clear();

    view.vertexCount = static_cast<int>(m_mesh->positions().size());
    view.renderType = Triangles;

    ExternalVertexBufferView positionBuffer;
    positionBuffer.data = &m_mesh->positions()[0];
    positionBuffer.byteSize = m_mesh->positions().size() * sizeof(ModelingPoint);
    positionBuffer.stride = sizeof(ModelingPoint);
    view.vertexBuffers.push_back(positionBuffer);

    ExternalVertexBufferView normalBuffer;
    normalBuffer.data = &m_mesh->normals()[0];
    normalBuffer.byteSize = m_mesh->normals().size() * sizeof(ModelingNormal);
    normalBuffer.stride = sizeof(ModelingNormal);
    view.vertexBuffers.push_back(normalBuffer);

    ExternalVertexBufferView uvBuffer;
    uvBuffer.data = &m_mesh->uvs()[0];
    uvBuffer.byteSize = m_mesh->uvs().size() * sizeof(ModelingUV);
    uvBuffer.stride = sizeof(ModelingUV);
    view.vertexBuffers.push_back(uvBuffer);

    ExternalVertexAttribute positionAttribute;
    positionAttribute.location = 0;
    positionAttribute.componentCount = 3;
    positionAttribute.bufferIndex = 0;
    positionAttribute.byteOffset = 0;
    positionAttribute.componentType = GL_DOUBLE;
    positionAttribute.normalized = false;
    view.attributes.push_back(positionAttribute);

    ExternalVertexAttribute normalAttribute;
    normalAttribute.location = 1;
    normalAttribute.componentCount = 3;
    normalAttribute.bufferIndex = 1;
    normalAttribute.byteOffset = 0;
    normalAttribute.componentType = GL_DOUBLE;
    normalAttribute.normalized = false;
    view.attributes.push_back(normalAttribute);

    ExternalVertexAttribute uvAttribute;
    uvAttribute.location = 2;
    uvAttribute.componentCount = 2;
    uvAttribute.bufferIndex = 2;
    uvAttribute.byteOffset = 0;
    uvAttribute.componentType = GL_FLOAT;
    uvAttribute.normalized = false;
    view.attributes.push_back(uvAttribute);

    view.indices.data = &m_mesh->indices()[0];
    view.indices.byteSize = m_mesh->indices().size() * sizeof(std::uint32_t);
    view.indices.indexCount = static_cast<int>(m_mesh->indices().size());
    view.indices.indexType = GL_UNSIGNED_INT;

    return true;
}

ExternalMeshRevision ModelingMeshAdapter::structureRevision() const
{
    if (m_mesh == 0)
        return 0;

    return m_mesh->structureRevision();
}

ExternalMeshRevision ModelingMeshAdapter::contentRevision() const
{
    if (m_mesh == 0)
        return 0;

    return m_mesh->contentRevision();
}

bool ModelingMeshAdapter::changesSince(ExternalMeshRevision previousRevision, ExternalMeshChangeSet& changeSet) const
{
    if (m_mesh == 0)
        return false;

    std::vector<ModelingMeshChange> modelingChanges;

    if (!m_mesh->changesSince(previousRevision, modelingChanges))
        return false;

    changeSet.fromRevision = previousRevision;
    changeSet.toRevision = m_mesh->contentRevision();
    changeSet.dirtyRanges.clear();

    for (std::size_t i = 0; i < modelingChanges.size(); ++i)
    {
        const ModelingMeshChange& modelingChange = modelingChanges[i];

        ExternalMeshDirtyRange range;

        switch (modelingChange.buffer)
        {
        case ModelingMeshBufferPosition:
            range.bufferIndex = 0;
            break;

        case ModelingMeshBufferNormal:
            range.bufferIndex = 1;
            break;

        case ModelingMeshBufferUV:
            range.bufferIndex = 2;
            break;

        case ModelingMeshBufferIndex:
            range.bufferIndex = ExternalMeshIndexBuffer;
            break;
        }

        range.byteOffset = modelingChange.byteOffset;
        range.byteSize = modelingChange.byteSize;

        changeSet.dirtyRanges.push_back(range);
    }

    return true;
}