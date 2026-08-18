#ifndef COORDINATESYSTEMGEOMETRY_H
#define COORDINATESYSTEMGEOMETRY_H

#include "MyOpenGL/Resource/BufferGeometry.h"

/// 坐标系辅助显示几何。
/// 使用红、绿、蓝三条线表示局部坐标系的 +X、+Y、+Z 方向。
class CoordinateSystemGeometry : public BufferGeometry
{
public:
    explicit CoordinateSystemGeometry(const QString& name = "CoordinateSystem");

    /// 几何参数
    float axisLength() const;
    bool setAxisLength(float length); // 修改三个坐标轴长度，并重新生成完整 CPU 几何数据。

private:
    void rebuildGeometry(); // 根据当前轴长度重新生成 position + color 顶点数据和索引数据。

private:
    float m_axisLength; // 三个坐标轴从局部原点向正方向延伸的长度。
};

#endif // COORDINATESYSTEMGEOMETRY_H
