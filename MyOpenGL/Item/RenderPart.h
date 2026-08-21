#ifndef RENDERPART_H
#define RENDERPART_H

#include "AxisAlignedBoundingBox.h"

#include <cstdint>

class Geometry;
class RenderItem;

/// RenderPart 唯一标识类型，由 RenderItem 统一分配。
typedef std::uint64_t RenderPartId;

/// 无效 RenderPart ID。
const RenderPartId InvalidRenderPartId = static_cast<RenderPartId>(0);

/// RenderItem 内具有稳定身份的最小模型交互单位。
/// Geometry 描述绘制数据，LocalBounds 描述 Part 在 Item Local Space 中的空间范围。
/// RenderPart 不拥有 Geometry。
class RenderPart
{
public:
    /// Identity
    RenderPartId id() const { return m_id; }

    /// Geometry
    const Geometry* geometry() const;
    void setGeometry(const Geometry* geometry);

    /// Bounds
    bool hasLocalBounds() const;
    const AxisAlignedBoundingBox& localBounds() const;
    void setLocalBounds(const AxisAlignedBoundingBox& bounds);
    void clearLocalBounds();

private:
    friend class RenderItem;

    /// RenderItem 内部接口
    explicit RenderPart(RenderPartId id);
    ~RenderPart();

private:
    RenderPartId m_id;                     // Part 唯一 ID。
    const Geometry* m_geometry;            // Part 借用 Geometry，不拥有。
    AxisAlignedBoundingBox m_localBounds;  // Item Local Space Bounds。
};

#endif // RENDERPART_H