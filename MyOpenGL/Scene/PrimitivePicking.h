#ifndef PRIMITIVEPICKING_H
#define PRIMITIVEPICKING_H

#include <QMatrix4x4>
#include <QOpenGLFunctions_3_3_Core>
#include <QPointF>
#include <QVector3D>

#include <cstddef>

/// 精确 Picking 返回的 Render Primitive 类型。
enum PrimitivePickType
{
    PrimitivePickTriangle,
    PrimitivePickLine
};

/// 获取 Primitive Picking 类型的调试名称。
const char* primitivePickTypeName(PrimitivePickType type);

/// PrimitivePickSource 一次查询所需的显式上下文。
/// Ray 使用当前 Geometry 局部坐标；Screen Position / Viewport 使用 Qt Widget Pixel 坐标。
/// localToClip 用于把当前 Geometry 局部点投影到屏幕，供具有 Pixel Tolerance 的 Primitive Picking 使用。
struct PrimitivePickContext
{
    PrimitivePickContext();

    QVector3D rayOrigin;      // 当前 Geometry 局部坐标 Ray Origin。
    QVector3D rayDirection;   // 当前 Geometry 局部坐标 Ray Direction。
    QPointF screenPosition;   // 当前鼠标位置，原点位于 Viewport 左上角。
    QMatrix4x4 localToClip;   // 当前 Geometry Local -> Clip Matrix。
    int viewportWidth;        // 当前 Viewport Pixel 宽度。
    int viewportHeight;       // 当前 Viewport Pixel 高度。
    float pixelTolerance;     // 屏幕空间 Picking 容差，单位 Pixel。
};

/// 单个 Render Primitive 的局部空间拾取结果。
/// PrimitiveIndex 只描述当前 Render Geometry 中的绘制图元，不代表 Modeling FaceId / EdgeId。
struct PrimitivePickHit
{
    PrimitivePickHit();

    PrimitivePickType type; // 当前命中的 Render Primitive 类型。
    int primitiveIndex;     // 当前 Geometry 内命中的 Primitive 序号。
    int vertexCount;        // 当前命中 Primitive 的顶点数量；Triangle=3，Line=2。
    QVector3D position;     // 命中点的 Geometry 局部坐标。
    QVector3D barycentric;  // Triangle 重心坐标；Line 命中时保持为零。
    QVector3D vertices[3];  // 命中 Primitive 的局部顶点；只读取前 vertexCount 个元素。
};

/// RenderItem 可选的精确 Primitive Picking 数据源。
/// MyOpenGL 只要求返回 Render Primitive，不规定这些 Primitive 如何映射到外部 Modeling Face / Edge。
/// 屏幕空间 Vertex / Endpoint Snap 命中结果。
/// VertexIndex 只表示当前 Render Geometry 的 Position Vertex，不等同于外部 Modeling VertexId。
struct PointPickHit
{
    PointPickHit();

    int vertexIndex;          // 当前 Geometry 内命中的 Vertex 序号。
    QVector3D position;       // 命中 Vertex 的 Geometry 局部坐标。
    float screenDistance;     // 鼠标到该 Vertex 屏幕投影的距离，单位 Pixel。
    float ndcDepth;           // 当前 Vertex 的 NDC Z，用于相同屏幕距离时选择更靠近 Camera 的点。
};

/// RenderItem 可选的精确 Primitive / Point Picking 数据源。
/// MyOpenGL 只要求返回 Render Primitive 或 Geometry Vertex，不规定它们如何映射到外部 Modeling Face / Edge / Vertex。
class PrimitivePickSource
{
public:
    virtual ~PrimitivePickSource()
    {
    }

    /// Primitive Picking
    virtual bool pickPrimitive(const PrimitivePickContext& context, PrimitivePickHit& hit) const = 0;

    /// Point Picking
    virtual bool pickPoint(const PrimitivePickContext& context, PointPickHit& hit) const
    {
        Q_UNUSED(context);
        Q_UNUSED(hit);
        return false;
    }
};

/// 通用 Triangle List 拾取只读视图。
/// 只借用 Position / Index CPU Memory，不取得所有权，也不要求数据采用 MyOpenGL 自有 Geometry 格式。
struct TrianglePickView
{
    TrianglePickView();

