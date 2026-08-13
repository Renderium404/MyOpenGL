#include "Light.h"

#include <QDebug>

const char* lightTypeName(LightType type)
{
    switch (type)
    {
    case LightTypeDirectional:
        return "Directional";
    case LightTypePoint:
        return "Point";
    case LightTypeSpot:
        return "Spot";
    }

    return "Unknown";
}

Light::Light(const QString& name)
    : m_id(InvalidLightId)
    , m_name(name)
    , m_type(LightTypeDirectional)
    , m_enabled(true)
    , m_color(1.0f, 1.0f, 1.0f)
    , m_intensity(1.0f)
    , m_position(0.0f, 0.0f, 0.0f)
    , m_direction(0.0f, -1.0f, 0.0f)
    , m_range(10.0f)             // 默认 Point / Spot 影响距离为 10 个世界坐标单位。
    , m_innerConeAngle(20.0f)    // 默认 Spot 中心 20 度半锥角保持完整光照。
    , m_outerConeAngle(30.0f)    // 默认 Spot 在 30 度半锥角处衰减至边界。
{
}

/// 灯光基本信息

LightId Light::id() const
{
    return m_id;
}

const QString& Light::name() const
{
    return m_name;
}

LightType Light::type() const
{
    return m_type;
}

bool Light::isEnabled() const
{
    return m_enabled;
}

void Light::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

/// 光照属性

const QVector3D& Light::color() const
{
    return m_color;
}

float Light::intensity() const
{
    return m_intensity;
}

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

/// 空间属性

const QVector3D& Light::position() const
{
    return m_position;
}

const QVector3D& Light::direction() const
{
    return m_direction;
}

float Light::range() const
{
    return m_range;
}

float Light::innerConeAngle() const
{
    return m_innerConeAngle;
}

float Light::outerConeAngle() const
{
    return m_outerConeAngle;
}

/// 灯光类型设置

bool Light::setDirectional(const QVector3D& direction)
{
    // 1e-8 用于避免零长度方向参与 Normalize。
    const float directionEpsilon = 1.0e-8f;

    if (direction.lengthSquared() <= directionEpsilon)
    {
        qWarning() << "Light setDirectional failed: direction cannot be zero:" << m_name;
        return false;
    }

    m_type = LightTypeDirectional;
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

    m_type = LightTypePoint;
    m_position = position;
    m_range = range;
    return true;
}

bool Light::setSpot(const QVector3D& position, const QVector3D& direction, float range, float innerConeAngle, float outerConeAngle)
{
    const float directionEpsilon = 1.0e-8f;

    if (direction.lengthSquared() <= directionEpsilon)
    {
        qWarning() << "Light setSpot failed: direction cannot be zero:" << m_name;
        return false;
    }

    if (range <= 0.0f)
    {
        qWarning() << "Light setSpot failed: range must be greater than zero:" << m_name;
        return false;
    }

    // Spot 使用半锥角；90 度以上会覆盖或超过整个半球，不作为当前聚光灯定义。
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

    m_type = LightTypeSpot;
    m_position = position;
    m_direction = direction.normalized();
    m_range = range;
    m_innerConeAngle = innerConeAngle;
    m_outerConeAngle = outerConeAngle;
    return true;
}

/// LightManager 内部接口

void Light::setId(LightId id)
{
    m_id = id;
}