#ifndef ITEMMANAGER_H
#define ITEMMANAGER_H

#include "AxisAlignedBoundingBox.h"
#include "RenderItem.h"

#include <cstddef>
#include <map>
#include <vector>

/// 用户 RenderItem 的扁平管理器。
/// ItemManager 负责 RenderItem 的创建、RenderItemId、所有权、查询和整体 Bounds 聚合。
class ItemManager
{
public:
    ItemManager();
    ~ItemManager();

    /// Item 管理
    RenderItem* createItem(const QString& name = "RenderItem");
    RenderItem* get(RenderItemId id);
    const RenderItem* get(RenderItemId id) const;
    bool contains(RenderItemId id) const;
    std::size_t count() const;
    bool remove(RenderItemId id);
    void clear();

    /// Item 查询

    /// 按创建顺序访问 RenderItem；索引非法时返回 0。
    RenderItem* itemAt(int index);
    const RenderItem* itemAt(int index) const;

    /// Item Bounds
    bool worldBounds(AxisAlignedBoundingBox& bounds, bool visibleOnly = true) const;

private:
    typedef std::map<RenderItemId, RenderItem*> ItemMap;

    RenderItemId allocateId();

private:
    std::vector<RenderItem*> m_items; // 当前管理的全部 RenderItem，保持创建顺序。
    ItemMap m_itemsById;              // RenderItemId 到 RenderItem 的快速查询。
    RenderItemId m_nextId;            // 下一个可分配 RenderItemId。
};

#endif // ITEMMANAGER_H
