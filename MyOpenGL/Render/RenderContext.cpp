#include "RenderContext.h"

RenderContext::RenderContext()
    : cameraPosition(0.0f, 0.0f, 0.0f)
    , cameraForward(0.0f, 0.0f, -1.0f)
    , cameraUp(0.0f, 1.0f, 0.0f)
    , viewportWidth(0)
    , viewportHeight(0)
{
}

bool RenderContext::isValid() const
{
    return viewportWidth > 0 && viewportHeight > 0;
}