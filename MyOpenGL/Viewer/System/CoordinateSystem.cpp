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

    /// 世界原点 -> NDC

    const QVector4D cameraPoint = context.view * QVector4D(m_worldOrigin, 1.0f);
    const QVector4D clip = context.projection * cameraPoint;

    // Perspective 下 Camera 后方 Point 的 Clip W 为负。
    // Parallel 下 W 固定为 1，因此同样可以通过这里。
    if (clip.w() <= 1.0e-8f)
        return false;

    const float ndcX = clip.x() / clip.w();
    const float ndcY = clip.y() / clip.w();
    const float ndcZ = clip.z() / clip.w();

    // 世界坐标系原点不在当前可见裁剪空间时不绘制。
    if (ndcX < -1.0f || ndcX > 1.0f ||
        ndcY < -1.0f || ndcY > 1.0f ||
        ndcZ < -1.0f || ndcZ > 1.0f)
    {
        return false;
    }

    /// 固定 Pixel -> 世界空间 Scale

    bool invertible = false;
    const QMatrix4x4 inverseProjection = context.projection.inverted(&invertible);

    if (!invertible)
        return false;

    // 一个屏幕 Pixel 在 NDC Y 方向对应 2 / ViewportHeight。
    // 在坐标系原点当前深度上分别反投影两个相邻位置，
    // 即可同时兼容 Perspective 和 Parallel Projection。
    const float pixelNdcY = 2.0f / static_cast<float>(context.viewportHeight);

    QVector4D centerCamera = inverseProjection * QVector4D(ndcX, ndcY, ndcZ, 1.0f);
    QVector4D onePixelCamera = inverseProjection * QVector4D(ndcX, ndcY + pixelNdcY, ndcZ, 1.0f);

    if (std::fabs(centerCamera.w()) <= 1.0e-8f ||
        std::fabs(onePixelCamera.w()) <= 1.0e-8f)
    {
        return false;
    }

    centerCamera /= centerCamera.w();
    onePixelCamera /= onePixelCamera.w();

    const float worldUnitsPerPixel =
        (onePixelCamera.toVector3D() - centerCamera.toVector3D()).length();

    if (worldUnitsPerPixel <= 1.0e-8f)
        return false;

    const float worldAxisLength = worldUnitsPerPixel * m_pixelLength;

    /// RenderState

    state = RenderState();

    // Geometry 本身使用单位轴长。
    // Model 将它放到真实世界原点，并按当前深度缩放到固定 Pixel 长度。
    state.model.setToIdentity();
    state.model.translate(m_worldOrigin);
    state.model.scale(worldAxisLength);

    // CoordinateSystem 是真实世界空间系统对象，
    // 使用主 Camera 的 View / Projection，而不是建立独立 Overlay Camera。
    state.view = context.view;
    state.projection = context.projection;

    // 使用主 Viewer Viewport，使坐标轴正确参与场景深度关系。
    state.viewport = RenderViewport(
        0,
        0,
        context.viewportWidth,
        context.viewportHeight);

    state.depthTestEnabled = true;
    state.depthWriteEnabled = true;

    return true;
}