    const void* vertexData;         // 顶点 Buffer 首地址。
    std::size_t vertexByteSize;     // 顶点 Buffer 总 Byte 数。
    int vertexCount;                // 顶点数量。
    std::size_t vertexStride;       // 相邻 Vertex 起点的 Byte 距离。
    std::size_t positionByteOffset; // Position.xyz 在单个 Vertex 内的 Byte 偏移。
    GLenum positionType;            // Position 分量类型；当前支持 GL_FLOAT / GL_DOUBLE。

    const void* indexData;     // Triangle Index Buffer 首地址。
    std::size_t indexByteSize; // Index Buffer 总 Byte 数。
    int indexCount;            // Index 数量，Triangle List 必须为 3 的倍数。
    GLenum indexType;          // Index 类型；支持 GL_UNSIGNED_BYTE / SHORT / INT。
};

/// 对借用的 Triangle List CPU View 执行 Moller-Trumbore Ray-Triangle Picking。
/// 返回最近的正向命中；不执行 Back-face Culling，因此三角形正反两侧都可拾取。
bool raycastTriangles(const TrianglePickView& view, const QVector3D& rayOrigin, const QVector3D& rayDirection, PrimitivePickHit& hit);

/// 通用 Geometry Vertex Picking 只读视图。
/// 只借用 Position / Index CPU Memory；只扫描当前 Index Buffer 实际引用的 Vertex。
struct PointPickView
{
    PointPickView();

    const void* vertexData;         // 顶点 Buffer 首地址。
    std::size_t vertexByteSize;     // 顶点 Buffer 总 Byte 数。
    int vertexCount;                // 顶点数量。
    std::size_t vertexStride;       // 相邻 Vertex 起点的 Byte 距离。
    std::size_t positionByteOffset; // Position.xyz 在单个 Vertex 内的 Byte 偏移。
    GLenum positionType;            // Position 分量类型；当前支持 GL_FLOAT / GL_DOUBLE。

    const void* indexData;     // Index Buffer 首地址。
    std::size_t indexByteSize; // Index Buffer 总 Byte 数。
    int indexCount;            // 当前 Index 数量。
    GLenum indexType;          // Index 类型；支持 GL_UNSIGNED_BYTE / SHORT / INT。
};

/// 对借用 Geometry Vertex 执行屏幕空间 Pixel-Tolerance Picking。
/// 返回鼠标附近最近的被索引 Vertex；屏幕距离相同时选择更靠近 Camera 的 Vertex。
bool pickPoints(const PointPickView& view, const PrimitivePickContext& context, PointPickHit& hit);

/// Line Picking 的索引连接方式。
enum LinePickTopology
{
    LinePickSegments, // 每两个 Index 独立组成一条 Line。
    LinePickStrip     // 相邻 Index 连续组成 Line Segment。
};

/// 通用 Line / LineStrip 拾取只读视图。
/// 只借用 Position / Index CPU Memory，不取得所有权。
struct LinePickView
{
    LinePickView();

    const void* vertexData;         // 顶点 Buffer 首地址。
    std::size_t vertexByteSize;     // 顶点 Buffer 总 Byte 数。
    int vertexCount;                // 顶点数量。
    std::size_t vertexStride;       // 相邻 Vertex 起点的 Byte 距离。
    std::size_t positionByteOffset; // Position.xyz 在单个 Vertex 内的 Byte 偏移。
    GLenum positionType;            // Position 分量类型；当前支持 GL_FLOAT / GL_DOUBLE。

    const void* indexData;     // Line Index Buffer 首地址。
    std::size_t indexByteSize; // Index Buffer 总 Byte 数。
    int indexCount;            // 当前 Index 数量。
    GLenum indexType;          // Index 类型；支持 GL_UNSIGNED_BYTE / SHORT / INT。
};

/// 对借用的 Line CPU View 执行屏幕空间 Pixel-Tolerance Picking。
/// 选取落在 pixelTolerance 内的线段，并返回该线段上与鼠标投影最近的局部坐标点。
bool pickLines(const LinePickView& view, LinePickTopology topology, const PrimitivePickContext& context, PrimitivePickHit& hit);

#endif // PRIMITIVEPICKING_H
