#include "Transform.h"

Transform::Transform()
    : m_position(0.0f, 0.0f, 0.0f)
    , m_rotation(1.0f, 0.0f, 0.0f, 0.0f)
    , m_scale(1.0f, 1.0f, 1.0f)
{
}

/// 变换状态

const QVector3D& Transform::position() const
{
    return m_position;
}

const QQuaternion& Transform::rotation() const
{
    return m_rotation;
}

const QVector3D& Transform::scale() const
{
    return m_scale;
}

void Transform::setPosition(const QVector3D& position)
{
    m_position = position;
}

void Transform::setRotation(const QQuaternion& rotation)
{
    m_rotation = rotation;
}

void Transform::setScale(const QVector3D& scale)
{
    m_scale = scale;
}

void Transform::setUniformScale(float scale)
{
    m_scale = QVector3D(scale, scale, scale);
}

void Transform::reset()
{
    m_position = QVector3D(0.0f, 0.0f, 0.0f);
    m_rotation = QQuaternion(1.0f, 0.0f, 0.0f, 0.0f);
    m_scale = QVector3D(1.0f, 1.0f, 1.0f);
}

/// Matrix

QMatrix4x4 Transform::matrix() const
{
    QMatrix4x4 result;
    result.translate(m_position);
    result.rotate(m_rotation);
    result.scale(m_scale);
    return result;
}