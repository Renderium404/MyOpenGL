#include "Light.h"

#include <QDebug>

Light::Light(const QString& name)
    : m_id(InvalidLightId)
    , m_name(name)
    , m_type(LightType::Ambient)
    , m_enabled(true)
    , m_color(1.0f, 1.0f, 1.0f)
    , m_intensity(1.0f)
    , m_position(0.0f, 0.0f, 0.0f)
    , m_direction(0.0f, -1.0f, 0.0f)
    , m_range(10.0f)
    , m_innerConeAngle(20.0f)
    , m_outerConeAngle(30.0f)
{
}

Light::~Light()
{
}

/// 基本信息

QString Light::type() const
{
    switch (m_type)
    {
    case LightType::Ambient:
        return "Ambient";

    case LightType::Directional:
        return "Directional";

    case LightType::Point:
        return "Point";

    case LightType::Spot:
        return "Spot";
    }

    return "Unknown";
}

/// 基础光照

bool Light::setColor(const QVector3D& color)
{
    if (color.x() < 0.0f || color.y() < 0.0f || color.z() < 0.0f)
    {
        qWarning() << "Light setColor failed: color components cannot be negative:" << m_name;
        return false;
    }

    m_color = color;
    return true;
}

bool Light::setIntensity(float intensity)
{
    if (intensity < 0.0f)
    {
        qWarning() << "Light setIntensity failed: intensity cannot be negative:" << m_name;
        return false;
    }

    m_intensity = intensity;
    return true;
}

/// 类型设置

void Light::setAmbient()
{
    m_type = LightType::Ambient;
}

bool Light::setDirectional(const QVector3D& direction)
{
    if (direction.lengthSquared() <= 1.0e-12f)
    {
        qWarning() << "Light setDirectional failed: direction cannot be zero:" << m_name;
        return false;
    }

    m_type = LightType::Directional;
    m_direction = direction.normalized();

    return true;
}

bool Light::setPoint(const QVector3D& position, float range)
{
    if (range <= 0.0f)
    {
        qWarning() << "Light setPoint failed: range must be greater than zero:" << m_name;
        return false;
    }

    m_type = LightType::Point;
    m_position = position;
    m_range = range;

    return true;
}

bool Light::setSpot(const QVector3D& position, const QVector3D& direction, float range, float innerConeAngle, float outerConeAngle)
{
    if (direction.lengthSquared() <= 1.0e-12f)
    {
        qWarning() << "Light setSpot failed: direction cannot be zero:" << m_name;
        return false;
    }

    if (range <= 0.0f)
    {
        qWarning() << "Light setSpot failed: range must be greater than zero:" << m_name;
        return false;
    }

    if (innerConeAngle < 0.0f || outerConeAngle <= 0.0f || outerConeAngle >= 90.0f)
    {
        qWarning() << "Light setSpot failed: cone angles are invalid:" << m_name;
        return false;
    }

    if (innerConeAngle >= outerConeAngle)
    {
        qWarning() << "Light setSpot failed: inner cone angle must be smaller than outer cone angle:" << m_name;
        return false;
    }

    m_type = LightType::Spot;
    m_position = position;
    m_direction = direction.normalized();
    m_range = range;
    m_innerConeAngle = innerConeAngle;
    m_outerConeAngle = outerConeAngle;

    return true;
}