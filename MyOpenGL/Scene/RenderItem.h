#ifndef RENDERITEM_H
#define RENDERITEM_H

#include "Scene/AxisAlignedBoundingBox.h"
#include "Scene/Transform.h"

#include <QString>

class Material;
class PrimitivePickSource;
class RenderableObject;

/// 用户 Scene 中一个可操作的可绘制对象实例。
/// RenderableObject / MeshResource 是系统底层渲染资源；RenderItem 将其封装为用户可进行 Transform、Visible、Bounds、Picking 等操作的 Scene 对象。
/// 不拥有 Mesh、Material 和 PrimitivePickSource，只保存借用引用、局部 Transform、可选 Bounds 和基础显示状态。
class RenderItem
{
public:
    explicit RenderItem(const QString& name = "RenderItem");

    /// 基本信息
    const QString& name() const;

    /// 绘制引用
    const RenderableObject* mesh() const;
    const Material* material() const;
    void setMesh(const RenderableObject* mesh);       // 绑定借用的 RenderableObject；传入 0 表示清除引用。
    void setMaterial(const Material* material);     // 绑定借用的 Material；传入 0 表示清除引用。

    /// Primitive Picking
    const PrimitivePickSource* primitivePickSource() const;
    void setPrimitivePickSource(const PrimitivePickSource* source); // 绑定可选的精确 Primitive Picker；RenderItem 不拥有该对象。

    /// Transform
    Transform& transform();
    const Transform& transform() const;

    /// Bounds
    bool hasLocalBounds() const;
    const AxisAlignedBoundingBox& localBounds() const;
    void setLocalBounds(const AxisAlignedBoundingBox& bounds); // 设置 Mesh 局部坐标 Bounds；无效 Bounds 会清除当前 Bounds。
    void clearLocalBounds();
    AxisAlignedBoundingBox worldBounds() const;                // 使用当前 Transform 将 Local Bounds 转换为世界 AABB。

    /// 显示状态
    bool isVisible() const;
    void setVisible(bool visible);
    bool depthTestEnabled() const;
    void setDepthTestEnabled(bool enabled); // 控制当前 Item 的基础 Depth Test；默认开启。

private:
    QString m_name;                         // 当前用户 Scene Item 调试名称。
    const RenderableObject* m_mesh;           // 当前借用的 Mesh，不拥有该对象。
    const Material* m_material;             // 当前借用的 Material，不拥有该对象。
    const PrimitivePickSource* m_primitivePickSource; // 当前借用的 Primitive Picker；0 表示只支持对象级 Bounds Picking。
    Transform m_transform;                  // 当前局部 Model Transform。
    AxisAlignedBoundingBox m_localBounds;   // 当前 Mesh 局部坐标 Bounds；Valid=false 表示不参与 Scene Bounds。
    bool m_visible;                         // 当前 Item 是否参与 Scene Draw。
    bool m_depthTestEnabled;                // 当前 Item 绘制时是否启用 Depth Test。
};

#endif // RENDERITEM_H