#ifndef RENDERPART_H
#define RENDERPART_H

#include "AxisAlignedBoundingBox.h"

#include <cstdint>

class Geometry;
class PrimitivePickSource;

typedef std::uint64_t RenderPartId;

// PartId=0 保留给旧单 Geometry RenderItem 的默认 Part，同时也是外部系统可以合法使用的稳定 PartId。
const RenderPartId DefaultRenderPartId = static_cast<RenderPartId>(0);

/// RenderItem 内部一个具有稳定身份的绘制分片。
/// Geometry 是基础显示数据；PrimitivePickSource 和 LocalBounds 是按需绑定的可选 Scene 辅助数据。
/// RenderPart 不拥有 Geometry 或 PrimitivePickSource。
class RenderPart
{
public:
    explicit RenderPart(RenderPartId id);

    /// 基本信息
    RenderPartId id() const;

    /// 绘制引用
    const Geometry* geometry() const;
    void setGeometry(const Geometry* geometry); // 绑定借用的 Geometry；传入 0 表示当前 Part 暂无可绘制 Geometry。

    /// Primitive Picking
    const PrimitivePickSource* primitivePickSource() const;
    void setPrimitivePickSource(const PrimitivePickSource* source); // 可选：绑定精确 Primitive / Point Picker；传入 0 清除，RenderPart 不拥有该对象。

    /// Bounds
    bool hasLocalBounds() const;
    const AxisAlignedBoundingBox& localBounds() const;
    void setLocalBounds(const AxisAlignedBoundingBox& bounds); // 可选：设置 Item Local Space Bounds；无效 Bounds 会清除。
    void clearLocalBounds();

private:
    RenderPartId m_id;                                // 当前 Item 内稳定 Part 标识。
    const Geometry* m_geometry;                       // 当前借用 Geometry，不拥有该对象。
    const PrimitivePickSource* m_primitivePickSource; // 当前借用 Picker；0 表示该 Part 不参与精确 Primitive / Point Picking。
    AxisAlignedBoundingBox m_localBounds;             // 当前 Part 在 Item Local Space 中的 Bounds。
};

#endif // RENDERPART_H
