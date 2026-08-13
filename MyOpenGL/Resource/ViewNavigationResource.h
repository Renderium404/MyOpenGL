#ifndef VIEWNAVIGATIONRESOURCE_H
#define VIEWNAVIGATIONRESOURCE_H

#include "Resource/MeshResource.h"

/// 视图方向导航辅助资源。
/// 保存静态 XYZ 导航几何；屏幕位置和 Camera 朝向由后续 Renderer 决定。
class ViewNavigationResource : public MeshResource
{
public:
    explicit ViewNavigationResource(const QString& name = "ViewNavigation");

    /// 几何参数
    float axisLength() const;
    bool setAxisLength(float length); // 修改导航器局部轴长度，并重新生成完整 CPU 几何数据。

private:
    void rebuildGeometry(); // 根据当前轴长度重新生成 position + color 顶点数据和索引数据。

private:
    float m_axisLength; // 导航坐标轴在自身局部坐标系中的长度。
};

#endif // VIEWNAVIGATIONRESOURCE_H