#include "CameraManager.h"

#include "Scene/AxisAlignedBoundingBox.h"
#include "Scene/Scene.h"

#include <QDebug>
#include <QQuaternion>
#include <QtMath>

CameraManager::CameraManager()
    : m_nextId(1)
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

    // 第一个加入管理器的 Camera 自动成为 Active Camera。
    if (m_activeCameraId == InvalidCameraId)
        m_activeCameraId = id;

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
    m_activeCameraId = InvalidCameraId;
}

/// Active Camera

bool CameraManager::setActiveCamera(CameraId id)
{
    if (!contains(id))
    {
        qWarning() << "CameraManager setActiveCamera failed: camera does not exist:" << id;
        return false;
    }

    m_activeCameraId = id;
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

/// 视图导航

bool CameraManager::orbit(float yawDegrees, float pitchDegrees)
{
    Camera* camera = activeCamera();

    if (camera == 0)
    {
        qWarning() << "CameraManager orbit failed: active camera does not exist.";
        return false;
    }

    QVector3D offset = camera->position() - camera->target();
    const QVector3D orbitUp = camera->up().normalized();

    // Yaw 始终允许绕 Orbit Up 旋转，即使 Camera 已经位于 Pitch 极限附近。
    const QQuaternion yawRotation = QQuaternion::fromAxisAndAngle(orbitUp, yawDegrees);
    offset = yawRotation.rotatedVector(offset);

    const QVector3D yawForward = (-offset).normalized();
    const QVector3D pitchAxis = QVector3D::crossProduct(yawForward, orbitUp).normalized();

    // Forward 与 Up 的点积等于当前 Elevation 的正弦值；限制到 [-1, 1] 避免浮点误差进入 asin() 非法范围。
    float upDot = QVector3D::dotProduct(yawForward, orbitUp);

    if (upDot < -1.0f)
        upDot = -1.0f;
    else if (upDot > 1.0f)
        upDot = 1.0f;

    const float currentPitchDegrees = qRadiansToDegrees(qAsin(upDot));

    // 保留 1 度极点余量，避免 Forward 与 Up 平行后 Right 方向退化。
    const float maximumPitchDegrees = 89.0f;
    float targetPitchDegrees = currentPitchDegrees + pitchDegrees;

    if (targetPitchDegrees < -maximumPitchDegrees)
        targetPitchDegrees = -maximumPitchDegrees;
    else if (targetPitchDegrees > maximumPitchDegrees)
        targetPitchDegrees = maximumPitchDegrees;

    const float appliedPitchDegrees = targetPitchDegrees - currentPitchDegrees;

    if (qAbs(appliedPitchDegrees) > 1.0e-6f)
    {
        const QQuaternion pitchRotation = QQuaternion::fromAxisAndAngle(pitchAxis, appliedPitchDegrees);
        offset = pitchRotation.rotatedVector(offset);
    }

    return camera->setView(camera->target() + offset, camera->target(), orbitUp);
}

bool CameraManager::pan(float rightDistance, float upDistance)
{
    Camera* camera = activeCamera();

    if (camera == 0)
    {
        qWarning() << "CameraManager pan failed: active camera does not exist.";
        return false;
    }

    const QVector3D translation = camera->right() * rightDistance + camera->viewUp() * upDistance;
    return camera->setView(camera->position() + translation, camera->target() + translation, camera->up());
}

bool CameraManager::zoom(float factor)
{
    Camera* camera = activeCamera();

    if (camera == 0)
    {
        qWarning() << "CameraManager zoom failed: active camera does not exist.";
        return false;
    }

    if (factor <= 0.0f)
    {
        qWarning() << "CameraManager zoom failed: factor must be greater than zero:" << camera->name();
        return false;
    }

    if (camera->projectionType() == CameraProjectionOrthographic)
    {
        // 正交相机通过改变可视高度实现 Zoom，不需要移动 Camera Position。
        const float newHeight = camera->orthographicHeight() / factor;

        // 0.001 世界单位作为最小正交视图高度，避免无限放大导致数值退化。
        if (newHeight < 0.001f)
        {
            qWarning() << "CameraManager zoom rejected: orthographic height is too small:" << camera->name();
            return false;
        }

        return camera->setOrthographic(newHeight, camera->nearPlane(), camera->farPlane());
    }

    // 透视相机通过改变 Position 与 Target 的距离实现 Zoom，Target 保持不变。
    const float currentDistance = camera->distanceToTarget();
    float newDistance = currentDistance / factor;

    // Camera 必须与 Near Plane 保持足够距离，否则 Orbit Target 会进入 Near Plane 裁剪区域。
    const float nearPlaneSafetyFactor = 2.5f;
    const float minimumDistance = camera->nearPlane() * nearPlaneSafetyFactor;
    const float maximumDistance = 200.0f;

    if (newDistance < minimumDistance)
        newDistance = minimumDistance;

    if (newDistance > maximumDistance)
        newDistance = maximumDistance;

    if (qAbs(newDistance - currentDistance) < 1.0e-6f)
        return true;

    const QVector3D newPosition = camera->target() - camera->forward() * newDistance;
    return camera->setView(newPosition, camera->target(), camera->up());
}

bool CameraManager::focus(const QVector3D& target)
{
    Camera* camera = activeCamera();

    if (camera == 0)
    {
        qWarning() << "CameraManager focus failed: active camera does not exist.";
        return false;
    }

    // Position 与 Target 同量平移，因此保持 Forward、Up 和 Camera Distance 不变。
    const QVector3D translation = target - camera->target();
    return camera->setView(camera->position() + translation, target, camera->up());
}

bool CameraManager::fitBounds(const AxisAlignedBoundingBox& bounds, int viewportWidth, int viewportHeight, float margin)
{
    Camera* camera = activeCamera();

    if (camera == 0)
    {
        qWarning() << "CameraManager fitBounds failed: active camera does not exist.";
        return false;
    }

    if (!bounds.isValid())
    {
        qWarning() << "CameraManager fitBounds failed: bounds are invalid.";
        return false;
    }

    if (viewportWidth <= 0 || viewportHeight <= 0)
    {
        qWarning() << "CameraManager fitBounds failed: viewport size is invalid.";
        return false;
    }

    if (margin < 1.0f)
    {
        qWarning() << "CameraManager fitBounds failed: margin must be at least 1.0:" << margin;
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

        // 正交 Fit 只平移 Position / Target 并调整 Orthographic Height，观察方向和 Camera Distance 保持不变。
        const QVector3D translation = center - camera->target();

        if (!camera->setView(camera->position() + translation, center, camera->up()))
            return false;

        return camera->setOrthographic(requiredHeight, camera->nearPlane(), camera->farPlane());
    }

    const float verticalHalfFovRadians = qDegreesToRadians(camera->fieldOfView() * 0.5f);
    const float verticalTangent = qTan(verticalHalfFovRadians);
    const float horizontalTangent = verticalTangent * aspect;

    if (verticalTangent <= 0.0f || horizontalTangent <= 0.0f)
    {
        qWarning() << "CameraManager fitBounds failed: perspective field of view is invalid:" << camera->name();
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

    const QVector3D newPosition = center - forward * requiredDistance;
    return camera->setView(newPosition, center, camera->up());
}

bool CameraManager::fitAll(const Scene& scene, int viewportWidth, int viewportHeight, float margin)
{
    AxisAlignedBoundingBox bounds;

    if (!scene.worldBounds(bounds, true))
    {
        qWarning() << "CameraManager fitAll failed: visible Scene contains no valid Bounds.";
        return false;
    }

    // Scene 只负责聚合 Bounds；实际 Perspective / Orthographic 取景统一由 fitBounds() 完成。
    return fitBounds(bounds, viewportWidth, viewportHeight, margin);
}

/// 标准视图

bool CameraManager::viewFront(const Scene& scene, int viewportWidth, int viewportHeight, float margin)
{
    return setStandardView(scene, viewportWidth, viewportHeight, QVector3D(0.0f, 0.0f, -1.0f), QVector3D(0.0f, 1.0f, 0.0f), margin);
}

bool CameraManager::viewBack(const Scene& scene, int viewportWidth, int viewportHeight, float margin)
{
    return setStandardView(scene, viewportWidth, viewportHeight, QVector3D(0.0f, 0.0f, 1.0f), QVector3D(0.0f, 1.0f, 0.0f), margin);
}

bool CameraManager::viewLeft(const Scene& scene, int viewportWidth, int viewportHeight, float margin)
{
    return setStandardView(scene, viewportWidth, viewportHeight, QVector3D(1.0f, 0.0f, 0.0f), QVector3D(0.0f, 1.0f, 0.0f), margin);
}

bool CameraManager::viewRight(const Scene& scene, int viewportWidth, int viewportHeight, float margin)
{
    return setStandardView(scene, viewportWidth, viewportHeight, QVector3D(-1.0f, 0.0f, 0.0f), QVector3D(0.0f, 1.0f, 0.0f), margin);
}

bool CameraManager::viewTop(const Scene& scene, int viewportWidth, int viewportHeight, float margin)
{
    // Top View 沿 -Y 观察；使用 -Z 作为屏幕上方向，避免 Up 与 Forward 平行。
    return setStandardView(scene, viewportWidth, viewportHeight, QVector3D(0.0f, -1.0f, 0.0f), QVector3D(0.0f, 0.0f, -1.0f), margin);
}

bool CameraManager::viewBottom(const Scene& scene, int viewportWidth, int viewportHeight, float margin)
{
    // Bottom View 沿 +Y 观察；使用 +Z 作为屏幕上方向，与 Top View 保持镜像语义。
    return setStandardView(scene, viewportWidth, viewportHeight, QVector3D(0.0f, 1.0f, 0.0f), QVector3D(0.0f, 0.0f, 1.0f), margin);
}

bool CameraManager::viewIsometric(const Scene& scene, int viewportWidth, int viewportHeight, float margin)
{
    // 从 +X/+Y/+Z 八分体观察原点方向；+Y 继续作为世界 Up 参考，使竖直方向保持直观。
    return setStandardView(scene, viewportWidth, viewportHeight, QVector3D(-1.0f, -1.0f, -1.0f), QVector3D(0.0f, 1.0f, 0.0f), margin);
}

bool CameraManager::setStandardView(const Scene& scene, int viewportWidth, int viewportHeight, const QVector3D& forward, const QVector3D& up, float margin)
{
    Camera* camera = activeCamera();

    if (camera == 0)
    {
        qWarning() << "CameraManager setStandardView failed: active camera does not exist.";
        return false;
    }

    const float directionEpsilon = 1.0e-8f;

    if (forward.lengthSquared() <= directionEpsilon || up.lengthSquared() <= directionEpsilon)
    {
        qWarning() << "CameraManager setStandardView failed: view direction is invalid:" << camera->name();
        return false;
    }

    const QVector3D normalizedForward = forward.normalized();
    const QVector3D normalizedUp = up.normalized();

    if (QVector3D::crossProduct(normalizedForward, normalizedUp).lengthSquared() <= directionEpsilon)
    {
        qWarning() << "CameraManager setStandardView failed: up vector cannot be parallel to view direction:" << camera->name();
        return false;
    }

    // 先只改变观察方向并保持当前 Target / Distance；随后统一复用 fitAll() 完成 Scene 居中和距离计算。
    // 这样所有标准视图与自由视角 Fit All 使用同一套 Bounds / FOV 规则。
    float distance = camera->distanceToTarget();
    const float minimumDistance = camera->nearPlane() * 2.5f;

    if (distance < minimumDistance)
        distance = minimumDistance;

    const QVector3D target = camera->target();
    const QVector3D position = target - normalizedForward * distance;

    if (!camera->setView(position, target, normalizedUp))
        return false;

    return fitAll(scene, viewportWidth, viewportHeight, margin);
}

