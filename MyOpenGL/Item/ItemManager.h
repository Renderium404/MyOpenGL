#ifndef ITEMMANAGER_H
#define ITEMMANAGER_H

#include "AxisAlignedBoundingBox.h"
#include "RenderItem.h"

#include <cstddef>
#include <map>
#include <vector>

class MaterialManager;
class RenderPointCloud;
class ResourceManager;

/// RenderItem / RenderPart 的统一生命周期管理器。
/// ItemManager 拥有全部 RenderItem 和 RenderPart；RenderItem 只组织 Part 的非拥有引用。
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
    // 删除 Item，不删除其组织的 Part。
    bool remove(RenderItemId id);
    void clear();

    /// Item 查询

    RenderItem* itemAt(int index);
    const RenderItem* itemAt(int index) const;

    /// Part 生命周期

    // 创建全局普通 Part；Part 创建后不自动属于任何 Item。
    RenderPart* createPart();
    // 创建全局 Label；Label 创建后不自动属于任何 Item。
    RenderLabel* createLabel();
    // 创建文本 Label 及其显示资源；Label 创建后不自动属于任何 Item。
    RenderLabel* createTextLabel(ResourceManager& resourceManager, MaterialManager& materialManager,
                                 const QString& text, int textPixelSize = 16);
    // 创建全局 RenderPointCloud；创建后不自动属于任何 Item。
    RenderPointCloud* createRenderPointCloud();

    RenderPart* getPart(RenderPartId id);
    const RenderPart* getPart(RenderPartId id) const;
    bool containsPart(RenderPartId id) const;
    std::size_t partCount() const;

    // 删除全局 Part；删除前自动从全部 Item 解除引用。
    bool removePart(RenderPartId id);
    // 返回当前有多少 Item 正在组织指定 Part。
    std::size_t partItemCount(RenderPartId id) const;

    /// Item Bounds

    bool worldBounds(AxisAlignedBoundingBox& bounds, bool visibleOnly = true) const;

private:
    enum class PartKind
    {
        Standard,
        Label
    };

    struct PartRecord
    {
        PartRecord();

        RenderPart* part;
        PartKind kind;
    };

    typedef std::map<RenderItemId, RenderItem*> ItemMap;
    typedef std::map<RenderPartId, PartRecord> PartMap;

    RenderItemId allocateItemId();
    RenderPartId allocatePartId();
    bool registerPart(RenderPart* part, PartKind kind);

private:
    std::vector<RenderItem*> m_items; // 当前管理的全部 RenderItem，保持创建顺序。
    ItemMap m_itemsById;              // RenderItemId 到 RenderItem 的快速查询。
    RenderItemId m_nextItemId;        // 下一个可分配 RenderItemId。

    PartMap m_partsById;              // 全局 RenderPartId 到 Part 记录。
    RenderPartId m_nextPartId;        // 下一个可分配全局 RenderPartId。
};

#endif // ITEMMANAGER_H
