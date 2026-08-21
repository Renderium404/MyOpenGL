#ifndef RENDERITEM_H
#define RENDERITEM_H

#include "RenderPart.h"
#include "RenderPartUpdate.h"
#include "Transform.h"

#include <QString>
#include <QVector3D>
#include <QVector4D>

#include <map>
#include <vector>

class ItemManager;
class Material;

/// RenderItem 唯一标识类型，由 ItemManager 统一分配。
typedef unsigned int RenderItemId;

/// 无效 RenderItem ID。
const RenderItemId InvalidRenderItemId = 0;

/// RenderItem 的整体显示模式。
/// 当前作用于整个 Item，不改变 RenderPart 的 Geometry 数据和交互身份。
enum class DisplayMode
{
    Shaded,         // 按当前 Material 正常绘制。
    Wireframe,      // Triangle Geometry 以统一边线形式绘制。
    ShadedWithEdges // 先绘制表面，再叠加统一边线。
};

/// 一次 RenderItem Bounds Raycast 的最近命中结果。
/// RenderPart 是 Item 内最小用户交互模型单位，因此命中结果明确返回 PartId。
/// distance 为 World Ray Origin 到命中点的世界空间前向距离；position 为世界空间命中坐标。
struct RenderItemRayHit
{
    RenderItemRayHit();

    RenderPartId partId; // 命中的 RenderPart 稳定标识。
    float distance;      // World Space Ray Distance。
    QVector3D position;  // World Space Hit Position。
};

/// ItemManager 中一个完整的模型对象实例。
/// RenderItem 拥有并组织多个 RenderPart；RenderPart 是 Item 内具有稳定身份的最小用户交互模型单位。
/// Item 自身统一保存 Transform、Material 和整体显示状态。
/// RenderItem 不拥有 RenderPart 引用的 Geometry，也不拥有 Material。
class RenderItem
{
public:
    /// 基本信息
    RenderItemId id() const { return m_id; }
    const QString& name() const { return m_name; } // 返回 Item 的稳定调试名称。

    QString type() const;
    DisplayMode displayMode() const { return m_type; }

    /// Part 管理

    /// 创建并接管一个新的 RenderPart。
    /// RenderPartId 由当前 RenderItem 自动分配；失败返回 0。
    RenderPart* createPart();

    /// 删除指定 RenderPart。
    /// 只删除 RenderPart 本身，不删除其借用的 Geometry。
    bool removePart(RenderPartId id);

    /// 删除当前 Item 拥有的全部 RenderPart。
    void clearParts();

    /// 返回当前 RenderPart 数量。
    int partCount() const;

    /// 判断指定 RenderPartId 是否属于当前 Item。
    bool containsPart(RenderPartId id) const;

    /// 按创建顺序访问 RenderPart；索引非法时返回 0。
    RenderPart* partAt(int index);
    const RenderPart* partAt(int index) const;

    /// 按稳定 RenderPartId 查询 RenderPart；不存在时返回 0。
    RenderPart* part(RenderPartId id);
    const RenderPart* part(RenderPartId id) const;

    /// Part Update

    /// 应用一个 RenderPart 更新。
    /// Replace 会同时更新指定 PartId 的 Geometry 与 LocalBounds；
    /// Remove 会删除指定 PartId。
    bool applyPartUpdate(const RenderPartUpdate& update);

    /// 批量应用 RenderPart 更新。
    /// 在实际修改 Item 前先验证全部 Update，避免因非法输入形成部分更新状态。
    bool applyPartUpdates(const std::vector<RenderPartUpdate>& updates);

    /// Material

    /// 返回当前 Item 统一使用的 Material。
    /// RenderItem 不拥有该对象。
    const Material* material() const;

    /// 设置当前 Item 统一使用的 Material。
    /// 传入对象由外部 MaterialManager 等模块管理生命周期。
    void setMaterial(const Material* material);

    /// Transform

    /// 返回 Item Transform。
    /// 当前所有 RenderPart 共用同一个 Item Local -> World Transform。
    Transform& transform();
    const Transform& transform() const;

    /// Bounds

    /// 当前 Item 是否至少存在一个具有有效 LocalBounds 的 RenderPart。
    bool hasLocalBounds() const;

    /// 返回全部 RenderPart LocalBounds 在 Item Local Space 中的聚合 AABB。
    /// 该 Bounds 用于描述整个 Item 的局部空间范围，不代表单个 Part 的交互范围。
    const AxisAlignedBoundingBox& localBounds() const;

    /// 将聚合 LocalBounds 经过当前 Item Transform 后转换为 World Space AABB。
    AxisAlignedBoundingBox worldBounds() const;

    /// Item Interaction

    /// 使用 World Space Ray 对当前 Item 的各 RenderPart LocalBounds 执行命中测试。
    /// Ray 会先通过 Item Transform 转换到 Item Local Space；
    /// 最终返回最近命中的 RenderPart，以及对应的世界空间距离和命中位置。
    /// 当前只进行 Part Bounds 级命中，不执行 Triangle / Line / Vertex 精确 Picking。
    bool raycast(const QVector3D& rayOrigin, const QVector3D& rayDirection, RenderItemRayHit& hit) const;

    /// Display

    /// 当前 Item 是否参与正常 Viewer 绘制。
    bool isVisible() const;
    void setVisible(bool visible);

    /// 返回或设置整个 Item 的显示模式。
    bool setDisplayMode(DisplayMode mode);

    /// Wireframe / ShadedWithEdges 模式下使用的统一边线颜色。
    const QVector4D& edgeColor() const;
    void setEdgeColor(const QVector4D& color);

    /// 当前 Item 绘制时是否启用深度测试。
    bool depthTestEnabled() const;
    void setDepthTestEnabled(bool enabled);

private:
    friend class ItemManager;

    /// ItemManager 内部接口
    explicit RenderItem(const QString& name);
    ~RenderItem();

    void setId(RenderItemId id) { m_id = id; }

    /// PartManager 内部接口
    RenderPartId allocatePartId();

    /// 根据全部具有有效 LocalBounds 的 RenderPart 重建 Item LocalBounds 聚合缓存。
    void rebuildLocalBoundsCache() const;

private:
    RenderItemId m_id;                              // Item 唯一 ID。
    QString m_name;                                 // Item 调试名称。

    std::vector<RenderPart*> m_parts;                // Item 拥有的 RenderPart，保持创建顺序。
    std::map<RenderPartId, RenderPart*> m_partsById; // RenderPartId 到 RenderPart 的快速查询。
    RenderPartId m_nextPartId;                      // 下一个可分配 RenderPartId。

    const Material* m_material;                      // Item 级借用 Material，不拥有。
    Transform m_transform;                           // Item Local -> World Transform。

    mutable AxisAlignedBoundingBox m_localBoundsCache; // 全部 Part LocalBounds 的聚合缓存。

    bool m_visible;                                  // 是否参与正常 Viewer 绘制。
    DisplayMode m_type;                              // Item 整体显示模式。
    QVector4D m_edgeColor;                           // Wireframe / Edge Overlay 颜色。
    bool m_depthTestEnabled;                         // 是否启用 Depth Test。
};

#endif // RENDERITEM_H