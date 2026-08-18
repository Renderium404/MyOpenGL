#include "Camera.h"

#include <QDebug>
#include <QMatrix3x3>
#include <QVector4D>
#include <QtMath>

const char* cameraProjectionTypeName(CameraProjectionType type)
{
    switch (type)
    {
    case CameraProjectionPerspective:
        return "Perspective";
    case CameraProjectionOrthographic:
        return "Orthographic";
    }

    return "Unknown";
}

Camera::Camera(const QString& name)
    : m_id(InvalidCameraId)
    , m_name(name)
    , m_projectionType(CameraProjectionPerspective)
    , m_position(0.0f, 0.0f, 5.0f)
    , m_target(0.0f, 0.0f, 0.0f)
    , m_up(0.0f, 1.0f, 0.0f)
    , m_orientation(1.0f, 0.0f, 0.0f, 0.0f) // 默认 Local Forward(-Z) 正对世界 -Z，Local Up(+Y) 对齐世界 +Y。
    , m_fieldOfView(45.0f)           // 常用透视垂直视场角，兼顾视野范围和透视变形。
    , m_orthographicHeight(10.0f)    // 默认正交视图从 -5 到 +5，共显示 10 个世界坐标单位。
    , m_nearPlane(0.1f)              // Near 必须大于 0，0.1 适合作为当前普通场景默认值。
    , m_farPlane(1000.0f)            // 为当前教学场景保留足够大的可视距离。
{
}

/// 相机基本信息

CameraId Camera::id() const
{
    return m_id;
}

const QString& Camera::name() const
{
    return m_name;
}

CameraProjectionType Camera::projectionType() const
{
    return m_projectionType;
}

/// 观察状态

const QVector3D& Camera::position() const
{
    return m_position;
}

const QVector3D& Camera::target() const
{
    return m_target;
}

const QVector3D& Camera::up() const
{
    return m_up;
}

const QQuaternion& Camera::orientation() const
{
    return m_orientation;
}

QVector3D Camera::forward() const
{
    return m_orientation.rotatedVector(QVector3D(0.0f, 0.0f, -1.0f)).normalized();
}

QVector3D Camera::right() const
{
    return m_orientation.rotatedVector(QVector3D(1.0f, 0.0f, 0.0f)).normalized();
}

QVector3D Camera::viewUp() const
{
    return m_orientation.rotatedVector(QVector3D(0.0f, 1.0f, 0.0f)).normalized();
}

float Camera::distanceToTarget() const
{
    return (m_position - m_target).length();
}

bool Camera::setView(const QVector3D& position, const QVector3D& target, const QVector3D& up)
{
    const QVector3D viewDirection = target - position;

    // 1e-8 用于避免零长度方向参与 Normalize 和 Cross Product。
    const float directionEpsilon = 1.0e-8f;

    if (viewDirection.lengthSquared() <= directionEpsilon)
    {
        qWarning() << "Camera setView failed: position and target cannot be the same:" << m_name;
        return false;
    }

    if (up.lengthSquared() <= directionEpsilon)
    {
        qWarning() << "Camera setView failed: up vector cannot be zero:" << m_name;
        return false;
    }

    const QVector3D normalizedForward = viewDirection.normalized();
    QVector3D normalizedRight = QVector3D::crossProduct(normalizedForward, up.normalized());

    if (normalizedRight.lengthSquared() <= directionEpsilon)
    {
        qWarning() << "Camera setView failed: up vector cannot be parallel to view direction:" << m_name;
        return false;
    }

    normalizedRight.normalize();
    const QVector3D normalizedUp = QVector3D::crossProduct(normalizedRight, normalizedForward).normalized();
    const QVector3D normalizedBackward = -normalizedForward;

    // Camera Local Basis:
    // +X = Right, +Y = Up, +Z = Backward，因此 Local Forward 固定为 -Z。
    // 将三个世界空间基础轴作为旋转矩阵列向量，再转换为四元数保存完整姿态。
    QMatrix3x3 rotationMatrix;
    rotationMatrix(0, 0) = normalizedRight.x();
    rotationMatrix(1, 0) = normalizedRight.y();
    rotationMatrix(2, 0) = normalizedRight.z();

    rotationMatrix(0, 1) = normalizedUp.x();
    rotationMatrix(1, 1) = normalizedUp.y();
    rotationMatrix(2, 1) = normalizedUp.z();

    rotationMatrix(0, 2) = normalizedBackward.x();
    rotationMatrix(1, 2) = normalizedBackward.y();
    rotationMatrix(2, 2) = normalizedBackward.z();

    m_position = position;
    m_target = target;
    m_orientation = QQuaternion::fromRotationMatrix(rotationMatrix).normalized();
    m_up = m_orientation.rotatedVector(QVector3D(0.0f, 1.0f, 0.0f)).normalized();
    return true;
}


/// Picking Ray

