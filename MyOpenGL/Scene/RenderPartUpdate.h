#ifndef RENDERPARTUPDATE_H
#define RENDERPARTUPDATE_H

#include "RenderPart.h"

class Geometry;

/// RenderItem Part 结构更新操作。
/// Replace 对不存在 Part 表示 Add，对已存在 Part 表示替换当前 Geometry。
enum RenderPartUpdateOperation
{
    RenderPartUpdateReplace,
    RenderPartUpdateRemove
};

/// 描述一次 RenderItem 内稳定 PartId 的基础显示更新。
/// Update 只负责 Part 与 Geometry 的绑定关系；Picking、Bounds 等可选能力不进入基础更新接口。
struct RenderPartUpdate
{
    RenderPartUpdate();

    /// Update 创建
    static RenderPartUpdate replacement(RenderPartId partId, const Geometry* geometry);
    static RenderPartUpdate removal(RenderPartId partId);

    /// 状态判断
    bool isValid() const; // Replace 必须具有 Geometry；Remove 必须不携带 Geometry。

    RenderPartId partId;                 // 目标 Item 内稳定 RenderPartId。
    RenderPartUpdateOperation operation; // Replace / Remove。
    const Geometry* geometry;            // Replace 使用的借用 Geometry；Remove 必须为 0。
};

#endif // RENDERPARTUPDATE_H
