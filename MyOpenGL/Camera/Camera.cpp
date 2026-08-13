#include "Camera.h"

#include <QDebug>

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

QVector3D Camera::forward() const
{
    return (m_target - m_position).normalized();
}

QVector3D Camera::right() const
{
    return QVector3D::crossProduct(forward(), m_up).normalized();
}

QVector3D Camera::viewUp() const
{
    return QVector3D::crossProduct(right(), forward()).normalized();
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

    if (QVector3D::crossProduct(viewDirection.normalized(), up.normalized()).lengthSquared() <= directionEpsilon)
    {
        qWarning() << "Camera setView failed: up vector cannot be parallel to view direction:" << m_name;
        return false;
    }

    m_position = position;
    m_target = target;
    m_up = up.normalized();
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