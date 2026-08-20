#ifndef COORDINATESYSTEM_H
#define COORDINATESYSTEM_H

#include "MyOpenGL/Render/RenderContext.h"
#include "MyOpenGL/Render/RenderState.h"
#include "MyOpenGL/Viewer/Geometry/CoordinateSystemGeometry.h"

#include <QVector3D>

/// 世界坐标系系统显示对象。
///
/// CoordinateSystem 自己拥有坐标轴 Geometry。
/// 坐标系原点位于世界空间，但轴的视觉长度保持固定 Pixel 大小，
/// 因此不会随着 Camera Zoom 而在屏幕上变大或变小。
///
/// 本类负责：
/// 1. 持有坐标系 Geometry；
/// 2. 保存世界坐标原点；
/// 3. 保存固定屏幕 Pixel 长度规则；
/// 4. 根据当前 RenderContext 生成实际 RenderState。
///
/// 本类不保存 Material，具体渲染 Material 由 Viewer 的系统渲染层统一提供。
class CoordinateSystem
{
public:
    CoordinateSystem();

    /// Geometry
    CoordinateSystemGeometry& geometry();
    const CoordinateSystemGeometry& geometry() const;

    /// 显示状态
    bool isVisible() const;
    void setVisible(bool visible);

    /// 世界空间位置
    const QVector3D& worldOrigin() const;
    void setWorldOrigin(const QVector3D& origin);

    /// 固定屏幕尺寸
    float pixelLength() const;
    bool setPixelLength(float pixelLength);

    /// Render
    /// 根据当前 Camera / Projection / Viewport，
    /// 将固定 Pixel 长度解析为实际世界空间 Scale。
    bool buildRenderState(const RenderContext& context, RenderState& state) const;

private:
    CoordinateSystemGeometry m_geometry; // 坐标系自身拥有的 RGB XYZ Geometry。

    QVector3D m_worldOrigin; // 坐标系在世界空间中的原点。
    float m_pixelLength;     // 单根坐标轴在屏幕上的目标 Pixel 长度。
    bool m_visible;          // 是否显示世界坐标系。
};

#endif // COORDINATESYSTEM_H