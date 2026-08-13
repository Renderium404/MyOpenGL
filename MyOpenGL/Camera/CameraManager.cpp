#include "CameraManager.h"
#include <QtMath>
#include <QDebug>
#include <QQuaternion>

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