#include "ModelingMesh.h"

ModelingMesh::ModelingMesh()
    : m_structureRevision(0)
    , m_contentRevision(0)
    , m_historyStartRevision(0)
{
}

/// 建模操作

void ModelingMesh::buildQuad(double halfWidth)
{
    m_positions.clear();
    m_normals.clear();
    m_uvs.clear();
    m_indices.clear();
    m_changes.clear();

    ModelingPoint point;

    // Vertex 0：左下。
    point.x = -halfWidth;
    point.y = 0.0;
    point.z = 0.0;
    m_positions.push_back(point);

    // Vertex 1：右下。
    point.x = halfWidth;
    point.y = 0.0;
    point.z = 0.0;
    m_positions.push_back(point);

    // Vertex 2：右上。
    point.x = halfWidth;
    point.y = 2.0;
    point.z = 0.0;
    m_positions.push_back(point);

    // Vertex 3：左上。
    point.x = -halfWidth;
    point.y = 2.0;
    point.z = 0.0;
    m_positions.push_back(point);

    for (int i = 0; i < 4; ++i)
    {
        ModelingNormal normal;
        normal.x = 0.0;
        normal.y = 0.0;
        normal.z = 1.0;
        m_normals.push_back(normal);
    }

    ModelingUV uv;

    uv.u = 0.0f;
    uv.v = 0.0f;
    m_uvs.push_back(uv);

    uv.u = 1.0f;
    uv.v = 0.0f;
    m_uvs.push_back(uv);

    uv.u = 1.0f;
    uv.v = 1.0f;
    m_uvs.push_back(uv);

    uv.u = 0.0f;
    uv.v = 1.0f;
    m_uvs.push_back(uv);

    m_indices.push_back(0);
    m_indices.push_back(1);
    m_indices.push_back(2);

    m_indices.push_back(0);
    m_indices.push_back(2);
    m_indices.push_back(3);

    // 重建可能改变 vector 地址、Buffer Size 或 Topology，因此 Structure 和 Content 都递增。
    ++m_structureRevision;
    ++m_contentRevision;

    m_historyStartRevision = m_contentRevision;
}

void ModelingMesh::buildSplitQuad(double halfWidth)
{
    m_positions.clear();
    m_normals.clear();
    m_uvs.clear();
    m_indices.clear();
    m_changes.clear();

    ModelingPoint point;

    // 前 4 个 Vertex 与 buildQuad() 保持完全相同的几何语义。
    // 因此 Vertex 2 在两种 Topology 下始终表示右上角。
    point.x = -halfWidth;
    point.y = 0.0;
    point.z = 0.0;
    m_positions.push_back(point);

    point.x = halfWidth;
    point.y = 0.0;
    point.z = 0.0;
    m_positions.push_back(point);

    point.x = halfWidth;
    point.y = 2.0;
    point.z = 0.0;
    m_positions.push_back(point);

    point.x = -halfWidth;
    point.y = 2.0;
    point.z = 0.0;
    m_positions.push_back(point);

    // Vertex 4：下边中点。
    point.x = 0.0;
    point.y = 0.0;
    point.z = 0.0;
    m_positions.push_back(point);

    // Vertex 5：上边中点。
    point.x = 0.0;
    point.y = 2.0;
    point.z = 0.0;
    m_positions.push_back(point);

    for (int i = 0; i < 6; ++i)
    {
        ModelingNormal normal;
        normal.x = 0.0;
        normal.y = 0.0;
        normal.z = 1.0;
        m_normals.push_back(normal);
    }

    ModelingUV uv;

    uv.u = 0.0f;
    uv.v = 0.0f;
    m_uvs.push_back(uv);

    uv.u = 1.0f;
    uv.v = 0.0f;
    m_uvs.push_back(uv);

    uv.u = 1.0f;
    uv.v = 1.0f;
    m_uvs.push_back(uv);

    uv.u = 0.0f;
    uv.v = 1.0f;
    m_uvs.push_back(uv);

    uv.u = 0.5f;
    uv.v = 0.0f;
    m_uvs.push_back(uv);

    uv.u = 0.5f;
    uv.v = 1.0f;
    m_uvs.push_back(uv);

    // 左侧 Quad。
    m_indices.push_back(0);
    m_indices.push_back(4);
    m_indices.push_back(5);

    m_indices.push_back(0);
    m_indices.push_back(5);
    m_indices.push_back(3);

    // 右侧 Quad。
    m_indices.push_back(4);
    m_indices.push_back(1);
    m_indices.push_back(2);

    m_indices.push_back(4);
    m_indices.push_back(2);
    m_indices.push_back(5);

    // Vertex Count、Index Count 和 Topology 都发生变化，因此属于 Structure Change。
    ++m_structureRevision;
    ++m_contentRevision;

    m_historyStartRevision = m_contentRevision;
}

bool ModelingMesh::raiseVertex(int vertexIndex, double y)
{
    if (vertexIndex < 0 || vertexIndex >= static_cast<int>(m_positions.size()))
        return false;

    m_positions[vertexIndex].y += y;

    const std::size_t byteOffset = static_cast<std::size_t>(vertexIndex) * sizeof(ModelingPoint);
    recordChange(ModelingMeshBufferPosition, byteOffset, sizeof(ModelingPoint));

    return true;
}

/// Mesh 数据

const std::vector<ModelingPoint>& ModelingMesh::positions() const
{
    return m_positions;
}

const std::vector<ModelingNormal>& ModelingMesh::normals() const
{
    return m_normals;
}

const std::vector<ModelingUV>& ModelingMesh::uvs() const
{
    return m_uvs;
}

const std::vector<std::uint32_t>& ModelingMesh::indices() const
{
    return m_indices;
}

/// Revision

unsigned long long ModelingMesh::structureRevision() const
{
    return m_structureRevision;
}

unsigned long long ModelingMesh::contentRevision() const
{
    return m_contentRevision;
}

/// Change History

bool ModelingMesh::changesSince(unsigned long long previousRevision, std::vector<ModelingMeshChange>& changes) const
{
    changes.clear();

    if (previousRevision > m_contentRevision)
        return false;

    if (previousRevision == m_contentRevision)
        return true;

    // Structure 重建后旧 Change History 已经被清除，此时调用者应执行 Full Update。
    if (previousRevision < m_historyStartRevision)
        return false;

    for (std::size_t i = 0; i < m_changes.size(); ++i)
    {
        if (m_changes[i].revision > previousRevision)
            changes.push_back(m_changes[i]);
    }

    return true;
}

/// 内部辅助

void ModelingMesh::recordChange(ModelingMeshBuffer buffer, std::size_t byteOffset, std::size_t byteSize)
{
    ++m_contentRevision;

    ModelingMeshChange change;
    change.revision = m_contentRevision;
    change.buffer = buffer;
    change.byteOffset = byteOffset;
    change.byteSize = byteSize;

    m_changes.push_back(change);
}