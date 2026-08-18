#ifndef SCENE_H
#define SCENE_H

#include "AxisAlignedBoundingBox.h"
#include "PrimitivePicking.h"
#include "RenderPart.h"

#include <QMatrix4x4>
#include <QPointF>
#include <QString>

#include <vector>

class RenderItem;

typedef std::vector<RenderItem*> RenderItemCandidates; // 一次 Scene 查询允许参与的明确 RenderItem 集合。

/// Scene 对象级 Raycast 命中结果。
struct SceneRayHit
{
    SceneRayHit();

    RenderItem* item;   // 最近命中的 Scene Item；没有命中时为 0。
    float distance;     // 从 Ray Origin 到最近 AABB 交点的世界空间距离。
    QVector3D position; // 最近命中点的世界坐标。
};

/// Scene Primitive Picking 的显式查询参数。
/// Scene 不查询 Camera / Viewer 状态，由调用者提供当前 World Ray、ViewProjection、Viewport 和 Pixel Tolerance。
struct ScenePrimitivePickQuery
{
    ScenePrimitivePickQuery();

    QVector3D rayOrigin;      // World Ray Origin。
    QVector3D rayDirection;   // World Ray Direction。
    QPointF screenPosition;   // 当前鼠标位置，原点位于 Viewport 左上角。
    QMatrix4x4 viewProjection;// 当前 Projection * View Matrix。
    int viewportWidth;        // 当前 Viewport Pixel 宽度。
    int viewportHeight;       // 当前 Viewport Pixel 高度。
    float pixelTolerance;     // Line 等屏幕空间 Primitive 的拾取容差，单位 Pixel。
    bool filterPrimitiveType; // true 时 pickPrimitive() 只接受 requiredPrimitiveType；Point Picking 不使用该字段。
    PrimitivePickType requiredPrimitiveType; // 可选 Primitive 类型过滤条件。
};

/// Scene Geometry Vertex / Endpoint Snap 命中结果。
/// PartId 定位 Item 内 RenderPart；VertexIndex 只属于该 Part 的 Render Geometry，外部 Modeling VertexId 仍由 Adapter 映射。
struct ScenePointHit
{
    ScenePointHit();

    RenderItem* item;       // 命中的 Scene Item；没有命中时为 0。
    RenderPartId partId;     // 命中的 Item 内稳定 RenderPartId；只有 item != 0 时有效。
    int vertexIndex;        // 当前 RenderPart Geometry 的 Vertex 序号。
    float screenDistance;   // 鼠标到 Vertex 屏幕投影距离，单位 Pixel。
    float distance;         // 从 World Ray Origin 到 Vertex 的世界空间前向距离。
    QVector3D position;     // 命中 Vertex 的世界坐标。
};

/// Scene 精确 Render Primitive Picking 命中结果。
/// PartId 定位 Item 内 RenderPart；PrimitiveIndex 只属于该 Part 的 Render Geometry，Modeling FaceId / EdgeId 仍由 Adapter 映射。
struct ScenePrimitiveHit
{
    ScenePrimitiveHit();

    RenderItem* item;          // 最近命中的 Scene Item；没有命中时为 0。
    RenderPartId partId;        // 命中的 Item 内稳定 RenderPartId；只有 item != 0 时有效。
    PrimitivePickType type;    // 当前命中的 Render Primitive 类型。
    int primitiveIndex;        // 当前 RenderPart Geometry 内命中的 Primitive 序号。
    int vertexCount;           // 当前 Primitive 顶点数量；Triangle=3，Line=2。
    float distance;            // 从 World Ray Origin 到命中位置的世界空间前向距离。
    QVector3D position;        // 命中位置的世界坐标。
    QVector3D barycentric;     // Triangle 重心坐标；Line 命中时保持为零。
    QVector3D vertices[3];     // 命中 Primitive 的世界坐标顶点；只读取前 vertexCount 个元素。
};

/// 用户对象的扁平 Scene 容器。
/// Scene 只拥有用户可操作 RenderItem；Geometry、Material、Camera、Light 和 GPU Resource 仍由现有 Manager 管理。
/// Viewer 内部 Grid、Axis、Camera Target、Highlight、ViewNavigation 等辅助模型不进入 Scene。
class Scene
{
public:
    Scene();
    ~Scene();

    /// Item 所有权
    RenderItem* createItem(const QString& name); // 创建并接管一个 RenderItem，绘制顺序与创建顺序一致。
    bool removeItem(RenderItem* item);           // 删除指定 Item，不删除其引用的 Geometry / Material。
    void clear();                                // 删除全部 RenderItem。

    /// Item 查询
    int itemCount() const;
    RenderItem* item(int index);
    const RenderItem* item(int index) const;

    /// Scene Bounds
    bool worldBounds(AxisAlignedBoundingBox& bounds, bool visibleOnly = true) const; // 聚合具有 Local Bounds 的 Item；默认忽略隐藏 Item。

    /// Picking
    bool raycast(const RenderItemCandidates& candidates, const QVector3D& rayOrigin, const QVector3D& rayDirection, SceneRayHit& hit, bool visibleOnly = true) const; // 仅对明确 Candidate 中具有 Local Bounds 的 Item 执行 AABB Raycast。
    bool pickPoint(const RenderItemCandidates& candidates, const ScenePrimitivePickQuery& query, ScenePointHit& hit, bool visibleOnly = true) const; // 遍历明确 Candidate 的全部 RenderPart，对各 Part Geometry Vertex 执行屏幕空间 Point Snap。
    bool pickPrimitive(const RenderItemCandidates& candidates, const ScenePrimitivePickQuery& query, ScenePrimitiveHit& hit, bool visibleOnly = true) const; // 遍历明确 Candidate 的全部 RenderPart，对绑定 Picker 的 Part 执行精确 Render Primitive Picking。

private:
    std::vector<RenderItem*> m_items; // Scene 拥有的用户 RenderItem 列表，同时定义这些用户对象的基础绘制顺序。
};

#endif // SCENE_H
