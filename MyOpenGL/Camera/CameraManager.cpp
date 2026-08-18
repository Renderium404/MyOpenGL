#include "CameraManager.h"

#include <QDebug>
#include <QQuaternion>
#include <QtMath>

CameraManager::CameraManager()
    : m_viewBounds(createDefaultViewBounds())
    , m_nextId(1)
    , m_activeCameraId(InvalidCameraId)
{
}

CameraManager::~CameraManager()
{
    clear();
}

/// 相机管理

CameraId CameraManager::add(Camera* camera)
{
    if (camera == 0)
    {
        qWarning() << "CameraManager add failed: camera is null.";
        return InvalidCameraId;
    }

    if (camera->id() != InvalidCameraId)
    {
        qWarning() << "CameraManager add failed: camera already has an id:" << camera->name();
        return InvalidCameraId;
    }

    const CameraId id = m_nextId++;
    camera->setId(id);
    m_cameras[id] = camera;

    // 第一个加入管理器的 Camera 自动成为 Active Camera，并立即以当前 View Bounds Center 同步视图中心。
    if (m_activeCameraId == InvalidCameraId)
    {
        m_activeCameraId = id;

        if (!applyView(m_viewBounds, camera->forward(), camera->up(), camera->distanceToTarget()))
        {
            qWarning() << "CameraManager add: unable to synchronize first Camera with default View Bounds:" << camera->name();
            m_activeCameraId = InvalidCameraId;
        }
    }

    return id;
}

Camera* CameraManager::get(CameraId id)
{
    CameraMap::iterator it = m_cameras.find(id);

    if (it == m_cameras.end())
        return 0;

    return it->second;
}

const Camera* CameraManager::get(CameraId id) const
{
    CameraMap::const_iterator it = m_cameras.find(id);

    if (it == m_cameras.end())
        return 0;

    return it->second;
}

bool CameraManager::contains(CameraId id) const
{
    return m_cameras.find(id) != m_cameras.end();
}

std::size_t CameraManager::count() const
{
    return m_cameras.size();
}

bool CameraManager::remove(CameraId id)
{
    CameraMap::iterator it = m_cameras.find(id);

    if (it == m_cameras.end())
    {
        qWarning() << "CameraManager remove failed: camera does not exist:" << id;
        return false;
    }

    delete it->second;
    m_cameras.erase(it);

    if (m_activeCameraId == id)
        m_activeCameraId = InvalidCameraId;

    return true;
}

void CameraManager::clear()
{
    CameraMap::iterator it = m_cameras.begin();

    while (it != m_cameras.end())
    {
        delete it->second;
        ++it;
    }

    m_cameras.clear();
    m_viewBounds = createDefaultViewBounds();
    m_activeCameraId = InvalidCameraId;
}

/// Active Camera

bool CameraManager::setActiveCamera(CameraId id)
{
    Camera* camera = get(id);

    if (camera == 0)
    {
        qWarning() << "CameraManager setActiveCamera failed: camera does not exist:" << id;
        return false;
    }

    const CameraId previousActiveCameraId = m_activeCameraId;
    m_activeCameraId = id;

    // 切换 Camera 后仍以同一 View Bounds 为视图控制范围；只保留该 Camera 自己的方向和距离。
    if (!applyView(m_viewBounds, camera->forward(), camera->up(), camera->distanceToTarget()))
    {
        m_activeCameraId = previousActiveCameraId;
        return false;
    }

    return true;
}

CameraId CameraManager::activeCameraId() const
{
    return m_activeCameraId;
}

Camera* CameraManager::activeCamera()
{
    return get(m_activeCameraId);
}

const Camera* CameraManager::activeCamera() const
{
    return get(m_activeCameraId);
}

/// View Bounds

bool CameraManager::setViewBounds(const AxisAlignedBoundingBox& bounds)
{
    if (!bounds.isValid())
    {
        qWarning() << "CameraManager setViewBounds failed: bounds are invalid.";
        return false;
    }

    m_viewBounds = bounds;
    return true;
}

void CameraManager::clearViewBounds()
{
    const AxisAlignedBoundingBox defaultBounds = createDefaultViewBounds();
    Camera* camera = activeCamera();

    if (camera != 0)
    {
        // 恢复默认 Bounds 时同步 Camera，使 Target 与 View Bounds Center 始终保持一致。
        if (!applyView(defaultBounds, camera->forward(), camera->up(), camera->distanceToTarget()))
        {
            qWarning() << "CameraManager clearViewBounds failed: unable to apply default View Bounds.";
            return;
        }
    }

    m_viewBounds = defaultBounds;
}

