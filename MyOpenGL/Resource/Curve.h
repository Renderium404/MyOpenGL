#ifndef CURVE_H
#define CURVE_H

#include "BufferGeometry.h"

#include <QVector3D>

#include <vector>

/// 动态折线资源。
/// 使用控制点顺序生成 GL_LINE_STRIP，支持控制点位置的局部增量更新。
class Curve : public BufferGeometry
{
public:
    explicit Curve(const QString& name = "Curve");

    /// 曲线基本信息
    int pointCount() const;
    const std::vector<QVector3D>& points() const;
    const QVector3D& color() const;

    /// 曲线完整数据
    bool setPoints(const std::vector<QVector3D>& points); // 替换全部控制点；至少需要两个点，并触发全量更新。
    bool setColor(const QVector3D& color);                // 修改整条曲线颜色；重新生成完整 Vertex 数据。

    /// 曲线动态编辑
    bool updatePoint(int pointIndex, const QVector3D& point); // 修改已有控制点，只更新对应 Vertex 的 position.xyz。
    bool appendPoint(const QVector3D& point);                 // 添加控制点导致 Buffer 大小变化，因此触发全量更新。
    bool removePoint(int pointIndex);                         // 删除控制点导致 Buffer 大小变化，因此触发全量更新。

private:
    void rebuildGeometry(); // 根据控制点和颜色重新生成完整 position + color Vertex 数据及连续索引。

private:
    std::vector<QVector3D> m_points; // 当前曲线按绘制顺序排列的控制点。
    QVector3D m_color;               // 当前整条曲线使用的 RGB 颜色，可使用大于 1 的 HDR 分量。
};

#endif // CURVE_H
