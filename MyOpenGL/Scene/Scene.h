#ifndef SCENE_H
#define SCENE_H

#include "Scene/AxisAlignedBoundingBox.h"
#include "Scene/PrimitivePicking.h"

#include <QString>

#include <vector>

class RenderItem;

/// Scene 对象级 Raycast 命中结果。
struct SceneRayHit
{
    SceneRayHit();

    RenderItem* item;   // 最近命中的 Scene Item；没有命中时为 0。
    float distance;     // 从 Ray Origin 到最近 AABB 交点的世界空间距离。
    QVector3D position; // 最近命中点的世界坐标。
};


/// Scene 精确 Render Primitive Raycast 命中结果。
/// PrimitiveIndex 仍然只属于 Render Geometry；映射 Modeling FaceId 应由外部 Adapter 完成。
struct ScenePrimitiveHit
{
    ScenePrimitiveHit();

    RenderItem* item;         // 最近命中的 Scene Item；没有命中时为 0。
    int primitiveIndex;       // 当前 RenderItem 内命中的 Primitive 序号。
    float distance;           // 从 World Ray Origin 到精确 Triangle 命中点的世界空间距离。
    QVector3D position;       // 精确命中点的世界坐标。
    QVector3D barycentric;    // 命中 Triangle 的重心坐标。
    QVector3D vertices[3];    // 命中 Triangle 三个顶点的世界坐标。
};

/// 扁平 Scene 容器。
/// Scene 只拥有 RenderItem；Mesh、Material、Camera、Light 和 GPU Resource 仍由现有 Manager 管理。
class Scene
{
public:
    Scene();
    ~Scene();

    /// Item 所有权
    RenderItem* createItem(const QString& name); // 创建并接管一个 RenderItem，绘制顺序与创建顺序一致。
    bool removeItem(RenderItem* item);           // 删除指定 Item，不删除其引用的 Mesh / Material。
    void clear();                                // 删除全部 RenderItem。

    /// Item 查询
    int itemCount() const;
    RenderItem* item(int index);
    const RenderItem* item(int index) const;

    /// Scene Bounds
    bool worldBounds(AxisAlignedBoundingBox& bounds, bool visibleOnly = true) const; // 聚合具有 Local Bounds 的 Item；默认忽略隐藏 Item。

    /// Picking / Selection
    bool raycast(const QVector3D& rayOrigin, const QVector3D& rayDirection, SceneRayHit& hit, bool visibleOnly = true, bool skipPrimitivePickable = false); // 对具有 Local Bounds 的 Item 执行 AABB Raycast；可跳过已经支持精确 Primitive Picking 的 Item。
    bool raycastPrimitive(const QVector3D& rayOrigin, const QVector3D& rayDirection, ScenePrimitiveHit& hit, bool visibleOnly = true); // 对绑定 PrimitivePickSource 的 Item 执行精确 Render Primitive Raycast。
    RenderItem* selectedItem();
    const RenderItem* selectedItem() const;
    bool setSelectedItem(RenderItem* item); // 设置当前选中 Item；Item 必须属于本 Scene。
    void clearSelection();

private:
    std::vector<RenderItem*> m_items; // Scene 拥有的扁平 RenderItem 列表，同时定义基础绘制顺序。
    RenderItem* m_selectedItem;        // 当前对象级 Selection；只借用 m_items 中的对象。
};

#endif // SCENE_H
