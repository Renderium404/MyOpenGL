#include "RenderLabel.h"

RenderLabel::RenderLabel(RenderLabelId id)
    : RenderPart(id)
{
    m_followCamera = true;
    m_pixelSize = true;

    m_depthTestMode = RenderPartStateMode::Disabled;
    m_depthWriteMode = RenderPartStateMode::Disabled;
}

RenderLabel::~RenderLabel()
{
    
}

/// Pixel Offset

const QPointF& RenderLabel::pixelOffset() const
{
    return m_pixelOffset;
}

void RenderLabel::setPixelOffset(const QPointF& offset)
{
    m_pixelOffset = offset;
}