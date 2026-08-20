#include "ViewNavigation.h"

#include <QDebug>
#include <QMatrix4x4>
#include <QVector4D>

#include <cmath>

ViewNavigation::ViewNavigation()
    : m_faceGeometry("ViewNavigationFaces")
    , m_axisGeometry("ViewNavigationAxes")
    , m_visible(true)
    , m_pixelSize(128)
    , m_margin(12)
{
    // 导航器 XYZ 轴从中心向外延伸，
    // 长度大于中央导航立方体，使三个方向清晰可见。
    m_axisGeometry.setAxisLength(0.85f);
}

/// Geometry

ViewNavigationGeometry& ViewNavigation::faceGeometry()
{
    return m_faceGeometry;
}

const ViewNavigationGeometry& ViewNavigation::faceGeometry() const
{
    return m_faceGeometry;
}

CoordinateSystemGeometry& ViewNavigation::axisGeometry()
{
    return m_axisGeometry;
}

const CoordinateSystemGeometry& ViewNavigation::axisGeometry() const
{
    return m_axisGeometry;
}

/// 显示状态

bool ViewNavigation::isVisible() const
{
    return m_visible;
}

void ViewNavigation::setVisible(bool visible)
{
    m_visible = visible;
}

/// 固定屏幕尺寸

int ViewNavigation::pixelSize() const
{
    return m_pixelSize;
}

bool ViewNavigation::setPixelSize(int pixelSize)
{
    if (pixelSize <= 0)
    {
        qWarning() << "ViewNavigation setPixelSize failed: pixelSize must be greater than zero.";
        return false;
    }

    m_pixelSize = pixelSize;
    return true;
}

/// Margin

int ViewNavigation::margin() const
{
    return m_margin;
}

bool ViewNavigation::setMargin(int margin)
{
    if (margin < 0)
    {
        qWarning() << "ViewNavigation setMargin failed: margin cannot be negative.";
        return false;
    }

    m_margin = margin;
    return true;
}

/// Render

RenderViewport ViewNavigation::viewport(
    const RenderContext& context) const
{
    return RenderViewport(
        context.viewportWidth - m_margin - m_pixelSize,
        context.viewportHeight - m_margin - m_pixelSize,
        m_pixelSize,
        m_pixelSize);
}

bool ViewNavigation::buildRenderState(
    const RenderContext& context,
    RenderState& state) const
{
    if (!m_visible)
        return false;

    if (!context.isValid())
        return false;

    const RenderViewport navigationViewport =
        viewport(context);

    if (!navigationViewport.isValid())
        return false;

    // Viewer 如果比导航器自身还小，
    // 计算出的 Viewport 可能已经跑到主 Viewport 外面。
    if (navigationViewport.x < 0 ||
        navigationViewport.y < 0)
    {
        return false;
    }

    QVector3D forward = context.cameraForward;
    QVector3D up = context.cameraUp;

    if (forward.lengthSquared() <= 1.0e-12f ||
        up.lengthSquared() <= 1.0e-12f)
    {
        return false;
    }

    forward.normalize();
    up.normalize();

    // Camera Forward 与 Up 正常情况下已经正交，
    // 这里重新正交化一次，避免累计浮点误差影响 lookAt。
    QVector3D right =
        QVector3D::crossProduct(forward, up);

    if (right.lengthSquared() <= 1.0e-12f)
        return false;

    right.normalize();

    up =
        QVector3D::crossProduct(right, forward)
        .normalized();

    state = RenderState();

    /// Model
    ///
    /// 导航器的 Geometry 永远保持标准世界 XYZ 坐标方向。
    /// 不通过 Model Matrix 跟随 Camera。
    state.model.setToIdentity();

    /// View
    ///
    /// 导航器使用自己的虚拟 Camera。
    ///
    /// 虚拟 Camera 与主 Camera 使用相同 Forward / Up，
    /// 但始终在距离导航器原点固定的位置观察局部原点。
    ///
    /// 因此主 Camera Orbit 时：
    /// 世界 XYZ 在导航器中的显示方向会同步变化。
    state.view.setToIdentity();

    state.view.lookAt(
        -forward * 3.0f,
        QVector3D(0.0f, 0.0f, 0.0f),
        up);

    /// Projection
    ///
    /// 导航器使用固定正交投影，
    /// 不使用主 Camera 的 Perspective / Parallel Projection。
    ///
    /// 因此：
    /// - 主 Camera Zoom 不影响导航器尺寸；
    /// - Perspective / Parallel 切换不影响导航器尺寸。
    state.projection.setToIdentity();

    state.projection.ortho(
        -1.0f,
         1.0f,
        -1.0f,
         1.0f,
         0.1f,
        10.0f);

    /// Viewport
    ///
    /// 导航器固定在 Viewer 右上角，
    /// 并拥有独立 Pixel Viewport。
    state.viewport = navigationViewport;

    /// Depth
    ///
    /// 导航器虽然作为 Overlay 绘制，
    /// 但是 Face 和 Axis 之间仍然存在真实三维遮挡关系，
    /// 因此导航器内部仍然开启 Depth Test。
    state.depthTestEnabled = true;
    state.depthWriteEnabled = true;

    return true;
}

