#include "RenderLabel.h"

RenderLabel::RenderLabel(RenderLabelId id)
    : m_id(id)
    , m_anchorWorld(0.0f, 0.0f, 0.0f)
    , m_anchorScence(0.0f, 0.0f)
    , m_pixelOffset(0.0, 0.0)
    , m_geometry(0)
    , m_material(0)
    , m_visible(true)
{
}

RenderLabel::~RenderLabel()
{
}

const QVector3D& RenderLabel::anchorWorld() const
{
    return m_anchorWorld;
}

void RenderLabel::setAnchorWorld(const QVector3D& anchor)
{
    m_anchorWorld = anchor;
}

const QVector2D& RenderLabel::anchorSence() const
{
    return m_anchorScence;
}

void RenderLabel::setAnchorSence(const QVector2D& anchor)
{
    m_anchorScence = anchor;
}

const QPointF& RenderLabel::pixelOffset() const
{
    return m_pixelOffset;
}

void RenderLabel::setPixelOffset(const QPointF& offset)
{
    m_pixelOffset = offset;
}

const Geometry* RenderLabel::geometry() const
{
    return m_geometry;
}

void RenderLabel::setGeometry(const Geometry* geometry)
{
    m_geometry = geometry;
}

const Material* RenderLabel::material() const
{
    return m_material;
}

void RenderLabel::setMaterial(const Material* material)
{
    m_material = material;
}

bool RenderLabel::isVisible() const
{
    return m_visible;
}

void RenderLabel::setVisible(bool visible)
{
    m_visible = visible;
}

bool RenderLabel::isRenderable() const
{
    return m_visible && m_geometry != 0 && m_material != 0;
}