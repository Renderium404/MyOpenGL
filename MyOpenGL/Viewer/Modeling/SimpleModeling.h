#ifndef SIMPLEMODELING_H
#define SIMPLEMODELING_H

#include <QString>
#include <QVector3D>
class BufferGeometry;

/// 简单实体建模。
/// 创建的 Geometry 由调用方负责交给 ResourceManager 管理。
namespace SimpleModeling
{
    /// 线段
    /// 创建一条独立线段。
    BufferGeometry* createLine(const QString& name, const QVector3D& start, const QVector3D& end, const QVector3D& color, float lineWidth = 1.0f);
    /// 创建一条连续折线，按 points 顺序使用 GL_LINE_STRIP 连续连接。
    BufferGeometry* createLineStrip(const QString& name, const std::vector<QVector3D>& points, const QVector3D& color, float lineWidth = 1.0f);
    /// 创建多条相互独立的线段，points 每两个点组成一条线。
    BufferGeometry* createLines(const QString& name, const std::vector<QVector3D>& points, const QVector3D& color, float lineWidth = 1.0f);
    /// 创建由两个方向确定的较小夹角圆弧。
    BufferGeometry* createArc(const QString& name, const QVector3D& center, const QVector3D& startDirection, const QVector3D& endDirection, float radius, const QVector3D& color, float lineWidth = 1.0f, int segments = 32); 
    
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