bool CameraManager::hasViewBounds() const
{
    return m_viewBounds.isValid();
}

const AxisAlignedBoundingBox& CameraManager::viewBounds() const
{
    return m_viewBounds;
}

bool CameraManager::focusBounds(const AxisAlignedBoundingBox& bounds)
{
    Camera* camera = activeCamera();

    if (camera == 0)
    {
        qWarning() << "CameraManager focusBounds failed: active camera does not exist.";
        return false;
    }

    if (!bounds.isValid())
    {
        qWarning() << "CameraManager focusBounds failed: bounds are invalid.";
        return false;
    }

    const float distance = camera->distanceToTarget();

    // focusBounds() 的操作对象是完整 Bounds：
    // 替换 View Bounds，并保持当前观察方向 / 距离，把 Camera 平移到新的 Bounds Center。
    if (!applyView(bounds, camera->forward(), camera->up(), distance))
        return false;

    m_viewBounds = bounds;
    return true;
}

bool CameraManager::focusPoint(const QVector3D& worldPoint)
{
    Camera* camera = activeCamera();

    if (camera == 0)
    {
        qWarning() << "CameraManager focusPoint failed: active camera does not exist.";
        return false;
    }

    if (!validateViewBounds("focusPoint"))
        return false;

    // focusPoint() 不直接修改 Camera Target。
    // 它平移整个 View Bounds，使 Bounds Center 移到指定点；Camera 再由新 Bounds Center 派生。
    const QVector3D translation = worldPoint - m_viewBounds.center();
    AxisAlignedBoundingBox translatedBounds;

    if (!translatedViewBounds(translation, translatedBounds))
        return false;

    const float distance = camera->distanceToTarget();

    if (!applyView(translatedBounds, camera->forward(), camera->up(), distance))
        return false;

    m_viewBounds = translatedBounds;
    return true;
}

bool CameraManager::fitViewBounds(int viewportWidth, int viewportHeight, float margin)
{
    if (!validateViewBounds("fitViewBounds"))
        return false;

    return fitBoundsInternal(m_viewBounds, viewportWidth, viewportHeight, margin);
}

bool CameraManager::fitBounds(const AxisAlignedBoundingBox& bounds, int viewportWidth, int viewportHeight, float margin)
{
    if (!bounds.isValid())
    {
        qWarning() << "CameraManager fitBounds failed: bounds are invalid.";
        return false;
    }

    // 先使用明确 Bounds 计算并应用 Camera；成功后再更新缓存，避免失败时留下半更新状态。
    if (!fitBoundsInternal(bounds, viewportWidth, viewportHeight, margin))
        return false;

    m_viewBounds = bounds;
    return true;
}

/// 视图导航

bool CameraManager::setViewDirection(const QVector3D& forward, const QVector3D& up)
{
    Camera* camera = activeCamera();

    if (camera == 0)
    {
        qWarning() << "CameraManager setViewDirection failed: active camera does not exist.";
        return false;
    }

    if (!validateViewBounds("setViewDirection"))
        return false;

    // Direction 只改变 Camera 围绕 View Bounds Center 的观察角度。
    // Target 始终重新取 m_viewBounds.center()，不沿用 Camera 自己缓存的旧 Target。
    return applyView(m_viewBounds, forward, up, camera->distanceToTarget());
}

