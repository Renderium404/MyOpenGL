#include "CameraManager.h"

#include <QDebug>
#include <QMatrix3x3>
#include <QtMath>

CameraManager::CameraManager()
    : m_nextId(1)
    , m_activeCameraId(InvalidCameraId)
    , m_viewBounds()
{
}

CameraManager::~CameraManager()
{
    clear();
}

/// Camera 管理

Camera* CameraManager::createCamera(const QString& name)
{
    const CameraId id = allocateId();

    if (id == InvalidCameraId)
    {
        qWarning() << "CameraManager createCamera failed: unable to allocate CameraId:" << name;
        return 0;
    }

    Camera* camera = new Camera(name);

    camera->setId(id);
    m_cameras[id] = camera;

    if (m_activeCameraId == InvalidCameraId)
        m_activeCameraId = id;

    return camera;
}

Camera* CameraManager::get(CameraId id)
{
    CameraMap::iterator it = m_cameras.find(id);
    return it != m_cameras.end() ? it->second : 0;
}

const Camera* CameraManager::get(CameraId id) const
{
    CameraMap::const_iterator it = m_cameras.find(id);
    return it != m_cameras.end() ? it->second : 0;
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
        return false;

    Camera* camera = it->second;
    const bool wasActive = m_activeCameraId == id;

    m_cameras.erase(it);

    if (camera != 0)
    {
        camera->setId(InvalidCameraId);
        delete camera;
    }

    if (wasActive)
        m_activeCameraId = m_cameras.empty() ? InvalidCameraId : m_cameras.begin()->first;

    return true;
}

void CameraManager::clear()
{
    CameraMap::iterator it = m_cameras.begin();

    while (it != m_cameras.end())
    {
        Camera* camera = it->second;

        if (camera != 0)
        {
            camera->setId(InvalidCameraId);
            delete camera;
        }

        ++it;
    }

    m_cameras.clear();
    m_nextId = 1;
    m_activeCameraId = InvalidCameraId;
    m_viewBounds.reset();
}

/// Active Camera

