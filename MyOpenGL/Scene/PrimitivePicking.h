#ifndef PRIMITIVEPICKING_H
#define PRIMITIVEPICKING_H

#include <QOpenGLFunctions_3_3_Core>
#include <QVector3D>

#include <cstddef>

/// 单个 Render Primitive 的局部空间拾取结果。
/// PrimitiveIndex 只描述当前 Render Geometry 中的绘制图元，不代表 Modeling FaceId。
struct PrimitivePickHit
{
    PrimitivePickHit();

    int primitiveIndex;          // 命中的 Render Primitive 序号；Triangle Mesh 中每三个 Index 对应一个 Primitive。
    QVector3D position;          // 命中点的 Mesh 局部坐标。
    QVector3D barycentric;       // Triangle 三个顶点对应的重心坐标；x+y+z 约等于 1。
    QVector3D vertices[3];       // 命中 Triangle 的三个局部坐标顶点，顺序与 Index Buffer 一致。
};

/// RenderItem 可选的精确 Primitive Picking 数据源。
/// MyOpenGL 只要求返回 Render Primitive，不规定这些 Primitive 如何映射到外部 Modeling Face。
class PrimitivePickSource
{
public:
    virtual ~PrimitivePickSource()
    {
    }

    /// Primitive Picking
    virtual bool raycastPrimitive(const QVector3D& rayOrigin, const QVector3D& rayDirection, PrimitivePickHit& hit) const = 0; // 输入和输出均使用 Mesh 局部坐标。
};

/// 通用 Triangle Mesh 拾取只读视图。
/// 只借用 Position / Index CPU Memory，不取得所有权，也不要求数据采用 MyOpenGL 自有 Mesh 格式。
struct TriangleMeshPickView
{
    TriangleMeshPickView();

    const void* vertexData;          // 顶点 Buffer 首地址。
    std::size_t vertexByteSize;      // 顶点 Buffer 总 Byte 数。
    int vertexCount;                 // 顶点数量。
    std::size_t vertexStride;        // 相邻 Vertex 起点的 Byte 距离。
    std::size_t positionByteOffset;  // Position.xyz 在单个 Vertex 内的 Byte 偏移。
    GLenum positionType;             // Position 分量类型；当前支持 GL_FLOAT / GL_DOUBLE。

    const void* indexData;           // Triangle Index Buffer 首地址。
    std::size_t indexByteSize;       // Index Buffer 总 Byte 数。
    int indexCount;                  // Index 数量，Triangle Mesh 必须为 3 的倍数。
    GLenum indexType;                // Index 类型；支持 GL_UNSIGNED_BYTE / SHORT / INT。
};

/// 对借用的 Triangle Mesh CPU View 执行 Moller-Trumbore Ray-Triangle Picking。
/// 返回最近的正向命中；不执行 Back-face Culling，因此三角形正反两侧都可拾取。
bool raycastTriangleMesh(const TriangleMeshPickView& view, const QVector3D& rayOrigin, const QVector3D& rayDirection, PrimitivePickHit& hit);

#endif // PRIMITIVEPICKING_H