bool CameraManager::orbit(float yawDegrees, float pitchDegrees)
{
    Camera* camera = activeCamera();

    if (camera == 0)
    {
        qWarning() << "CameraManager orbit failed: active camera does not exist.";
        return false;
    }

    if (!validateViewBounds("orbit"))
        return false;

    const QVector3D center = m_viewBounds.center();
    const QVector3D offset = camera->position() - center;

    const float directionEpsilon = 1.0e-8f;

    if (offset.lengthSquared() <= directionEpsilon)
    {
        qWarning() << "CameraManager orbit failed: current Camera position equals View Bounds Center:" << camera->name();
        return false;
    }

    const QVector3D orbitUp = camera->viewUp();
    const QVector3D orbitRight = camera->right();

    if (orbitUp.lengthSquared() <= directionEpsilon || orbitRight.lengthSquared() <= directionEpsilon)
    {
        qWarning() << "CameraManager orbit failed: current Camera orientation is invalid:" << camera->name();
        return false;
    }

    // 鼠标一次二维拖动直接转换为当前屏幕坐标系中的旋转向量：
    // Horizontal -> 当前 Camera Up，Vertical -> 当前 Camera Right。
    // 旋转轴来自当前完整姿态，不依赖世界固定 Up，也不限制 Pitch 到 +/-89 度。
    const QVector3D rotationVector = orbitUp * yawDegrees + orbitRight * pitchDegrees;
    const float rotationAngle = rotationVector.length();

    if (rotationAngle <= 1.0e-6f)
        return true;

    const QQuaternion incrementalRotation =
        QQuaternion::fromAxisAndAngle(rotationVector / rotationAngle, rotationAngle);

    const QVector3D rotatedOffset = incrementalRotation.rotatedVector(offset);
    const QVector3D rotatedUp = incrementalRotation.rotatedVector(orbitUp).normalized();

    if (rotatedOffset.lengthSquared() <= directionEpsilon || rotatedUp.lengthSquared() <= directionEpsilon)
    {
        qWarning() << "CameraManager orbit failed: quaternion rotation produced invalid Camera state:" << camera->name();
        return false;
    }

    // Position Offset 与 Up 使用同一个增量四元数旋转，因此穿过顶部/底部后仍保持完整相机姿态，
    // 不会出现 Forward 与固定世界 Up 平行导致的极点锁定。
    const QVector3D forward = (-rotatedOffset).normalized();
    return applyView(m_viewBounds, forward, rotatedUp, rotatedOffset.length());
}

bool CameraManager::pan(float rightDistance, float upDistance)
{
    Camera* camera = activeCamera();

    if (camera == 0)
    {
        qWarning() << "CameraManager pan failed: active camera does not exist.";
        return false;
    }

    if (!validateViewBounds("pan"))
        return false;

    const QVector3D translation = camera->right() * rightDistance + camera->viewUp() * upDistance;
    AxisAlignedBoundingBox translatedBounds;

    if (!translatedViewBounds(translation, translatedBounds))
        return false;

    // Pan 的本质是平移整个 View Bounds；Camera 与 Bounds 同步平移。
    if (!applyView(translatedBounds, camera->forward(), camera->up(), camera->distanceToTarget()))
        return false;

    m_viewBounds = translatedBounds;
    return true;
}

bool CameraManager::zoom(float factor)
{
    Camera* camera = activeCamera();

    if (camera == 0)
    {
        qWarning() << "CameraManager zoom failed: active camera does not exist.";
        return false;
    }

    if (!validateViewBounds("zoom"))
        return false;

    if (factor <= 0.0f)
    {
        qWarning() << "CameraManager zoom failed: factor must be greater than zero:" << camera->name();
        return false;
    }

    if (camera->projectionType() == CameraProjectionOrthographic)
    {
        // 正交相机通过改变可视高度实现 Zoom；View Bounds 自身不发生变化。
        const float newHeight = camera->orthographicHeight() / factor;

        // 0.001 世界单位作为最小正交视图高度，避免无限放大导致数值退化。
        if (newHeight < 0.001f)
            return true;

        // 即使正交 Zoom 不需要移动 Camera，也先重新以 View Bounds Center 同步 Target。
        if (!applyView(m_viewBounds, camera->forward(), camera->up(), camera->distanceToTarget()))
            return false;

        return camera->setOrthographic(newHeight, camera->nearPlane(), camera->farPlane());
    }

    // 透视相机围绕 View Bounds Center 改变观察距离。
    const float currentDistance = camera->distanceToTarget();
    float newDistance = currentDistance / factor;

    // Camera 必须与 Near Plane 保持足够距离，否则 View Bounds Center 会进入 Near Plane 附近。
    const float nearPlaneSafetyFactor = 2.5f;
    const float minimumDistance = camera->nearPlane() * nearPlaneSafetyFactor;
    const float maximumDistance = 200.0f;

    if (newDistance < minimumDistance)
        newDistance = minimumDistance;

    if (newDistance > maximumDistance)
        newDistance = maximumDistance;

    if (qAbs(newDistance - currentDistance) < 1.0e-6f)
        return true;

    return applyView(m_viewBounds, camera->forward(), camera->up(), newDistance);
}

/// 标准方向

bool CameraManager::viewFront()
{
    return setViewDirection(QVector3D(0.0f, 0.0f, -1.0f), QVector3D(0.0f, 1.0f, 0.0f));
}

bool CameraManager::viewBack()
{
    return setViewDirection(QVector3D(0.0f, 0.0f, 1.0f), QVector3D(0.0f, 1.0f, 0.0f));
}

