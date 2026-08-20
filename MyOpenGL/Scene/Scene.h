#ifndef SCENE_H
#define SCENE_H

#include "AxisAlignedBoundingBox.h"

#include <QString>
#include <vector>

class RenderItem;

/// 用户对象的扁平 Scene 容器。
/// Scene 只负责 RenderItem 的所有权、查询和整体 Bounds 聚合。
class Scene
{
public:
    Scene();
    ~Scene();

    /// Item 管理
    RenderItem* createItem(const QString& name);
    bool removeItem(RenderItem* item);
    void clear();

    /// Item 查询
    int itemCount() const;
    RenderItem* item(int index);
    const RenderItem* item(int index) const;

    /// Scene Bounds
    bool worldBounds(AxisAlignedBoundingBox& bounds, bool visibleOnly = true) const;

private:
    std::vector<RenderItem*> m_items;
};

#endif // SCENE_H
