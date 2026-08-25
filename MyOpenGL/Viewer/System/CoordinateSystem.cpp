#include "CoordinateSystem.h"

#include <QDebug>
#include <QVector4D>

#include <cmath>

CoordinateSystem::CoordinateSystem()
    : m_geometry("WorldCoordinateSystem")
    , m_worldOrigin(0.0f, 0.0f, 0.0f)
    , m_pixelLength(90.0f)
    , m_visible(true)
{
    // Geometry 使用单位长度。
    // 最终世界空间长度由 buildRenderState() 根据 Pixel 规则动态计算。
    m_geometry.setAxisLength(1.0f);
}

/// Geometry

CoordinateSystemGeometry& CoordinateSystem::geometry()
{
    return m_geometry;
}

const CoordinateSystemGeometry& CoordinateSystem::geometry() const
{
    return m_geometry;
}

/// 显示状态

bool CoordinateSystem::isVisible() const
{
    return m_visible;
}

void CoordinateSystem::setVisible(bool visible)
{
    m_visible = visible;
}

/// 世界空间位置

const QVector3D& CoordinateSystem::worldOrigin() const
{
    return m_worldOrigin;
}

void CoordinateSystem::setWorldOrigin(const QVector3D& origin)
{
    m_worldOrigin = origin;
}

/// 固定屏幕尺寸

float CoordinateSystem::pixelLength() const
{
    return m_pixelLength;
}

bool CoordinateSystem::setPixelLength(float pixelLength)
{
    if (pixelLength <= 0.0f)
    {
        qWarning() << "CoordinateSystem setPixelLength failed: pixelLength must be greater than zero.";
        return false;
    }

    m_pixelLength = pixelLength;
    return true;
}

/// Render

bool CoordinateSystem::buildRenderState(const RenderContext& context, RenderState& state) const
{
    if (!m_visible || !context.isValid())
        return false;

    /// 世界原点 -> Screen

    const QVector4D clip = context.projection * context.view * QVector4D(m_worldOrigin, 1.0f);

    // 原点位于 Camera 后方时不显示。
    if (clip.w() <= 1.0e-8f)
        return false;

    const float ndcX = clip.x() / clip.w();
    const float ndcY = clip.y() / clip.w();

    // 原点已经完全离开屏幕时不显示。
    if (ndcX < -1.0f || ndcX > 1.0f || ndcY < -1.0f || ndcY > 1.0f)
        return false;

    const float pixelX = (ndcX * 0.5f + 0.5f) * context.viewportWidth;
    const float pixelY = (ndcY * 0.5f + 0.5f) * context.viewportHeight;

    /// Local Viewport

    const int halfSize = static_cast<int>(std::ceil(m_pixelLength * 1.1f));
    const int viewportSize = halfSize * 2;

    state = RenderState();

    state.viewport = RenderViewport(static_cast<int>(pixelX) - halfSize,
                                    static_cast<int>(pixelY) - halfSize,
                                    viewportSize,
                                    viewportSize);

    /// Model

    state.model.setToIdentity();

    /// View
    ///
    /// 保留主 Camera 的旋转，移除平移。
    /// 因此 XYZ 方向与世界坐标系保持一致，但 Geometry 不再位于真实世界深度。

    state.view = context.view;
    state.view.setColumn(3, QVector4D(0.0f, 0.0f, 0.0f, 1.0f));

    /// Projection
    ///
    /// Geometry 的单根轴长度为 1。
    /// viewport 半径约等于 pixelLength，所以轴视觉尺寸固定。

    state.projection.setToIdentity();
    state.projection.ortho(-1.1f, 1.1f, -1.1f, 1.1f, -10.0f, 10.0f);

    state.depthTestEnabled = true;
    state.depthWriteEnabled = true;

    return state.viewport.isValid();
}