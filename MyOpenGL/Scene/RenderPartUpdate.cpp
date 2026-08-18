#include "RenderPartUpdate.h"

RenderPartUpdate::RenderPartUpdate()
    : partId(DefaultRenderPartId)
    , operation(RenderPartUpdateRemove)
    , geometry(0)
{
}

/// Update 创建

RenderPartUpdate RenderPartUpdate::replacement(RenderPartId partIdValue, const Geometry* geometryValue)
{
    RenderPartUpdate result;
    result.partId = partIdValue;
    result.operation = RenderPartUpdateReplace;
    result.geometry = geometryValue;
    return result;
}

RenderPartUpdate RenderPartUpdate::removal(RenderPartId partIdValue)
{
    RenderPartUpdate result;
    result.partId = partIdValue;
    result.operation = RenderPartUpdateRemove;
    return result;
}

/// 状态判断

bool RenderPartUpdate::isValid() const
{
    if (operation == RenderPartUpdateReplace)
        return geometry != 0;

    if (operation == RenderPartUpdateRemove)
        return geometry == 0;

    return false;
}
