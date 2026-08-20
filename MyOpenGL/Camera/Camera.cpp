#include "Camera.h"

Camera::Camera()
    : m_id(InvalidCameraId)
    , m_name("Camera")
    , m_position(0.0f, 0.0f, 0.0f)
    , m_orientation()
    , m_type(ProjectionType::Perspective)
    , m_perspectiveFieldOfView(45.0f)
    , m_perspectiveNearPlane(0.1f)
    , m_perspectiveFarPlane(1000.0f)
    , m_parallelHeight(10.0f)
    , m_parallelNearPlane(0.1f)
    , m_parallelFarPlane(1000.0f)
{
    setPerspective(45.0f, 0.1f, 1000.0f);
}
QString Camera::type() const
{
    switch (m_type)
    {
    case ProjectionType::Perspective: return "Perspective";
    case ProjectionType::Parallel: return "Parallel";
    }

    return "Unknown";
}
bool Camera::setPerspective(float fieldOfView, float nearPlane, float farPlane)
{
    if (fieldOfView <= 0.0f || fieldOfView >= 180.0f)
        return false;

    if (nearPlane <= 0.0f || farPlane <= nearPlane)
        return false;

    m_perspectiveFieldOfView = fieldOfView;
    m_perspectiveNearPlane = nearPlane;
    m_perspectiveFarPlane = farPlane;
    setType(ProjectionType::Perspective);
    return true;
}
bool Camera::setParallel(float height, float nearPlane, float farPlane)
{
    if (height <= 0.0f)
        return false;

    if (nearPlane <= 0.0f || farPlane <= nearPlane)
        return false;

    m_parallelHeight = height;
    m_parallelNearPlane = nearPlane;
    m_parallelFarPlane = farPlane;
    setType(ProjectionType::Parallel);
    return true;
}
bool Camera::setCamera(const QMatrix4x4& matrix)
{
    if (!matrix.isAffine())
        return false;

    m_position = matrix.column(3).toVector3D();

    QMatrix3x3 rotation;
    rotation(0, 0) = matrix(0, 0);
    rotation(0, 1) = matrix(0, 1);
    rotation(0, 2) = matrix(0, 2);

    rotation(1, 0) = matrix(1, 0);
    rotation(1, 1) = matrix(1, 1);
    rotation(1, 2) = matrix(1, 2);

    rotation(2, 0) = matrix(2, 0);
    rotation(2, 1) = matrix(2, 1);
    rotation(2, 2) = matrix(2, 2);

    m_orientation = QQuaternion::fromRotationMatrix(rotation).normalized();

    return true;
}
bool Camera::setCamera(const QVector3D& position, const QQuaternion& orientation)
{
    if (orientation.isNull())
        return false;

    m_position = position;
    m_orientation = orientation.normalized();
    return true;
}


QMatrix4x4 Camera::cameraMatrix() const
{
    QMatrix4x4 matrix;
    matrix.translate(m_position);
    matrix.rotate(m_orientation);
    return matrix;
}
QMatrix4x4 Camera::viewMatrix() const
{
    QMatrix4x4 view;
    view.rotate(m_orientation.conjugated());
    view.translate(-m_position);
    return view;
}
QVector3D Camera::forward() const
{
    return m_orientation.rotatedVector(QVector3D(0.0f, 0.0f, -1.0f));
}

QVector3D Camera::right() const
{
    return m_orientation.rotatedVector(QVector3D(1.0f, 0.0f, 0.0f));
}

QVector3D Camera::up() const
{
    return m_orientation.rotatedVector(QVector3D(0.0f, 1.0f, 0.0f));
}


QMatrix4x4 Camera::projectionMatrix(float aspect) const
{
    QMatrix4x4 projection;

    if (aspect <= 0.0f)
        return projection;

    switch (m_type)
    {
    case ProjectionType::Perspective:
        projection.perspective(
            m_perspectiveFieldOfView,
            aspect,
            m_perspectiveNearPlane,
            m_perspectiveFarPlane);
        break;

    case ProjectionType::Parallel:
    {
        const float halfHeight = m_parallelHeight * 0.5f;
        const float halfWidth = halfHeight * aspect;

        projection.ortho(
            -halfWidth,
             halfWidth,
            -halfHeight,
             halfHeight,
             m_parallelNearPlane,
             m_parallelFarPlane);
        break;
    }
    }

    return projection;
}

bool Camera::screenToCamera(float screenPointX, float screenPointY, float depth, int viewportWidth, int viewportHeight, QVector3D& cameraPoint)  const
{
    if (viewportWidth <= 0 || viewportHeight <= 0 || depth < 0.0f || depth > 1.0f)
        return false;

    const float ndcX = screenPointX / viewportWidth * 2.0f - 1.0f;
    const float ndcY = 1.0f - screenPointY / viewportHeight * 2.0f;
    const float ndcZ = depth * 2.0f - 1.0f;
    const float aspect = static_cast<float>(viewportWidth) / viewportHeight;

    bool invertible = false;
    const QMatrix4x4 inverseProjection = projectionMatrix(aspect).inverted(&invertible);

    if (!invertible)
        return false;

    QVector4D point = inverseProjection * QVector4D(ndcX, ndcY, ndcZ, 1.0f);

    if (qAbs(point.w()) <= 1.0e-8f)
        return false;

    point /= point.w();
    cameraPoint = point.toVector3D();
    return true;
}
bool Camera::cameraToScreen(const QVector3D& cameraPoint, int viewportWidth, int viewportHeight, float& screenPointX, float& screenPointY, float& depth) const
{
    if (viewportWidth <= 0 || viewportHeight <= 0)
        return false;

    const float aspect = static_cast<float>(viewportWidth) / viewportHeight;
    const QVector4D clipPoint = projectionMatrix(aspect) * QVector4D(cameraPoint, 1.0f);

    if (qAbs(clipPoint.w()) <= 1.0e-8f)
        return false;

    const QVector3D ndcPoint = clipPoint.toVector3D() / clipPoint.w();

    screenPointX = (ndcPoint.x() * 0.5f + 0.5f) * viewportWidth;
    screenPointY = (1.0f - (ndcPoint.y() * 0.5f + 0.5f)) * viewportHeight;
    depth = ndcPoint.z() * 0.5f + 0.5f;

    return true;
}
bool Camera::screenPointToRay(float screenPointX, float screenPointY, int viewportWidth, int viewportHeight, QVector3D& rayOrigin, QVector3D& rayDirection) const
{
    QVector3D nearCameraPoint;
    QVector3D farCameraPoint;

    if (!screenToCamera(screenPointX, screenPointY, 0.0f, viewportWidth, viewportHeight, nearCameraPoint))
        return false;

    if (!screenToCamera(screenPointX, screenPointY, 1.0f, viewportWidth, viewportHeight, farCameraPoint))
        return false;

    const QMatrix4x4 matrix = cameraMatrix();
    const QVector3D nearWorldPoint = (matrix * QVector4D(nearCameraPoint, 1.0f)).toVector3D();
    const QVector3D farWorldPoint = (matrix * QVector4D(farCameraPoint, 1.0f)).toVector3D();

    if (m_type == ProjectionType::Perspective)
        rayOrigin = m_position;
    else
        rayOrigin = nearWorldPoint;

    rayDirection = farWorldPoint - rayOrigin;

    if (rayDirection.lengthSquared() <= 1.0e-12f)
        return false;

    rayDirection.normalize();
    return true;
}