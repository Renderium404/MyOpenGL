#ifndef COORDINATESYSTEMRESOURCE_H
#define COORDINATESYSTEMRESOURCE_H

#include "Resource/MeshResource.h"

/// 坐标系辅助显示资源。
/// 使用红、绿、蓝三条线分别表示局部坐标系的 +X、+Y、+Z 方向。
class CoordinateSystemResource : public MeshResource
{
public:
    explicit CoordinateSystemResource(const QString& name = "CoordinateSystem");

    /// 几何参数
    float axisLength() const;
    bool setAxisLength(float length); // 修改三个坐标轴长度，并重新生成当前资源的完整 CPU 几何数据。

private:
    void rebuildGeometry(); // 根据当前轴长度重新生成 position + color 顶点数据和索引数据。

private:
    float m_axisLength; // 三个坐标轴从局部原点向正方向延伸的长度。
};

#endif // COORDINATESYSTEMRESOURCE_H