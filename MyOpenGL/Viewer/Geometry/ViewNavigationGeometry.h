#ifndef VIEWNAVIGATIONGEOMETRY_H
#define VIEWNAVIGATIONGEOMETRY_H

#include "MyOpenGL/Resource/BufferGeometry.h"

/// 视图导航器六面立方体几何。
/// 只描述导航器中央立方体的六个可点击面，不保存 Camera、Viewport、
/// Material、屏幕位置以及任何交互状态。
///
/// 面方向约定：
/// +X = Right
/// -X = Left
/// +Y = Top
/// -Y = Bottom
/// +Z = Front
/// -Z = Back
class ViewNavigationGeometry : public BufferGeometry
{
public:
    explicit ViewNavigationGeometry(const QString& name = "ViewNavigationFaces");

    /// 几何参数
    float halfSize() const; // 导航立方体从中心到任意面的局部距离。

private:
    void rebuildGeometry();

private:
    float m_halfSize; // 固定导航立方体半尺寸。
};

#endif // VIEWNAVIGATIONGEOMETRY_H