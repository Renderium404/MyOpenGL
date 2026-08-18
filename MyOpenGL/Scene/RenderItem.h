#ifndef RENDERITEM_H
#define RENDERITEM_H

#include "RenderPart.h"
#include "RenderPartUpdate.h"
#include "Transform.h"

#include <QString>
#include <QVector4D>

#include <map>
#include <vector>

class Geometry;
class Material;
class PrimitivePickSource;

/// RenderItem 显示模式。
/// 只描述用户对象的表面显示方式，不改变 Geometry Topology、Material 或 Picking 数据。
enum RenderItemDisplayMode
{
    RenderItemDisplayShaded,         // 使用当前 Material 正常绘制。
    RenderItemDisplayWireframe,      // Triangle Geometry 只绘制统一颜色线框。
    RenderItemDisplayShadedWithEdges // 先按 Material 绘制表面，再叠加统一颜色三角形边线。
};

/// 获取 RenderItem 显示模式的调试名称。
const char* renderItemDisplayModeName(RenderItemDisplayMode mode);

/// 用户 Scene 中一个可操作的可绘制对象实例。
/// RenderItem 拥有 RenderPart 组织结构，但不拥有 Part 引用的 Geometry / PrimitivePickSource，也不拥有 Material。
/// Transform、Visibility、DisplayMode 和 Material 仍属于整个用户对象；一个 Item 可以包含多个独立替换的 RenderPart。
class RenderItem
{
public:
    explicit RenderItem(const QString& name = "RenderItem");
    ~RenderItem();

    /// 基本信息
    const QString& name() const;

    /// Part 所有权
    RenderPart* createPart(RenderPartId id); // 创建并接管一个空 RenderPart；同一 Item 内 PartId 必须唯一。
    bool removePart(RenderPartId id);        // 删除指定 Part；不删除其借用的 Geometry / PrimitivePickSource。
    void clearParts();                       // 删除全部 RenderPart。
    int partCount() const;
    RenderPart* partAt(int index);           // 按创建顺序返回 Part。
    const RenderPart* partAt(int index) const;
    RenderPart* part(RenderPartId id);       // 按稳定 PartId 查询；不存在时返回 0。
    const RenderPart* part(RenderPartId id) const;

    /// Part Update
    bool applyPartUpdate(const RenderPartUpdate& update); // 应用一个 Geometry Replace / Remove；Replace 会清除该 Part 旧 Picker / Bounds。
    bool applyPartUpdates(const std::vector<RenderPartUpdate>& updates); // 批量应用不同 PartId；先完整校验，失败时不修改 Item。

    /// 旧单 Geometry 兼容接口
    /// 这些接口统一映射到 PartId=DefaultRenderPartId，现有单 Geometry Item 无需修改。
    const Geometry* geometry() const;
    void setGeometry(const Geometry* geometry);
    const PrimitivePickSource* primitivePickSource() const;
    void setPrimitivePickSource(const PrimitivePickSource* source);

    /// Material
    const Material* material() const;
    void setMaterial(const Material* material); // Item 级 Material；当前全部 Part 共用，RenderItem 不拥有该对象。

    /// Transform
    Transform& transform();
    const Transform& transform() const;

    /// Bounds
    bool hasLocalBounds() const;
    const AxisAlignedBoundingBox& localBounds() const; // 聚合全部具有有效 Bounds 的 Part。
    void setLocalBounds(const AxisAlignedBoundingBox& bounds); // 兼容接口：设置 Default Part Bounds。
    void clearLocalBounds();                                  // 兼容接口：清除 Default Part Bounds。
    AxisAlignedBoundingBox worldBounds() const;                // 使用 Item Transform 将聚合 Local Bounds 转换为世界 AABB。

    /// 显示状态
    bool isVisible() const;
    void setVisible(bool visible);
    RenderItemDisplayMode displayMode() const;
    bool setDisplayMode(RenderItemDisplayMode mode); // 设置整个 Item 的显示模式；非法枚举值会拒绝修改。
    const QVector4D& edgeColor() const;
    void setEdgeColor(const QVector4D& color);        // Wireframe / ShadedWithEdges 使用的 Item 级统一 RGBA 边线颜色。
    bool depthTestEnabled() const;
    void setDepthTestEnabled(bool enabled);           // 控制整个 Item 的基础 Depth Test；默认开启。

private:
    RenderPart* ensureDefaultPart();                   // 返回或创建 PartId=0 的旧接口兼容 Part。
    void rebuildLocalBoundsCache() const;              // 根据全部 Part Bounds 重建聚合缓存。

private:
    QString m_name;                                   // 当前用户 Scene Item 调试名称。
    std::vector<RenderPart*> m_parts;                 // RenderItem 拥有的 Part，创建顺序同时定义基础绘制顺序。
    std::map<RenderPartId, RenderPart*> m_partsById;  // 稳定 PartId 到 RenderPart 的快速查询。
    const Material* m_material;                       // 当前 Item 级借用 Material，不拥有该对象。
    Transform m_transform;                            // 当前局部 Model Transform。
    mutable AxisAlignedBoundingBox m_localBoundsCache;// 全部 Part Bounds 的聚合缓存，每次查询时重建以允许 Part 独立修改。
    bool m_visible;                                   // 当前 Item 是否参与 Scene Draw。
    RenderItemDisplayMode m_displayMode;              // 当前用户对象显示模式。
    QVector4D m_edgeColor;                            // Wireframe / Edge Overlay 的统一颜色。
    bool m_depthTestEnabled;                          // 当前 Item 绘制时是否启用 Depth Test。
};

#endif // RENDERITEM_H
