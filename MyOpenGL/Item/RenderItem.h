#ifndef RENDERITEM_H
#define RENDERITEM_H

#include "RenderLabel.h"
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
class MaterialManager;
class ResourceManager;

/// RenderItem 唯一标识类型，由 ItemManager 统一分配。
typedef unsigned int RenderItemId;

/// 无效 RenderItem ID。
const RenderItemId InvalidRenderItemId = 0;

/// RenderItem 的整体显示模式。
enum class DisplayMode
{
    Shaded,         // 按当前 Material 正常绘制。
    Wireframe,      // Triangle Geometry 以统一边线形式绘制。
    ShadedWithEdges // 绘制表面后叠加边线。
};

/// 一次 RenderItem Raycast 的最近命中结果。
struct RenderItemRayHit
{
    RenderItemRayHit();

    RenderPartId partId; // 命中的 RenderPart ID。
    float distance;      // World Space Ray Distance。
    QVector3D position;  // World Space 命中位置。
};

/// ItemManager 中一个完整的模型对象实例。
/// RenderItem 拥有 RenderPart 和 RenderLabel，但不拥有它们引用的 Geometry、Material 和 Texture。
class RenderItem
{
public:
    /// 基本信息
    RenderItemId id() const{return m_id;}
    const QString& name() const{return m_name;}
    QString type() const;
    DisplayMode displayMode() const{return m_type;}

    /// Part 管理
    /// 创建并接管一个新的 RenderPart。
    RenderPart* createPart();
    /// 删除指定 RenderPart，只删除 Part 本身，不删除其引用的 Geometry。
    bool removePart(RenderPartId id);
    /// 删除全部 RenderPart，不删除其引用的 Geometry。
    void clearParts();
    /// 返回当前 RenderPart 数量。
    int partCount() const;
    /// 判断指定 RenderPartId 是否属于当前 Item。
    bool containsPart(RenderPartId id) const;

    /// 按创建顺序访问 RenderPart，索引非法时返回 0。
    RenderPart* partAt(int index);
    const RenderPart* partAt(int index) const;

    /// 按稳定 RenderPartId 查询 RenderPart，不存在时返回 0。
    RenderPart* part(RenderPartId id);
    const RenderPart* part(RenderPartId id) const;

    /// Label 管理

    /// 创建并接管一个空 RenderLabel。
    RenderLabel* createLabel();
    /// 快速创建文本 Label，并自动创建其 Texture、Geometry 和 Material 资源。
    RenderLabel* createTextLabel(ResourceManager& resourceManager, MaterialManager& materialManager, const QString& text, int textPixelSize = 16);
    /// 删除指定 RenderLabel，只删除 Label 本身，不删除其引用的资源。
    bool removeLabel(RenderLabelId id);
    /// 删除全部 RenderLabel，不删除其引用的资源。
    void clearLabels();
    /// 返回当前 RenderLabel 数量。
    int labelCount() const;
    /// 判断指定 RenderLabelId 是否属于当前 Item。
    bool containsLabel(RenderLabelId id) const;

    /// 按创建顺序访问 RenderLabel，索引非法时返回 0。
    RenderLabel* labelAt(int index);
    const RenderLabel* labelAt(int index) const;

    /// 按稳定 RenderLabelId 查询 RenderLabel，不存在时返回 0。
    RenderLabel* label(RenderLabelId id);
    const RenderLabel* label(RenderLabelId id) const;

    /// Part Update

    /// 应用一个 RenderPart 更新。
    bool applyPartUpdate(const RenderPartUpdate& update);
    /// 批量应用 RenderPart 更新，并在修改前验证全部 Update。
    bool applyPartUpdates(const std::vector<RenderPartUpdate>& updates);

    /// Material
    /// 返回当前 Item 使用的 Material，RenderItem 不拥有该对象。
    const Material* material() const;
    /// 设置当前 Item 使用的 Material，RenderItem 不接管其生命周期。
    void setMaterial(const Material* material);

    /// Transform
    /// 返回 Item Transform。
    Transform& transform();
    const Transform& transform() const;

    /// Bounds

    /// 判断当前 Item 是否具有有效 LocalBounds。
    bool hasLocalBounds() const;
    /// 返回全部 RenderPart LocalBounds 的 Item Local Space 聚合结果。
    const AxisAlignedBoundingBox& localBounds() const;

    /// 返回当前 Item 的 World Space AABB。
    AxisAlignedBoundingBox worldBounds() const;

    /// Interaction

    /// 对当前 Item 执行默认精确射线命中测试。
    bool raycast(const QVector3D& rayOrigin, const QVector3D& rayDirection, RenderItemRayHit& hit) const;
    /// 对 RenderPart LocalBounds 执行射线命中测试。
    bool raycastBox(const QVector3D& rayOrigin, const QVector3D& rayDirection, RenderItemRayHit& hit) const;
    /// 先进行 Bounds 粗筛，再执行 Triangle 精确命中测试。
    bool raycastPoint(const QVector3D& rayOrigin, const QVector3D& rayDirection, RenderItemRayHit& hit) const;

    /// Display
    /// 返回或设置当前 Item 是否参与绘制。
    bool isVisible() const;
    void setVisible(bool visible);

    /// 设置当前 Item 的整体显示模式。
    bool setDisplayMode(DisplayMode mode);

    /// 返回或设置 Wireframe / ShadedWithEdges 使用的边线颜色。
    const QVector4D& edgeColor() const;
    void setEdgeColor(const QVector4D& color);

    /// 返回或设置当前 Item 是否启用深度测试。
    bool depthTestEnabled() const;
    void setDepthTestEnabled(bool enabled);

private:
    friend class ItemManager;

    /// ItemManager 内部接口。
    explicit RenderItem(const QString& name);
    ~RenderItem();

    void setId(RenderItemId id)
    {
        m_id = id;
    }

    /// ID 分配

    RenderPartId allocatePartId();
    RenderLabelId allocateLabelId();

    /// Bounds

    /// 根据全部有效 RenderPart LocalBounds 重建聚合缓存。
    void rebuildLocalBoundsCache() const;

private:
    RenderItemId m_id;                                // Item 唯一 ID。
    QString m_name;                                   // Item 调试名称。

    std::vector<RenderPart*> m_parts;                 // Item 拥有的 RenderPart。
    std::map<RenderPartId, RenderPart*> m_partsById;  // RenderPartId 查询表。
    RenderPartId m_nextPartId;                        // 下一个 RenderPartId。

    std::vector<RenderLabel*> m_labels;               // Item 拥有的 RenderLabel。
    std::map<RenderLabelId, RenderLabel*> m_labelsById; // RenderLabelId 查询表。
    RenderLabelId m_nextLabelId;                      // 下一个 RenderLabelId。

    const Material* m_material;                       // Item Material，不拥有。
    Transform m_transform;                            // Item Local -> World Transform。

    mutable AxisAlignedBoundingBox m_localBoundsCache; // RenderPart LocalBounds 聚合缓存。

    bool m_visible;                                   // 是否参与绘制。
    DisplayMode m_type;                               // Item 整体显示模式。
    QVector4D m_edgeColor;                            // 边线颜色。
    bool m_depthTestEnabled;                          // 是否启用深度测试。
};

#endif // RENDERITEM_H