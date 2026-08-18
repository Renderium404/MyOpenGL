#include "RenderPart.h"

RenderPart::RenderPart(RenderPartId id)
    : m_id(id)
    , m_geometry(0)
    , m_primitivePickSource(0)
{
}

/// 基本信息

RenderPartId RenderPart::id() const
{
    return m_id;
}

/// 绘制引用

const Geometry* RenderPart::geometry() const
{
    return m_geometry;
}

void RenderPart::setGeometry(const Geometry* geometry)
{
    m_geometry = geometry;
}

/// Primitive Picking

const PrimitivePickSource* RenderPart::primitivePickSource() const
{
    return m_primitivePickSource;
}

void RenderPart::setPrimitivePickSource(const PrimitivePickSource* source)
{
    m_primitivePickSource = source;
}

/// Bounds

bool RenderPart::hasLocalBounds() const
{
    return m_localBounds.isValid();
}

const AxisAlignedBoundingBox& RenderPart::localBounds() const
{
    return m_localBounds;
}

void RenderPart::setLocalBounds(const AxisAlignedBoundingBox& bounds)
{
    if (!bounds.isValid())
    {
        m_localBounds.reset();
        return;
    }

    m_localBounds = bounds;
}

void RenderPart::clearLocalBounds()
{
    m_localBounds.reset();
}
