#include "RenderState.h"

RenderViewport::RenderViewport()
    : x(0)
    , y(0)
    , width(0)
    , height(0)
{
}

RenderViewport::RenderViewport(int xValue, int yValue, int widthValue, int heightValue)
    : x(xValue)
    , y(yValue)
    , width(widthValue)
    , height(heightValue)
{
}

bool RenderViewport::isValid() const
{
    return width > 0 && height > 0;
}

RenderState::RenderState()
    : depthTestEnabled(true)
    , depthWriteEnabled(true)
    , blendEnabled(false)
{
}