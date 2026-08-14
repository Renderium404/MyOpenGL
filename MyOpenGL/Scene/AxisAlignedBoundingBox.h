#ifndef AXISALIGNEDBOUNDINGBOX_H
#define AXISALIGNEDBOUNDINGBOX_H

#include <QMatrix4x4>
#include <QVector3D>

/// 轴对齐包围盒。
/// 保存世界或局部坐标中的 Min / Max，并使用显式 Valid 状态表示当前是否包含有效几何范围。
class AxisAlignedBoundingBox
{
public:
    AxisAlignedBoundingBox();
    AxisAlignedBoundingBox(const QVector3D& minimum, const QVector3D& maximum);

    /// Bounds 状态
    bool isValid() const;
    const QVector3D& minimum() const;
    const QVector3D& maximum() const;
    QVector3D center() const;
    QVector3D size() const;
    void reset();
    bool set(const QVector3D& minimum, const QVector3D& maximum); // 设置完整 Min / Max；任一轴 Min > Max 时拒绝修改。

    /// Bounds 扩展
    void expandToInclude(const QVector3D& point);
    void expandToInclude(const AxisAlignedBoundingBox& bounds);

    /// 空间查询
    bool intersectRay(const QVector3D& rayOrigin, const QVector3D& rayDirection, float& hitDistance) const; // 使用 Slab Test 计算世界或局部 Ray 与当前 AABB 的最近前向交点。

    /// Transform
    AxisAlignedBoundingBox transformed(const QMatrix4x4& matrix) const; // 将 8 个 Corner 变换后重新计算轴对齐包围盒。

private:
    QVector3D m_minimum; // 当前三个轴的最小坐标。
    QVector3D m_maximum; // 当前三个轴的最大坐标。
    bool m_valid;        // 当前 Bounds 是否已经包含至少一个有效点。
};

#endif // AXISALIGNEDBOUNDINGBOX_H
