#ifndef MESHRESOURCEPRIMITIVEPICKSOURCE_H
#define MESHRESOURCEPRIMITIVEPICKSOURCE_H

#include "Scene/PrimitivePicking.h"

class MeshResource;

/// MyOpenGL Owned MeshResource 的精确 Triangle Picking Adapter。
/// 只借用 MeshResource 当前 CPU Vertex / Index 数据，不复制 Geometry，也不访问 GPU Buffer。
class MeshResourcePrimitivePickSource : public PrimitivePickSource
{
public:
    explicit MeshResourcePrimitivePickSource(const MeshResource* mesh);

    /// 数据源
    const MeshResource* mesh() const;
    void setMesh(const MeshResource* mesh); // 绑定借用的 MeshResource；传入 0 表示清除引用。

    /// Primitive Picking
    bool raycastPrimitive(const QVector3D& rayOrigin, const QVector3D& rayDirection, PrimitivePickHit& hit) const override;

private:
    const MeshResource* m_mesh; // 当前借用的 Owned MeshResource，不拥有该对象。
};

#endif // MESHRESOURCEPRIMITIVEPICKSOURCE_H
