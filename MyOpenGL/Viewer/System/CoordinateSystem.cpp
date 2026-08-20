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

    /// 世界原点 -> 屏幕位置

    const QVector4D clip = context.projection * context.view * QVector4D(m_worldOrigin, 1.0f);

    if (clip.w() <= 1.0e-8f)
        return false;

    const float ndcX = clip.x() / clip.w();
    const float ndcY = clip.y() / clip.w();

    if (ndcX < -1.0f || ndcX > 1.0f || ndcY < -1.0f || ndcY > 1.0f)
        return false;

    const float pixelX = (ndcX * 0.5f + 0.5f) * context.viewportWidth;
    const float pixelY = (ndcY * 0.5f + 0.5f) * context.viewportHeight;

    /// 独立 Viewport

    const int viewportSize = static_cast<int>(m_pixelLength * 2.4f);
    const int halfViewport = viewportSize / 2;

    const RenderViewport viewport(
        static_cast<int>(pixelX) - halfViewport,
        static_cast<int>(pixelY) - halfViewport,
        viewportSize,
        viewportSize);

    if (!viewport.isValid())
        return false;

    /// Camera Orientation

    QVector3D forward = context.cameraForward.normalized();
    QVector3D up = context.cameraUp.normalized();

    QVector3D right = QVector3D::crossProduct(forward, up);

    if (right.lengthSquared() <= 1.0e-12f)
        return false;

    right.normalize();
    up = QVector3D::crossProduct(right, forward).normalized();

    state = RenderState();

    state.model.setToIdentity();

    state.view.setToIdentity();
    state.view.lookAt(-forward * 3.0f, QVector3D(0.0f, 0.0f, 0.0f), up);

    const float halfRange = static_cast<float>(viewportSize) / (2.0f * m_pixelLength);

    state.projection.setToIdentity();
    state.projection.ortho(-halfRange, halfRange, -halfRange, halfRange, 0.1f, 10.0f);

    state.viewport = viewport;
    state.depthTestEnabled = true;
    state.depthWriteEnabled = true;

    return true;
}