/// Interaction

bool ViewNavigation::hitTest(
    const QPoint& mousePosition,
    const RenderContext& context,
    ViewNavigationFace& face) const
{
    face = ViewNavigationFaceNone;

    if (!m_visible)
        return false;

    RenderState state;

    if (!buildRenderState(context, state))
        return false;

    const RenderViewport& vp = state.viewport;

    // Qt Widget:
    //
    // (0,0)
    // ┌──────────────→ X
    // │
    // │
    // ↓ Y
    //
    // OpenGL Viewport:
    //
    // ↑ Y
    // │
    // │
    // └──────────────→ X
    // (0,0)
    //
    // 因此首先转换 Y。
    const int glMouseX =
        mousePosition.x();

    const int glMouseY =
        context.viewportHeight - 1 - mousePosition.y();

    // 鼠标不在导航器独立 Viewport 内。
    if (glMouseX < vp.x ||
        glMouseX >= vp.x + vp.width ||
        glMouseY < vp.y ||
        glMouseY >= vp.y + vp.height)
    {
        return false;
    }

    // 转换为导航器 Viewport 内局部 Pixel。
    const float localPixelX =
        static_cast<float>(glMouseX - vp.x) + 0.5f;

    const float localPixelY =
        static_cast<float>(glMouseY - vp.y) + 0.5f;

    // Pixel -> NDC [-1, +1]。
    const float ndcX =
        localPixelX /
        static_cast<float>(vp.width) *
        2.0f - 1.0f;

    const float ndcY =
        localPixelY /
        static_cast<float>(vp.height) *
        2.0f - 1.0f;

    /// NDC -> Navigation Local

    bool invertible = false;

    const QMatrix4x4 inverseMvp =
        (state.projection *
         state.view *
         state.model)
        .inverted(&invertible);

    if (!invertible)
        return false;

    // OpenGL NDC Near = -1，Far = +1。
    QVector4D nearPoint =
        inverseMvp *
        QVector4D(
            ndcX,
            ndcY,
            -1.0f,
            1.0f);

    QVector4D farPoint =
        inverseMvp *
        QVector4D(
            ndcX,
            ndcY,
            1.0f,
            1.0f);

    if (std::fabs(nearPoint.w()) <= 1.0e-8f ||
        std::fabs(farPoint.w()) <= 1.0e-8f)
    {
        return false;
    }

    nearPoint /= nearPoint.w();
    farPoint /= farPoint.w();

    const QVector3D rayOrigin =
        nearPoint.toVector3D();

    QVector3D rayDirection =
        farPoint.toVector3D() -
        rayOrigin;

    if (rayDirection.lengthSquared() <= 1.0e-12f)
        return false;

    rayDirection.normalize();

    /// Ray / Navigation Cube
    ///
    /// 当前导航面 Geometry 是以局部原点为中心的立方体：
    ///
    /// [-halfSize, +halfSize]
    ///
    /// 使用 Slab 算法计算 Ray 与立方体的最近交点。
    const float halfSize =
        m_faceGeometry.halfSize();

    float minimumDistance = 0.0f;
    float maximumDistance = 1.0e30f;

    for (int axis = 0; axis < 3; ++axis)
    {
        const float origin =
            rayOrigin[axis];

        const float direction =
            rayDirection[axis];

        // Ray 与当前 Axis Slab 平行。
        if (std::fabs(direction) <= 1.0e-8f)
        {
            if (origin < -halfSize ||
                origin > halfSize)
            {
                return false;
            }

            continue;
        }

        float distance0 =
            (-halfSize - origin) /
            direction;

        float distance1 =
            (halfSize - origin) /
            direction;

        if (distance0 > distance1)
        {
            const float temporary =
                distance0;

            distance0 =
                distance1;

            distance1 =
                temporary;
        }

        if (distance0 > minimumDistance)
            minimumDistance = distance0;

        if (distance1 < maximumDistance)
            maximumDistance = distance1;

        if (minimumDistance > maximumDistance)
            return false;
    }

    // 整个立方体位于 Ray 后方。
    if (maximumDistance < 0.0f)
        return false;

    const float hitDistance =
        minimumDistance >= 0.0f
        ? minimumDistance
        : maximumDistance;

    const QVector3D hitPoint =
        rayOrigin +
        rayDirection *
        hitDistance;

    /// Hit Point -> Face
    ///
    /// 理论上命中点必然存在一个分量满足：
    ///
    /// abs(component) == halfSize
    ///
    /// 这里通过距离比较避免浮点误差造成判断失败。
    const float distanceX =
        std::fabs(
            std::fabs(hitPoint.x()) -
            halfSize);

    const float distanceY =
        std::fabs(
            std::fabs(hitPoint.y()) -
            halfSize);

    const float distanceZ =
        std::fabs(
            std::fabs(hitPoint.z()) -
            halfSize);

    if (distanceX <= distanceY &&
        distanceX <= distanceZ)
    {
        face =
            hitPoint.x() >= 0.0f
            ? ViewNavigationFaceRight
            : ViewNavigationFaceLeft;

        return true;
    }

    if (distanceY <= distanceX &&
        distanceY <= distanceZ)
    {
        face =
            hitPoint.y() >= 0.0f
            ? ViewNavigationFaceTop
            : ViewNavigationFaceBottom;

        return true;
    }

    face =
        hitPoint.z() >= 0.0f
        ? ViewNavigationFaceFront
        : ViewNavigationFaceBack;

    return true;
}

