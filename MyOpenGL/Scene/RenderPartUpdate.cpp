#include "RenderPartUpdate.h"

RenderPartUpdate::RenderPartUpdate()
    : partId(DefaultRenderPartId)
    , operation(RenderPartUpdateRemove)
    , geometry(0)
{
}

RenderPartUpdate RenderPartUpdate::replacement(RenderPartId partIdValue, const Geometry* geometryValue, const AxisAlignedBoundingBox& localBoundsValue)
{
    RenderPartUpdate result;
    result.partId = partIdValue;
    result.operation = RenderPartUpdateReplace;
    result.geometry = geometryValue;
    result.localBounds = localBoundsValue;
    return result;
}

RenderPartUpdate RenderPartUpdate::removal(RenderPartId partIdValue)
{
    RenderPartUpdate result;
    result.partId = partIdValue;
    result.operation = RenderPartUpdateRemove;
    return result;
}

bool RenderPartUpdate::isValid() const
{
    if (operation == RenderPartUpdateReplace)
        return geometry != 0 && localBounds.isValid();

    if (operation == RenderPartUpdateRemove)
        return geometry == 0;

    return false;
}