bool CameraManager::viewLeft()
{
    return setViewDirection(QVector3D(1.0f, 0.0f, 0.0f), QVector3D(0.0f, 1.0f, 0.0f));
}

bool CameraManager::viewRight()
{
    return setViewDirection(QVector3D(-1.0f, 0.0f, 0.0f), QVector3D(0.0f, 1.0f, 0.0f));
}

bool CameraManager::viewTop()
{
    // Top View 沿 -Y 观察；使用 -Z 作为屏幕上方向，避免 Up 与 Forward 平行。
    return setViewDirection(QVector3D(0.0f, -1.0f, 0.0f), QVector3D(0.0f, 0.0f, -1.0f));
}

bool CameraManager::viewBottom()
{
    // Bottom View 沿 +Y 观察；使用 +Z 作为屏幕上方向，与 Top View 保持镜像语义。
    return setViewDirection(QVector3D(0.0f, 1.0f, 0.0f), QVector3D(0.0f, 0.0f, 1.0f));
}

bool CameraManager::viewIsometric()
{
    // 从 +X/+Y/+Z 八分体观察 View Bounds Center；+Y 作为世界 Up 参考。
    return setViewDirection(QVector3D(-1.0f, -1.0f, -1.0f), QVector3D(0.0f, 1.0f, 0.0f));
}

/// 内部视图计算

AxisAlignedBoundingBox CameraManager::createDefaultViewBounds()
{
    // 默认盒边长为 2、中心位于世界原点。
    // Viewer 尚未提供业务 Bounds 时，所有 Camera 操作仍然具有稳定的几何中心和空间尺度。
    return AxisAlignedBoundingBox(
        QVector3D(-1.0f, -1.0f, -1.0f),
        QVector3D(1.0f, 1.0f, 1.0f));
}

bool CameraManager::validateViewBounds(const char* operation) const
{
    if (m_viewBounds.isValid())
        return true;

    qWarning() << "CameraManager" << operation << "failed: View Bounds are invalid; CameraManager should always keep a default Bounds.";
    return false;
}

bool CameraManager::applyView(const AxisAlignedBoundingBox& bounds, const QVector3D& forward, const QVector3D& up, float distance)
{
    Camera* camera = activeCamera();

    if (camera == 0)
    {
        qWarning() << "CameraManager applyView failed: active camera does not exist.";
        return false;
    }

    if (!bounds.isValid())
    {
        qWarning() << "CameraManager applyView failed: bounds are invalid.";
        return false;
    }

    // 1e-8 用于避免零长度方向参与 Normalize 和 Cross Product。
    const float directionEpsilon = 1.0e-8f;

    if (forward.lengthSquared() <= directionEpsilon || up.lengthSquared() <= directionEpsilon)
    {
        qWarning() << "CameraManager applyView failed: view direction is invalid:" << camera->name();
        return false;
    }

    const QVector3D normalizedForward = forward.normalized();
    const QVector3D normalizedUp = up.normalized();

    if (QVector3D::crossProduct(normalizedForward, normalizedUp).lengthSquared() <= directionEpsilon)
    {
        qWarning() << "CameraManager applyView failed: up vector cannot be parallel to view direction:" << camera->name();
        return false;
    }

    // 所有 Manager 导航操作最终都归结为：
    // View Bounds Center -> Camera Target；
    // Forward + Distance -> Camera Position。
    const float minimumDistance = camera->nearPlane() * 2.5f;
    const float safeDistance = qMax(distance, minimumDistance);
    const QVector3D center = bounds.center();
    const QVector3D position = center - normalizedForward * safeDistance;

    return camera->setView(position, center, normalizedUp);
}

bool CameraManager::translatedViewBounds(const QVector3D& translation, AxisAlignedBoundingBox& translatedBounds) const
{
    translatedBounds.reset();

    if (!m_viewBounds.isValid())
    {
        qWarning() << "CameraManager translatedViewBounds failed: View Bounds are not set.";
        return false;
    }

    return translatedBounds.set(m_viewBounds.minimum() + translation, m_viewBounds.maximum() + translation);
}

