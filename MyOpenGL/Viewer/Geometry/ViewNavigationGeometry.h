#ifndef VIEWNAVIGATIONGEOMETRY_H
#define VIEWNAVIGATIONGEOMETRY_H

#include "MyOpenGL/Resource/BufferGeometry.h"

/// 视图方向导航辅助几何。
/// 只保存静态 XYZ 导航几何，不保存 Camera、Viewport 或交互状态。
class ViewNavigationGeometry : public BufferGeometry
{
public:
    explicit ViewNavigationGeometry(const QString& name = "ViewNavigation");

    /// 几何参数
    float axisLength() const;
    bool setAxisLength(float length); // 修改导航器局部轴长度，并重新生成完整 CPU 几何数据。

private:
    void rebuildGeometry(); // 根据当前轴长度重新生成 position + color 顶点数据和索引数据。

private:
    float m_axisLength; // 导航坐标轴在自身局部坐标系中的长度。
};

#endif // VIEWNAVIGATIONGEOMETRY_H
