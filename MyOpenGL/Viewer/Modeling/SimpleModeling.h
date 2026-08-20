#ifndef SIMPLEMODELING_H
#define SIMPLEMODELING_H

#include <QString>

class BufferGeometry;

/// 简单实体建模。
/// 创建的 Geometry 由调用方负责交给 ResourceManager 管理。
namespace SimpleModeling
{
    /// 球
    BufferGeometry* createSphere(const QString& name, float radius, int segments = 48, int rings = 24);

    /// 圆柱
    BufferGeometry* createCylinder(const QString& name, float radius, float height, int segments = 48);

    /// 圆锥
    BufferGeometry* createCone(const QString& name, float radius, float height, int segments = 48);

    /// 圆台
    BufferGeometry* createFrustum(const QString& name, float bottomRadius, float topRadius, float height, int segments = 48);

    /// 长方体
    BufferGeometry* createBox(const QString& name, float width, float height, float depth);

    /// 正多棱柱
    BufferGeometry* createPrism(const QString& name, int sideCount, float radius, float height);

    /// 正多棱锥
    BufferGeometry* createPyramid(const QString& name, int sideCount, float radius, float height);

    /// 正多棱台
    BufferGeometry* createPolyFrustum(const QString& name, int sideCount, float bottomRadius, float topRadius, float height);
}

#endif // SIMPLEMODELING_H