bool ViewNavigation::viewDirection(
    ViewNavigationFace face,
    QVector3D& forward,
    QVector3D& up) const
{
    switch (face)
    {
    case ViewNavigationFaceFront:
        // 从世界 +Z 方向观察原点。
        forward =
            QVector3D(
                0.0f,
                0.0f,
                -1.0f);

        up =
            QVector3D(
                0.0f,
                1.0f,
                0.0f);

        return true;

    case ViewNavigationFaceBack:
        // 从世界 -Z 方向观察原点。
        forward =
            QVector3D(
                0.0f,
                0.0f,
                1.0f);

        up =
            QVector3D(
                0.0f,
                1.0f,
                0.0f);

        return true;

    case ViewNavigationFaceRight:
        // 从世界 +X 方向观察原点。
        forward =
            QVector3D(
                -1.0f,
                0.0f,
                0.0f);

        up =
            QVector3D(
                0.0f,
                1.0f,
                0.0f);

        return true;

    case ViewNavigationFaceLeft:
        // 从世界 -X 方向观察原点。
        forward =
            QVector3D(
                1.0f,
                0.0f,
                0.0f);

        up =
            QVector3D(
                0.0f,
                1.0f,
                0.0f);

        return true;

    case ViewNavigationFaceTop:
        // 从世界 +Y 方向向下观察。
        forward =
            QVector3D(
                0.0f,
                -1.0f,
                0.0f);

        // Top View 中使用 -Z 作为屏幕上方向。
        up =
            QVector3D(
                0.0f,
                0.0f,
                -1.0f);

        return true;

    case ViewNavigationFaceBottom:
        // 从世界 -Y 方向向上观察。
        forward =
            QVector3D(
                0.0f,
                1.0f,
                0.0f);

        // Bottom View 使用 +Z 作为屏幕上方向。
        up =
            QVector3D(
                0.0f,
                0.0f,
                1.0f);

        return true;

    case ViewNavigationFaceNone:
        break;
    }

    return false;
}