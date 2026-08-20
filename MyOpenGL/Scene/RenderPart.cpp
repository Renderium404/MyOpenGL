#include "RenderPart.h"

RenderPart::RenderPart(RenderPartId id)
    : m_id(id)
    , m_geometry(0)
{
}

RenderPartId RenderPart::id() const
{
    return m_id;
}

const Geometry* RenderPart::geometry() const
{
    return m_geometry;
}

void RenderPart::setGeometry(const Geometry* geometry)
{
    m_geometry = geometry;
}

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