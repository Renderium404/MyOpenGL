#ifndef MODELINGMESH_H
#define MODELINGMESH_H

#include <cstddef>
#include <cstdint>
#include <vector>

/// 模拟建模库自己的 3D Point 类型。
struct ModelingPoint
{
    double x;
    double y;
    double z;
};

/// 模拟建模库自己的 Normal 类型。
struct ModelingNormal
{
    double x;
    double y;
    double z;
};

/// 模拟建模库自己的 UV 类型。
struct ModelingUV
{
    float u;
    float v;
};

enum ModelingMeshBuffer
{
    ModelingMeshBufferPosition,
    ModelingMeshBufferNormal,
    ModelingMeshBufferUV,
    ModelingMeshBufferIndex
};

/// 建模库自己记录的局部变化。
struct ModelingMeshChange
{
    unsigned long long revision;  // 本次修改完成后的 Content Revision。
    ModelingMeshBuffer buffer;    // 被修改的数据数组。
    std::size_t byteOffset;       // 数组中的字节偏移。
    std::size_t byteSize;         // 修改字节数。
};

/// 模拟完全独立的外部建模 Mesh。
/// 它拥有自己的数据结构、Revision 和 Change History，不依赖 MyOpenGL。
class ModelingMesh
{
public:
    ModelingMesh();

    /// 建模操作
    void buildQuad(double halfWidth);             // 构建 4 Vertex / 6 Index Quad，并递增 Structure Revision。
    void buildSplitQuad(double halfWidth);        // 构建相同外轮廓的 6 Vertex / 12 Index Split Quad，并递增 Structure Revision。
    bool raiseVertex(int vertexIndex, double y);  // 修改已有 Position，只递增 Content Revision。

    /// Mesh 数据
    const std::vector<ModelingPoint>& positions() const;
    const std::vector<ModelingNormal>& normals() const;
    const std::vector<ModelingUV>& uvs() const;
    const std::vector<std::uint32_t>& indices() const;

    /// Revision
    unsigned long long structureRevision() const;
    unsigned long long contentRevision() const;

    /// Change History
    bool changesSince(unsigned long long previousRevision, std::vector<ModelingMeshChange>& changes) const;

private:
    void recordChange(ModelingMeshBuffer buffer, std::size_t byteOffset, std::size_t byteSize);

private:
    std::vector<ModelingPoint> m_positions;         // 建模库 Position 数据。
    std::vector<ModelingNormal> m_normals;          // 建模库 Normal 数据。
    std::vector<ModelingUV> m_uvs;                  // 建模库 UV 数据。
    std::vector<std::uint32_t> m_indices;           // 建模库 Triangle Index。
    std::vector<ModelingMeshChange> m_changes;      // 当前 Structure 生命周期内保留的 Content Change History。
    unsigned long long m_structureRevision;         // Data Layout / Address / Topology Revision。
    unsigned long long m_contentRevision;           // 当前 DataView 内字节内容 Revision。
    unsigned long long m_historyStartRevision;      // 当前 Change History 能够覆盖的最早 Revision。
};

#endif // MODELINGMESH_H