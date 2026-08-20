#ifndef RENDERPART_H
#define RENDERPART_H

#include "AxisAlignedBoundingBox.h"

#include <cstdint>

class Geometry;

typedef std::uint64_t RenderPartId;

const RenderPartId DefaultRenderPartId = static_cast<RenderPartId>(0);

/// RenderItem 内具有稳定身份的最小模型交互单位。
/// Geometry 描述绘制数据，LocalBounds 描述 Part 在 Item Local Space 中的空间范围。
/// RenderPart 不拥有 Geometry。
class RenderPart
{
public:
    explicit RenderPart(RenderPartId id);

    /// Identity
    RenderPartId id() const;

    /// Geometry
    const Geometry* geometry() const;
    void setGeometry(const Geometry* geometry);

    /// Bounds
    bool hasLocalBounds() const;
    const AxisAlignedBoundingBox& localBounds() const;
    void setLocalBounds(const AxisAlignedBoundingBox& bounds);
    void clearLocalBounds();

private:
    RenderPartId m_id;
    const Geometry* m_geometry;
    AxisAlignedBoundingBox m_localBounds;
};

#endif // RENDERPART_H
