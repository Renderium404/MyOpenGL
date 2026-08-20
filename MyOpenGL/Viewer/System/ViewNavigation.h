#ifndef VIEWNAVIGATION_H
#define VIEWNAVIGATION_H

#include "MyOpenGL/Render/RenderContext.h"
#include "MyOpenGL/Render/RenderState.h"
#include "MyOpenGL/Viewer/Geometry/CoordinateSystemGeometry.h"
#include "MyOpenGL/Viewer/Geometry/ViewNavigationGeometry.h"

#include <QPoint>
#include <QVector3D>

/// 导航立方体六个可交互面。
enum ViewNavigationFace
{
    ViewNavigationFaceNone,

    ViewNavigationFaceFront,
    ViewNavigationFaceBack,

    ViewNavigationFaceLeft,
    ViewNavigationFaceRight,

    ViewNavigationFaceTop,
    ViewNavigationFaceBottom
};

/// Viewer 右上角视图方向导航器。
///
/// ViewNavigation 自己完整拥有导航器内部 Geometry：
///
/// ViewNavigation
/// ├── ViewNavigationGeometry
/// │      六个可点击导航面，Triangles
/// │
/// └── CoordinateSystemGeometry
///        RGB XYZ 坐标轴，Lines
///
/// 本类同时负责：
/// 1. 保存导航器固定 Pixel Viewport 规则；
/// 2. 根据当前 Camera Orientation 生成导航器 RenderState；
/// 3. 判断鼠标点击了哪个导航面；
/// 4. 将导航面转换为目标 Camera Forward / Up。
///
/// 本类不保存 Material。
/// 导航器使用的固定 Material 由 Viewer 系统渲染层统一提供。
class ViewNavigation
{
public:
    ViewNavigation();

    /// Geometry
    ViewNavigationGeometry& faceGeometry();
    const ViewNavigationGeometry& faceGeometry() const;

    CoordinateSystemGeometry& axisGeometry();
    const CoordinateSystemGeometry& axisGeometry() const;

    /// 显示状态
    bool isVisible() const;
    void setVisible(bool visible);

    /// 固定屏幕尺寸
    int pixelSize() const;
    bool setPixelSize(int pixelSize);

    /// 与 Viewer 右边和上边的 Pixel 距离
    int margin() const;
    bool setMargin(int margin);

    /// Render

    /// 根据当前 Viewer 尺寸计算导航器独立 OpenGL Viewport。
    RenderViewport viewport(const RenderContext& context) const;

    /// 根据当前主 Camera 朝向生成导航器独立 RenderState。
    bool buildRenderState(const RenderContext& context, RenderState& state) const;

    /// Interaction

    /// 判断 Qt Widget 鼠标坐标是否命中导航立方体，
    /// 命中时返回具体导航面。
    bool hitTest(
        const QPoint& mousePosition,
        const RenderContext& context,
        ViewNavigationFace& face) const;

    /// 将导航面转换为目标 Camera 世界空间 Forward / Up。
    bool viewDirection(
        ViewNavigationFace face,
        QVector3D& forward,
        QVector3D& up) const;

private:
    ViewNavigationGeometry m_faceGeometry;    // 六个可点击导航面。
    CoordinateSystemGeometry m_axisGeometry; // XYZ 导航坐标轴。

    bool m_visible; // 是否显示导航器。
    int m_pixelSize;// 导航器独立 Viewport 的固定 Pixel 尺寸。
    int m_margin;   // 导航器距离 Viewer 右边和上边的 Pixel 距离。
};

#endif // VIEWNAVIGATION_H