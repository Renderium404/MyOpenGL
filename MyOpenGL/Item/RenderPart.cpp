#include "RenderPart.h"

RenderPart::RenderPart(RenderPartId id)
    : m_id(id)
{
}

RenderPart::~RenderPart()
{
}

/// Identity

RenderPartId RenderPart::id() const
{
    return m_id;
}

/// Geometry

const Geometry* RenderPart::geometry() const
{
    return m_geometry;
}

void RenderPart::setGeometry(const Geometry* geometry)
{
    m_geometry = geometry;
}

/// Material

const Material* RenderPart::material() const
{
    return m_material;
}

void RenderPart::setMaterial(const Material* material)
{
    m_material = material;
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

/// Anchor

const QVector3D& RenderPart::anchor3D() const
{
    return m_anchor3D;
}

void RenderPart::setAnchor3D(const QVector3D& anchor)
{
    m_anchor3D = anchor;
}

const QVector2D& RenderPart::anchor2D() const
{
    return m_anchor2D;
}

void RenderPart::setAnchor2D(const QVector2D& anchor)
{
    m_anchor2D = anchor;
}

/// Display Space

bool RenderPart::followCamera() const
{
    return m_followCamera;
}

void RenderPart::setFollowCamera(bool enabled)
{
    m_followCamera = enabled;
}

bool RenderPart::pixelSize() const
{
    return m_pixelSize;
}

void RenderPart::setPixelSize(bool enabled)
{
    m_pixelSize = enabled;
}

bool RenderPart::isStandardModel() const
{
    return !m_followCamera && !m_pixelSize;
}

/// Depth

RenderPartStateMode RenderPart::depthTestMode() const
{
    return m_depthTestMode;
}

void RenderPart::setDepthTestMode(RenderPartStateMode mode)
{
    m_depthTestMode = mode;
}

RenderPartStateMode RenderPart::depthWriteMode() const
{
    return m_depthWriteMode;
}

void RenderPart::setDepthWriteMode(RenderPartStateMode mode)
{
    m_depthWriteMode = mode;
}