bool CameraManager::setActiveCamera(CameraId id)
{
    if (!contains(id))
        return false;

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

/// Camera Navigation

bool CameraManager::orbitAround(const QVector3D& anchor, float yaw, float pitch)
{
    Camera* camera = activeCamera();

    if (camera == 0)
    {
        qWarning() << "CameraManager orbitAround failed: active camera does not exist.";
        return false;
    }

    const QVector3D offset = camera->position() - anchor;

    if (offset.lengthSquared() <= 1.0e-12f)
    {
        qWarning() << "CameraManager orbitAround failed: camera is too close to anchor.";
        return false;
    }

    const QQuaternion yawRotation = QQuaternion::fromAxisAndAngle(camera->up(), yaw);
    const QVector3D pitchAxis = yawRotation.rotatedVector(camera->right()).normalized();
    const QQuaternion pitchRotation = QQuaternion::fromAxisAndAngle(pitchAxis, pitch);
    const QQuaternion rotation = (pitchRotation * yawRotation).normalized();

    const QVector3D newPosition = anchor + rotation.rotatedVector(offset);
    const QQuaternion newOrientation = (rotation * camera->orientation()).normalized();

    return camera->setCamera(newPosition, newOrientation);
}

bool CameraManager::panAt(const QVector3D& anchor, float deltaX, float deltaY, int viewportWidth, int viewportHeight)
{
    Camera* camera = activeCamera();

    if (camera == 0)
    {
        qWarning() << "CameraManager panAt failed: active camera does not exist.";
        return false;
    }

    if (viewportWidth <= 0 || viewportHeight <= 0)
    {
        qWarning() << "CameraManager panAt failed: viewport size is invalid.";
        return false;
    }

    float worldUnitsPerPixel = 0.0f;

    if (camera->projectionType() == ProjectionType::Perspective)
    {
        const float depth = QVector3D::dotProduct(anchor - camera->position(), camera->forward());

        if (depth <= 1.0e-8f)
        {
            qWarning() << "CameraManager panAt failed: anchor is behind camera.";
            return false;
        }

        const float halfFieldOfView = qDegreesToRadians(camera->perspectiveFieldOfView() * 0.5f);
        const float worldHeight = 2.0f * depth * qTan(halfFieldOfView);

        worldUnitsPerPixel = worldHeight / static_cast<float>(viewportHeight);
    }
    else
    {
        worldUnitsPerPixel = camera->parallelHeight() / static_cast<float>(viewportHeight);
    }

    const QVector3D translation = camera->right() * (-deltaX * worldUnitsPerPixel) + camera->up() * (deltaY * worldUnitsPerPixel);

    return camera->setCamera(camera->position() + translation, camera->orientation());
}

bool CameraManager::zoomAt(const QVector3D& anchor, float factor, int viewportWidth, int viewportHeight)
{
    Camera* camera = activeCamera();

    if (camera == 0)
    {
        qWarning() << "CameraManager zoomAt failed: active camera does not exist.";
        return false;
    }

    if (factor <= 0.0f)
    {
        qWarning() << "CameraManager zoomAt failed: factor must be greater than zero.";
        return false;
    }

    if (viewportWidth <= 0 || viewportHeight <= 0)
    {
        qWarning() << "CameraManager zoomAt failed: viewport size is invalid.";
        return false;
    }

    const QVector3D anchorOffset = anchor - camera->position();
    const float depth = QVector3D::dotProduct(anchorOffset, camera->forward());

    if (depth <= 1.0e-8f)
    {
        qWarning() << "CameraManager zoomAt failed: anchor is behind camera.";
        return false;
    }

    if (camera->projectionType() == ProjectionType::Perspective)
    {
        const float minimumDepth = camera->nearPlane() * 1.01f;
        const float maximumDepth = camera->farPlane() * 0.99f;

        float newDepth = depth / factor;
        newDepth = qBound(minimumDepth, newDepth, maximumDepth);

        const float actualFactor = depth / newDepth;
        const QVector3D newPosition = anchor - anchorOffset / actualFactor;

        return camera->setCamera(newPosition, camera->orientation());
    }

    const float minimumHeight = 1.0e-6f;
    const float oldHeight = camera->parallelHeight();
    const float newHeight = qMax(oldHeight / factor, minimumHeight);
    const float actualFactor = oldHeight / newHeight;

    const float anchorX = QVector3D::dotProduct(anchorOffset, camera->right());
    const float anchorY = QVector3D::dotProduct(anchorOffset, camera->up());

    const QVector3D newAnchorOffset = camera->right() * (anchorX / actualFactor) + camera->up() * (anchorY / actualFactor) + camera->forward() * depth;
    const QVector3D newPosition = anchor - newAnchorOffset;

    if (!camera->setParallel(newHeight, camera->nearPlane(), camera->farPlane()))
        return false;

    return camera->setCamera(newPosition, camera->orientation());
}

bool CameraManager::setViewDirection(const QVector3D& anchor, const QVector3D& forward, const QVector3D& up)
{
    Camera* camera = activeCamera();

    if (camera == 0)
    {
        qWarning() << "CameraManager setViewDirection failed: active camera does not exist.";
        return false;
    }

    if (forward.lengthSquared() <= 1.0e-12f || up.lengthSquared() <= 1.0e-12f)
    {
        qWarning() << "CameraManager setViewDirection failed: forward or up is invalid.";
        return false;
    }

    QVector3D normalizedForward = forward.normalized();
    QVector3D normalizedUp = up.normalized();

    QVector3D right = QVector3D::crossProduct(normalizedForward, normalizedUp);

    if (right.lengthSquared() <= 1.0e-12f)
    {
        qWarning() << "CameraManager setViewDirection failed: forward and up are parallel.";
        return false;
    }

    right.normalize();

    // 重新正交化 Up，保证 Camera 三个局部轴互相垂直。
    normalizedUp = QVector3D::crossProduct(right, normalizedForward).normalized();

    float distance = (camera->position() - anchor).length();

    if (distance <= 1.0e-8f)
        distance = camera->nearPlane() * 2.0f;

    // Camera Local:
    // +X = Right
    // +Y = Up
    // +Z = Back，所以第三列为 -Forward。
    QMatrix3x3 rotation;

    rotation(0, 0) = right.x();
    rotation(1, 0) = right.y();
    rotation(2, 0) = right.z();

    rotation(0, 1) = normalizedUp.x();
    rotation(1, 1) = normalizedUp.y();
    rotation(2, 1) = normalizedUp.z();

    const QVector3D back = -normalizedForward;

    rotation(0, 2) = back.x();
    rotation(1, 2) = back.y();
    rotation(2, 2) = back.z();

    const QQuaternion orientation = QQuaternion::fromRotationMatrix(rotation).normalized();
    const QVector3D newPosition = anchor - normalizedForward * distance;

    return camera->setCamera(newPosition, orientation);
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
    m_viewBounds.reset();
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

    const QVector3D center = bounds.center();
    float distance = (center - camera->position()).length();

    if (distance <= 1.0e-8f)
        distance = camera->nearPlane() * 2.0f;

    const QVector3D newPosition = center - camera->forward() * distance;

    if (!camera->setCamera(newPosition, camera->orientation()))
        return false;

    m_viewBounds = bounds;

    return true;
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

    if (margin <= 0.0f)
    {
        qWarning() << "CameraManager fitBounds failed: margin must be greater than zero.";
        return false;
    }

    const QVector3D minimum = bounds.minimum();
    const QVector3D maximum = bounds.maximum();
    const QVector3D center = bounds.center();

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

    const QVector3D right = camera->right();
    const QVector3D up = camera->up();
    const QVector3D forward = camera->forward();

    const float aspect = static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);

    if (camera->projectionType() == ProjectionType::Perspective)
    {
        const float halfFieldOfView = qDegreesToRadians(camera->perspectiveFieldOfView() * 0.5f);
        const float verticalTangent = qTan(halfFieldOfView);
        const float horizontalTangent = verticalTangent * aspect;

        float requiredDistance = 0.0f;

        for (int i = 0; i < 8; ++i)
        {
            const QVector3D relative = corners[i] - center;

            const float horizontal = qAbs(QVector3D::dotProduct(relative, right));
            const float vertical = qAbs(QVector3D::dotProduct(relative, up));
            const float depthOffset = QVector3D::dotProduct(relative, forward);

            requiredDistance = qMax(requiredDistance, horizontal * margin / horizontalTangent - depthOffset);
            requiredDistance = qMax(requiredDistance, vertical * margin / verticalTangent - depthOffset);
            requiredDistance = qMax(requiredDistance, camera->nearPlane() * 1.01f - depthOffset);
        }

        if (requiredDistance <= 0.0f)
            requiredDistance = camera->nearPlane() * 2.0f;

        for (int i = 0; i < 8; ++i)
        {
            const float depth = requiredDistance + QVector3D::dotProduct(corners[i] - center, forward);

            if (depth >= camera->farPlane())
            {
                qWarning() << "CameraManager fitBounds failed: bounds exceed camera far plane.";
                return false;
            }
        }

        const QVector3D newPosition = center - forward * requiredDistance;

        if (!camera->setCamera(newPosition, camera->orientation()))
            return false;
    }
    else
    {
        float horizontalExtent = 0.0f;
        float verticalExtent = 0.0f;
        float minimumDepthOffset = 0.0f;
        float maximumDepthOffset = 0.0f;

        for (int i = 0; i < 8; ++i)
        {
            const QVector3D relative = corners[i] - center;

            horizontalExtent = qMax(horizontalExtent, qAbs(QVector3D::dotProduct(relative, right)));
            verticalExtent = qMax(verticalExtent, qAbs(QVector3D::dotProduct(relative, up)));

            const float depthOffset = QVector3D::dotProduct(relative, forward);

            if (i == 0)
            {
                minimumDepthOffset = depthOffset;
                maximumDepthOffset = depthOffset;
            }
            else
            {
                minimumDepthOffset = qMin(minimumDepthOffset, depthOffset);
                maximumDepthOffset = qMax(maximumDepthOffset, depthOffset);
            }
        }

        const float requiredHeight = qMax(verticalExtent * 2.0f, horizontalExtent * 2.0f / aspect) * margin;

        const float minimumCenterDepth = camera->nearPlane() * 1.01f - minimumDepthOffset;
        const float maximumCenterDepth = camera->farPlane() * 0.99f - maximumDepthOffset;

        if (minimumCenterDepth >= maximumCenterDepth)
        {
            qWarning() << "CameraManager fitBounds failed: bounds exceed camera depth range.";
            return false;
        }

        float centerDepth = QVector3D::dotProduct(center - camera->position(), forward);
        centerDepth = qBound(minimumCenterDepth, centerDepth, maximumCenterDepth);

        const QVector3D newPosition = center - forward * centerDepth;

        if (!camera->setParallel(qMax(requiredHeight, 1.0e-6f), camera->nearPlane(), camera->farPlane()))
            return false;

        if (!camera->setCamera(newPosition, camera->orientation()))
            return false;
    }

    m_viewBounds = bounds;

    return true;
}

/// CameraId

CameraId CameraManager::allocateId()
{
    while (m_nextId == InvalidCameraId || contains(m_nextId))
        ++m_nextId;

    const CameraId id = m_nextId;

    ++m_nextId;

    return id;
}