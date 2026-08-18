#ifndef BUFFERGEOMETRYPICKSOURCE_H
#define BUFFERGEOMETRYPICKSOURCE_H

#include "PrimitivePicking.h"

class BufferGeometry;

/// BufferGeometry 的精确 Primitive Picking Adapter。
/// Triangles 使用 Ray-Triangle Picking；Lines / LineStrip 使用屏幕空间 Pixel-Tolerance Picking；所有 RenderType 都可对被索引 Vertex 执行 Point Snap。
/// 只借用当前 CPU Vertex / Index 数据，不复制 Geometry，也不访问 GPU Buffer。
class BufferGeometryPickSource : public PrimitivePickSource
{
public:
    explicit BufferGeometryPickSource(const BufferGeometry* geometry);

    /// 数据源
    const BufferGeometry* geometry() const;
    void setGeometry(const BufferGeometry* geometry); // 绑定借用的 BufferGeometry；传入 0 表示清除引用。

    /// Picking
    bool pickPrimitive(const PrimitivePickContext& context, PrimitivePickHit& hit) const override;
    bool pickPoint(const PrimitivePickContext& context, PointPickHit& hit) const override; // 对当前 Index Buffer 实际引用的 Geometry Vertex 执行 Pixel-Tolerance Snap。

private:
    bool findPositionValueOffset(int& valueOffset) const;

private:
    const BufferGeometry* m_geometry; // 当前借用的 BufferGeometry，不拥有该对象。
};

#endif // BUFFERGEOMETRYPICKSOURCE_H
