#ifndef RENDERPARTUPDATE_H
#define RENDERPARTUPDATE_H

#include "RenderPart.h"

class Geometry;

enum RenderPartUpdateOperation
{
    RenderPartUpdateReplace,
    RenderPartUpdateRemove
};

/// 描述一次 RenderItem 内稳定 PartId 的内容更新。
/// Replace 同时提交 Geometry 和 LocalBounds，保证 Part 的绘制数据与空间数据保持一致。
struct RenderPartUpdate
{
    RenderPartUpdate();

    static RenderPartUpdate replacement(RenderPartId partId, const Geometry* geometry, const AxisAlignedBoundingBox& localBounds);
    static RenderPartUpdate removal(RenderPartId partId);

    bool isValid() const;

    RenderPartId partId;
    RenderPartUpdateOperation operation;

    const Geometry* geometry;
    AxisAlignedBoundingBox localBounds;
};

#endif // RENDERPARTUPDATE_H