bool Camera::screenPointToRay(int screenX, int screenY, int viewportWidth, int viewportHeight, QVector3D& rayOrigin, QVector3D& rayDirection) const
{
    if (viewportWidth <= 0 || viewportHeight <= 0)
    {
        qWarning() << "Camera screenPointToRay failed: viewport size is invalid:" << m_name;
        return false;
    }

    // Qt Widget 像素原点位于左上角，而 OpenGL NDC 的 Y 正方向向上。
    const float normalizedX = static_cast<float>(screenX) / static_cast<float>(viewportWidth);
    const float normalizedY = static_cast<float>(screenY) / static_cast<float>(viewportHeight);
    const float ndcX = normalizedX * 2.0f - 1.0f;
    const float ndcY = 1.0f - normalizedY * 2.0f;
    const float aspect = static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);

    bool invertible = false;
    const QMatrix4x4 inverseViewProjection = (projectionMatrix(aspect) * viewMatrix()).inverted(&invertible);

    if (!invertible)
    {
        qWarning() << "Camera screenPointToRay failed: view-projection matrix is not invertible:" << m_name;
        return false;
    }

    QVector4D nearPoint = inverseViewProjection * QVector4D(ndcX, ndcY, -1.0f, 1.0f);
    QVector4D farPoint = inverseViewProjection * QVector4D(ndcX, ndcY, 1.0f, 1.0f);

    const float homogeneousEpsilon = 1.0e-8f;

    if (qAbs(nearPoint.w()) <= homogeneousEpsilon || qAbs(farPoint.w()) <= homogeneousEpsilon)
    {
        qWarning() << "Camera screenPointToRay failed: unprojected homogeneous coordinate is invalid:" << m_name;
        return false;
    }

    nearPoint /= nearPoint.w();
    farPoint /= farPoint.w();

    const QVector3D nearWorld = nearPoint.toVector3D();
    const QVector3D farWorld = farPoint.toVector3D();

    // 透视相机的所有 Picking Ray 从 Camera Position 发出；正交相机则从对应像素的 Near Plane 点发出。
    rayOrigin = m_projectionType == CameraProjectionPerspective ? m_position : nearWorld;
    rayDirection = farWorld - rayOrigin;

    if (rayDirection.lengthSquared() <= homogeneousEpsilon)
    {
        qWarning() << "Camera screenPointToRay failed: generated ray direction is zero:" << m_name;
        return false;
    }

    rayDirection.normalize();
    return true;
}

/// 透视投影

float Camera::fieldOfView() const
{
    return m_fieldOfView;
}

bool Camera::setPerspective(float fieldOfView, float nearPlane, float farPlane)
{
    // FOV 接近 0 或 180 度时透视矩阵会趋于退化，因此限制在有效开区间内。
    if (fieldOfView <= 0.0f || fieldOfView >= 180.0f)
    {
        qWarning() << "Camera setPerspective failed: fieldOfView must be between 0 and 180 degrees:" << m_name;
        return false;
    }

    if (nearPlane <= 0.0f || farPlane <= nearPlane)
    {
        qWarning() << "Camera setPerspective failed: projection planes are invalid:" << m_name;
        return false;
    }

    m_projectionType = CameraProjectionPerspective;
    m_fieldOfView = fieldOfView;
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
    return true;
}

/// 正交投影

float Camera::orthographicHeight() const
{
    return m_orthographicHeight;
}

bool Camera::setOrthographic(float height, float nearPlane, float farPlane)
{
    if (height <= 0.0f)
    {
        qWarning() << "Camera setOrthographic failed: height must be greater than zero:" << m_name;
        return false;
    }

    if (nearPlane <= 0.0f || farPlane <= nearPlane)
    {
        qWarning() << "Camera setOrthographic failed: projection planes are invalid:" << m_name;
        return false;
    }

    m_projectionType = CameraProjectionOrthographic;
    m_orthographicHeight = height;
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
    return true;
}

/// 公共投影参数

float Camera::nearPlane() const
{
    return m_nearPlane;
}

float Camera::farPlane() const
{
    return m_farPlane;
}

/// 矩阵

QMatrix4x4 Camera::viewMatrix() const
{
    QMatrix4x4 matrix;
    matrix.lookAt(m_position, m_target, viewUp());
    return matrix;
}

QMatrix4x4 Camera::projectionMatrix(float aspect) const
{
    QMatrix4x4 matrix;

    if (aspect <= 0.0f)
    {
        qWarning() << "Camera projectionMatrix failed: aspect must be greater than zero:" << m_name;
        return matrix;
    }

    if (m_projectionType == CameraProjectionPerspective)
    {
        matrix.perspective(m_fieldOfView, aspect, m_nearPlane, m_farPlane);
        return matrix;
    }

    const float halfHeight = m_orthographicHeight * 0.5f;
    const float halfWidth = halfHeight * aspect;
    matrix.ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, m_nearPlane, m_farPlane);
    return matrix;
}

/// CameraManager 内部接口

void Camera::setId(CameraId id)
{
    m_id = id;
}