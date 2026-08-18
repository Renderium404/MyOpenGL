#include "BufferGeometryPickSource.h"

#include "MyOpenGL/Resource/BufferGeometry.h"

#include <vector>

BufferGeometryPickSource::BufferGeometryPickSource(const BufferGeometry* geometry)
    : m_geometry(geometry)
{
}

/// 数据源

const BufferGeometry* BufferGeometryPickSource::geometry() const
{
    return m_geometry;
}

void BufferGeometryPickSource::setGeometry(const BufferGeometry* geometry)
{
    m_geometry = geometry;
}

/// Primitive Picking

bool BufferGeometryPickSource::pickPrimitive(const PrimitivePickContext& context, PrimitivePickHit& hit) const
{
    if (m_geometry == 0)
        return false;

    int positionValueOffset = -1;

    if (!findPositionValueOffset(positionValueOffset))
        return false;

    const std::vector<GLfloat>& vertices = m_geometry->vertexData();
    const std::vector<GLuint>& indices = m_geometry->indexData();

    if (vertices.empty() || indices.empty())
        return false;

    switch (m_geometry->renderType())
    {
    case Triangles:
    {
        TrianglePickView view;
        view.vertexData = &vertices[0];
        view.vertexByteSize = vertices.size() * sizeof(GLfloat);
        view.vertexCount = m_geometry->vertexCount();
        view.vertexStride = static_cast<std::size_t>(m_geometry->valuesPerVertex()) * sizeof(GLfloat);
        view.positionByteOffset = static_cast<std::size_t>(positionValueOffset) * sizeof(GLfloat);
        view.positionType = GL_FLOAT;
        view.indexData = &indices[0];
        view.indexByteSize = indices.size() * sizeof(GLuint);
        view.indexCount = static_cast<int>(indices.size());
        view.indexType = GL_UNSIGNED_INT;

        return raycastTriangles(view, context.rayOrigin, context.rayDirection, hit);
    }

    case Lines:
    case LineStrip:
    {
        LinePickView view;
        view.vertexData = &vertices[0];
        view.vertexByteSize = vertices.size() * sizeof(GLfloat);
        view.vertexCount = m_geometry->vertexCount();
        view.vertexStride = static_cast<std::size_t>(m_geometry->valuesPerVertex()) * sizeof(GLfloat);
        view.positionByteOffset = static_cast<std::size_t>(positionValueOffset) * sizeof(GLfloat);
        view.positionType = GL_FLOAT;
        view.indexData = &indices[0];
        view.indexByteSize = indices.size() * sizeof(GLuint);
        view.indexCount = static_cast<int>(indices.size());
        view.indexType = GL_UNSIGNED_INT;

        const LinePickTopology topology = m_geometry->renderType() == Lines ? LinePickSegments : LinePickStrip;
        return pickLines(view, topology, context, hit);
    }
    }

    return false;
}

bool BufferGeometryPickSource::pickPoint(const PrimitivePickContext& context, PointPickHit& hit) const
{
    if (m_geometry == 0)
        return false;

    int positionValueOffset = -1;

    if (!findPositionValueOffset(positionValueOffset))
        return false;

    const std::vector<GLfloat>& vertices = m_geometry->vertexData();
    const std::vector<GLuint>& indices = m_geometry->indexData();

    if (vertices.empty() || indices.empty())
        return false;

    PointPickView view;
    view.vertexData = &vertices[0];
    view.vertexByteSize = vertices.size() * sizeof(GLfloat);
    view.vertexCount = m_geometry->vertexCount();
    view.vertexStride = static_cast<std::size_t>(m_geometry->valuesPerVertex()) * sizeof(GLfloat);
    view.positionByteOffset = static_cast<std::size_t>(positionValueOffset) * sizeof(GLfloat);
    view.positionType = GL_FLOAT;
    view.indexData = &indices[0];
    view.indexByteSize = indices.size() * sizeof(GLuint);
    view.indexCount = static_cast<int>(indices.size());
    view.indexType = GL_UNSIGNED_INT;

    return pickPoints(view, context, hit);
}

/// 内部辅助

bool BufferGeometryPickSource::findPositionValueOffset(int& valueOffset) const
{
    valueOffset = -1;

    if (m_geometry == 0)
        return false;

    const std::vector<GeometryVertexAttribute>& attributes = m_geometry->attributes();

    for (std::size_t i = 0; i < attributes.size(); ++i)
    {
        if (attributes[i].location == 0 && attributes[i].componentCount >= 3)
        {
            valueOffset = attributes[i].valueOffset;
            return true;
        }
    }

    return false;
}
