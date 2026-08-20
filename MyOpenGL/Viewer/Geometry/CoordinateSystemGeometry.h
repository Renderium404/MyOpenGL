#ifndef COORDINATESYSTEMGEOMETRY_H
#define COORDINATESYSTEMGEOMETRY_H

#include "MyOpenGL/Resource/BufferGeometry.h"

/// 坐标系辅助显示几何。
/// 原点使用小球，XYZ 坐标轴使用圆柱和圆锥箭头。
class CoordinateSystemGeometry : public BufferGeometry
{
public:
    explicit CoordinateSystemGeometry(const QString& name = "CoordinateSystem");

    /// 几何参数
    float axisLength() const;
    bool setAxisLength(float length);

private:
    void rebuildGeometry();

private:
    float m_axisLength; // 坐标原点到箭头尖端的总长度。
};

#endif // COORDINATESYSTEMGEOMETRY_H