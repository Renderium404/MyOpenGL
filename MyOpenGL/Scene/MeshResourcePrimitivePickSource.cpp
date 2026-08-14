#include "MeshResourcePrimitivePickSource.h"

#include "Resource/MeshResource.h"

#include <vector>

MeshResourcePrimitivePickSource::MeshResourcePrimitivePickSource(const MeshResource* mesh)
    : m_mesh(mesh)
{
}

/// 数据源

const MeshResource* MeshResourcePrimitivePickSource::mesh() const
{
    return m_mesh;
}

void MeshResourcePrimitivePickSource::setMesh(const MeshResource* mesh)
{
    m_mesh = mesh;
}

/// Primitive Picking

bool MeshResourcePrimitivePickSource::raycastPrimitive(const QVector3D& rayOrigin, const QVector3D& rayDirection, PrimitivePickHit& hit) const
{
    if (m_mesh == 0 || m_mesh->primitiveType() != MeshPrimitiveTriangles)
        return false;

    const std::vector<MeshVertexAttribute>& attributes = m_mesh->attributes();
    int positionValueOffset = -1;

    for (std::size_t i = 0; i < attributes.size(); ++i)
    {
        if (attributes[i].location == 0 && attributes[i].componentCount >= 3)
        {
            positionValueOffset = attributes[i].valueOffset;
            break;
        }
    }

    if (positionValueOffset < 0)
        return false;

    const std::vector<GLfloat>& vertices = m_mesh->vertexData();
    const std::vector<GLuint>& indices = m_mesh->indexData();

    if (vertices.empty() || indices.empty())
        return false;

    TriangleMeshPickView view;
    view.vertexData = &vertices[0];
    view.vertexByteSize = vertices.size() * sizeof(GLfloat);
    view.vertexCount = m_mesh->vertexCount();
    view.vertexStride = static_cast<std::size_t>(m_mesh->valuesPerVertex()) * sizeof(GLfloat);
    view.positionByteOffset = static_cast<std::size_t>(positionValueOffset) * sizeof(GLfloat);
    view.positionType = GL_FLOAT;
    view.indexData = &indices[0];
    view.indexByteSize = indices.size() * sizeof(GLuint);
    view.indexCount = static_cast<int>(indices.size());
    view.indexType = GL_UNSIGNED_INT;

    return raycastTriangleMesh(view, rayOrigin, rayDirection, hit);
}
