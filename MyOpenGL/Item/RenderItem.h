#ifndef RENDERITEM_H
#define RENDERITEM_H

#include "RenderLabel.h"
#include "RenderPart.h"
#include "Transform.h"

#include <QString>
#include <QVector3D>
#include <QVector4D>

#include <map>
#include <vector>

class ItemManager;
class Light;
class Material;
class Renderer;
struct RenderContext;

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

    RenderPartId partId; // 命中的全局 RenderPart ID。
    float distance;      // World Space Ray Distance。
    QVector3D position;  // World Space 命中位置。
};

/// ItemManager 中一个完整的模型对象实例。
/// RenderItem 只组织 RenderPart / RenderLabel，不拥有其生命周期；Part 的创建、删除和 ID 统一由 ItemManager 管理。
class RenderItem
{
public:
    /// 基本信息

    RenderItemId id() const{return m_id;}
    const QString& name() const{return m_name;}
    QString type() const;
    DisplayMode displayMode() const{return m_type;}

    /// Render

    /// 绘制当前 Item 组织的普通 Part。
    bool drawParts(Renderer& renderer, const RenderContext& context, const std::vector<const Light*>& lights) const;
    /// 绘制当前 Item 组织的 Label。
    bool drawLabels(Renderer& renderer, const RenderContext& context, const std::vector<const Light*>& lights) const;

    /// Part 组织

    // 将已有全局 Part 加入当前 Item；不接管生命周期，同一个 Part 可以同时加入多个 Item。
    bool addPart(RenderPart* part);
    // 从当前 Item 移除 Part；不删除 Part。
    bool removePart(RenderPartId id);
    // 清空当前 Item 的全部普通 Part 引用；不删除 Part。
    void clearParts();

    /// Part 查询

    int partCount() const;
    bool containsPart(RenderPartId id) const;
    RenderPart* partAt(int index);
    const RenderPart* partAt(int index) const;
    RenderPart* part(RenderPartId id);
    const RenderPart* part(RenderPartId id) const;

    /// Label 组织

    // 将已有全局 Label 加入当前 Item；不接管生命周期，同一个 Label 可以同时加入多个 Item。
    bool addLabel(RenderLabel* label);
    // 从当前 Item 移除 Label；不删除 Label。
    bool removeLabel(RenderLabelId id);
    // 清空当前 Item 的全部 Label 引用；不删除 Label。
    void clearLabels();

    /// Label 查询

    int labelCount() const;
    bool containsLabel(RenderLabelId id) const;
    RenderLabel* labelAt(int index);
    const RenderLabel* labelAt(int index) const;
    RenderLabel* label(RenderLabelId id);
    const RenderLabel* label(RenderLabelId id) const;

    /// Material

    const Material* material() const;
    void setMaterial(const Material* material);

    /// Transform

    Transform& transform();
    const Transform& transform() const;

    /// Bounds

    bool hasLocalBounds() const;
    const AxisAlignedBoundingBox& localBounds() const;
    AxisAlignedBoundingBox worldBounds() const;

    /// Interaction

    bool raycast(const QVector3D& rayOrigin, const QVector3D& rayDirection, RenderItemRayHit& hit) const;
    bool raycastBox(const QVector3D& rayOrigin, const QVector3D& rayDirection, RenderItemRayHit& hit) const;
    bool raycastPoint(const QVector3D& rayOrigin, const QVector3D& rayDirection, RenderItemRayHit& hit) const;

    /// Display

    bool isVisible() const;
    void setVisible(bool visible);
    bool setDisplayMode(DisplayMode mode);
    const QVector4D& edgeColor() const;
    void setEdgeColor(const QVector4D& color);
    bool depthTestEnabled() const;
    void setDepthTestEnabled(bool enabled);
    bool depthWriteEnabled() const;
    void setDepthWriteEnabled(bool enabled);

private:
    friend class ItemManager;

    /// ItemManager 内部接口

    explicit RenderItem(const QString& name);
    ~RenderItem();
    void setId(RenderItemId id){m_id = id;}

    /// Bounds

    void rebuildLocalBoundsCache() const;

private:
    RenderItemId m_id;                                  // Item 唯一 ID。
    QString m_name;                                     // Item 调试名称。

    std::vector<RenderPart*> m_parts;                   // 当前 Item 组织的普通 Part，非拥有引用。
    std::map<RenderPartId, RenderPart*> m_partsById;    // 当前 Item 内普通 Part 查询表。

    std::vector<RenderLabel*> m_labels;                 // 当前 Item 组织的 Label，非拥有引用。
    std::map<RenderLabelId, RenderLabel*> m_labelsById; // 当前 Item 内 Label 查询表。

    const Material* m_material;                         // Item 默认 Material，不拥有。
    Transform m_transform;                              // Item Local -> World Transform。

    mutable AxisAlignedBoundingBox m_localBoundsCache;  // 标准 RenderPart 的 Item Local Space 聚合 Bounds。

    bool m_visible;                                     // 是否参与绘制。
    DisplayMode m_type;                                 // Item 整体显示模式。
    QVector4D m_edgeColor;                              // 边线颜色。
    bool m_depthTestEnabled;                            // 默认是否启用深度测试。
    bool m_depthWriteEnabled;                           // 默认是否写入深度缓冲。
};

#endif // RENDERITEM_H