bool CameraManager::fitBoundsInternal(const AxisAlignedBoundingBox& bounds, int viewportWidth, int viewportHeight, float margin)
{
    Camera* camera = activeCamera();

    if (camera == 0)
    {
        qWarning() << "CameraManager fitBoundsInternal failed: active camera does not exist.";
        return false;
    }

    if (!bounds.isValid())
    {
        qWarning() << "CameraManager fitBoundsInternal failed: bounds are invalid.";
        return false;
    }

    if (viewportWidth <= 0 || viewportHeight <= 0)
    {
        qWarning() << "CameraManager fitBoundsInternal failed: viewport size is invalid.";
        return false;
    }

    if (margin < 1.0f)
    {
        qWarning() << "CameraManager fitBoundsInternal failed: margin must be at least 1.0:" << margin;
        return false;
    }

    const QVector3D center = bounds.center();
    const QVector3D minimum = bounds.minimum();
    const QVector3D maximum = bounds.maximum();
    const QVector3D forward = camera->forward();
    const QVector3D right = camera->right();
    const QVector3D up = camera->viewUp();
    const float aspect = static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);

    const QVector3D corners[] =
    {
        QVector3D(minimum.x(), minimum.y(), minimum.z()),
        QVector3D(maximum.x(), minimum.y(), minimum.z()),
        QVector3D(minimum.x(), maximum.y(), minimum.z()),
        QVector3D(maximum.x(), maximum.y(), minimum.z()),
        QVector3D(minimum.x(), minimum.y(), maximum.z()),
        QVector3D(maximum.x(), minimum.y(), maximum.z()),
        QVector3D(minimum.x(), maximum.y(), maximum.z()),
        QVector3D(maximum.x(), maximum.y(), maximum.z())
    };

    if (camera->projectionType() == CameraProjectionOrthographic)
    {
        float maximumHorizontalExtent = 0.0f;
        float maximumVerticalExtent = 0.0f;

        for (int i = 0; i < 8; ++i)
        {
            const QVector3D relative = corners[i] - center;
            maximumHorizontalExtent = qMax(maximumHorizontalExtent, qAbs(QVector3D::dotProduct(relative, right)));
            maximumVerticalExtent = qMax(maximumVerticalExtent, qAbs(QVector3D::dotProduct(relative, up)));
        }

        const float requiredHeightFromWidth = maximumHorizontalExtent * 2.0f / aspect;
        float requiredHeight = qMax(maximumVerticalExtent * 2.0f, requiredHeightFromWidth) * margin;

        // 平面或点状 Bounds 仍保留最小正交高度，避免投影矩阵退化。
        const float minimumOrthographicHeight = 0.001f;

        if (requiredHeight < minimumOrthographicHeight)
            requiredHeight = minimumOrthographicHeight;

        // 正交 Fit 同样先把 Camera Target 对齐到 Bounds Center，再调整 Orthographic Height。
        if (!applyView(bounds, forward, camera->up(), camera->distanceToTarget()))
            return false;

        return camera->setOrthographic(requiredHeight, camera->nearPlane(), camera->farPlane());
    }

    const float verticalHalfFovRadians = qDegreesToRadians(camera->fieldOfView() * 0.5f);
    const float verticalTangent = qTan(verticalHalfFovRadians);
    const float horizontalTangent = verticalTangent * aspect;

    if (verticalTangent <= 0.0f || horizontalTangent <= 0.0f)
    {
        qWarning() << "CameraManager fitBoundsInternal failed: perspective field of view is invalid:" << camera->name();
        return false;
    }

    float requiredDistance = 0.0f;

    for (int i = 0; i < 8; ++i)
    {
        const QVector3D relative = corners[i] - center;
        const float horizontalOffset = qAbs(QVector3D::dotProduct(relative, right));
        const float verticalOffset = qAbs(QVector3D::dotProduct(relative, up));
        const float depthOffset = QVector3D::dotProduct(relative, forward);

        // Camera 位于 center - forward * distance。
        // Corner 到 Camera 的前向深度为 distance + depthOffset，因此分别满足水平和垂直 FOV 约束。
        requiredDistance = qMax(requiredDistance, horizontalOffset / horizontalTangent - depthOffset);
        requiredDistance = qMax(requiredDistance, verticalOffset / verticalTangent - depthOffset);

        // 最靠近 Camera 的 Corner 仍必须位于 Near Plane 之后。
        const float nearPlaneSafetyFactor = 1.25f;
        requiredDistance = qMax(requiredDistance, camera->nearPlane() * nearPlaneSafetyFactor - depthOffset);
    }

    requiredDistance *= margin;

    // 完全退化的 Bounds 仍保留一个最小观察距离。
    const float minimumDistance = camera->nearPlane() * 2.5f;

    if (requiredDistance < minimumDistance)
        requiredDistance = minimumDistance;

    return applyView(bounds, forward, camera->up(), requiredDistance);
}