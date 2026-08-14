#include "AxisAlignedBoundingBox.h"

#include <QDebug>
#include <QtMath>

#include <cfloat>

AxisAlignedBoundingBox::AxisAlignedBoundingBox()
    : m_minimum(0.0f, 0.0f, 0.0f)
    , m_maximum(0.0f, 0.0f, 0.0f)
    , m_valid(false)
{
}

AxisAlignedBoundingBox::AxisAlignedBoundingBox(const QVector3D& minimum, const QVector3D& maximum)
    : m_minimum(0.0f, 0.0f, 0.0f)
    , m_maximum(0.0f, 0.0f, 0.0f)
    , m_valid(false)
{
    set(minimum, maximum);
}

/// Bounds 状态

bool AxisAlignedBoundingBox::isValid() const
{
    return m_valid;
}

const QVector3D& AxisAlignedBoundingBox::minimum() const
{
    return m_minimum;
}

const QVector3D& AxisAlignedBoundingBox::maximum() const
{
    return m_maximum;
}

QVector3D AxisAlignedBoundingBox::center() const
{
    if (!m_valid)
        return QVector3D();

    return (m_minimum + m_maximum) * 0.5f;
}

QVector3D AxisAlignedBoundingBox::size() const
{
    if (!m_valid)
        return QVector3D();

    return m_maximum - m_minimum;
}

void AxisAlignedBoundingBox::reset()
{
    m_minimum = QVector3D(0.0f, 0.0f, 0.0f);
    m_maximum = QVector3D(0.0f, 0.0f, 0.0f);
    m_valid = false;
}

bool AxisAlignedBoundingBox::set(const QVector3D& minimum, const QVector3D& maximum)
{
    if (minimum.x() > maximum.x() || minimum.y() > maximum.y() || minimum.z() > maximum.z())
    {
        qWarning() << "AxisAlignedBoundingBox set failed: minimum exceeds maximum.";
        return false;
    }

    m_minimum = minimum;
    m_maximum = maximum;
    m_valid = true;
    return true;
}

/// Bounds 扩展

void AxisAlignedBoundingBox::expandToInclude(const QVector3D& point)
{
    if (!m_valid)
    {
        m_minimum = point;
        m_maximum = point;
        m_valid = true;
        return;
    }

    m_minimum.setX(qMin(m_minimum.x(), point.x()));
    m_minimum.setY(qMin(m_minimum.y(), point.y()));
    m_minimum.setZ(qMin(m_minimum.z(), point.z()));

    m_maximum.setX(qMax(m_maximum.x(), point.x()));
    m_maximum.setY(qMax(m_maximum.y(), point.y()));
    m_maximum.setZ(qMax(m_maximum.z(), point.z()));
}

void AxisAlignedBoundingBox::expandToInclude(const AxisAlignedBoundingBox& bounds)
{
    if (!bounds.isValid())
        return;

    expandToInclude(bounds.minimum());
    expandToInclude(bounds.maximum());
}


/// 空间查询

bool AxisAlignedBoundingBox::intersectRay(const QVector3D& rayOrigin, const QVector3D& rayDirection, float& hitDistance) const
{
    hitDistance = 0.0f;

    if (!m_valid)
        return false;

    const float directionEpsilon = 1.0e-8f;

    if (rayDirection.lengthSquared() <= directionEpsilon)
    {
        qWarning() << "AxisAlignedBoundingBox intersectRay failed: ray direction is zero.";
        return false;
    }

    const float originValues[] = { rayOrigin.x(), rayOrigin.y(), rayOrigin.z() };
    const float directionValues[] = { rayDirection.x(), rayDirection.y(), rayDirection.z() };
    const float minimumValues[] = { m_minimum.x(), m_minimum.y(), m_minimum.z() };
    const float maximumValues[] = { m_maximum.x(), m_maximum.y(), m_maximum.z() };

    float minimumDistance = 0.0f;
    float maximumDistance = FLT_MAX;

    for (int axis = 0; axis < 3; ++axis)
    {
        const float origin = originValues[axis];
        const float direction = directionValues[axis];
        const float minimum = minimumValues[axis];
        const float maximum = maximumValues[axis];

        if (qAbs(direction) <= directionEpsilon)
        {
            // Ray 与当前 Slab 平行；Origin 不在 Slab 内时永远不可能命中。
            if (origin < minimum || origin > maximum)
                return false;

            continue;
        }

        float firstDistance = (minimum - origin) / direction;
        float secondDistance = (maximum - origin) / direction;

        if (firstDistance > secondDistance)
        {
            const float temporary = firstDistance;
            firstDistance = secondDistance;
            secondDistance = temporary;
        }

        minimumDistance = qMax(minimumDistance, firstDistance);
        maximumDistance = qMin(maximumDistance, secondDistance);

        if (minimumDistance > maximumDistance)
            return false;
    }

    // minimumDistance 从 0 开始，因此 Ray Origin 位于盒内时返回 0；否则返回最近的前向交点距离。
    hitDistance = minimumDistance;
    return true;
}

/// Transform

AxisAlignedBoundingBox AxisAlignedBoundingBox::transformed(const QMatrix4x4& matrix) const
{
    AxisAlignedBoundingBox result;

    if (!m_valid)
        return result;

    const QVector3D corners[] =
    {
        QVector3D(m_minimum.x(), m_minimum.y(), m_minimum.z()),
        QVector3D(m_maximum.x(), m_minimum.y(), m_minimum.z()),
        QVector3D(m_minimum.x(), m_maximum.y(), m_minimum.z()),
        QVector3D(m_maximum.x(), m_maximum.y(), m_minimum.z()),
        QVector3D(m_minimum.x(), m_minimum.y(), m_maximum.z()),
        QVector3D(m_maximum.x(), m_minimum.y(), m_maximum.z()),
        QVector3D(m_minimum.x(), m_maximum.y(), m_maximum.z()),
        QVector3D(m_maximum.x(), m_maximum.y(), m_maximum.z())
    };

    for (int i = 0; i < 8; ++i)
        result.expandToInclude(matrix.map(corners[i]));

    return result;
}
