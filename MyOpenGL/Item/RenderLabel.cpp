#include "RenderLabel.h"

RenderLabel::RenderLabel(RenderLabelId id)
    : m_id(id)
    , m_anchorPosition(0.0f, 0.0f, 0.0f)
    , m_pixelOffset(0.0, 0.0)
    , m_geometry(0)
    , m_material(0)
    , m_visible(true)
{
}

RenderLabel::~RenderLabel()
{
}

/// Text

const QString& RenderLabel::text() const
{
    return m_text;
}

void RenderLabel::setText(const QString& text)
{
    m_text = text;
}

/// Anchor

const QVector3D& RenderLabel::anchorPosition() const
{
    return m_anchorPosition;
}

void RenderLabel::setAnchorPosition(const QVector3D& position)
{
    m_anchorPosition = position;
}

/// Screen Offset

const QPointF& RenderLabel::pixelOffset() const
{
    return m_pixelOffset;
}

void RenderLabel::setPixelOffset(const QPointF& offset)
{
    m_pixelOffset = offset;
}

/// Geometry

const Geometry* RenderLabel::geometry() const
{
    return m_geometry;
}

void RenderLabel::setGeometry(const Geometry* geometry)
{
    m_geometry = geometry;
}

/// Material

const Material* RenderLabel::material() const
{
    return m_material;
}

void RenderLabel::setMaterial(const Material* material)
{
    m_material = material;
}

/// Display

bool RenderLabel::isVisible() const
{
    return m_visible;
}

void RenderLabel::setVisible(bool visible)
{
    m_visible = visible;
}

/// State

bool RenderLabel::isRenderable() const
{
    return m_visible &&
           m_geometry != 0 &&
           m_material != 0;
}