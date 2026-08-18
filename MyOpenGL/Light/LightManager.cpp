#include "LightManager.h"

#include <QDebug>

LightManager::LightManager()
    : m_nextId(1)
    , m_ambientColor(1.0f, 1.0f, 1.0f)
    , m_ambientIntensity(0.1f) // 默认使用较弱环境光，避免没有直接光照的表面完全为黑色。
{
}

LightManager::~LightManager()
{
    clear();
}

/// 灯光管理

LightId LightManager::add(Light* light)
{
    if (light == 0)
    {
        qWarning() << "LightManager add failed: light is null.";
        return InvalidLightId;
    }

    if (light->id() != InvalidLightId)
    {
        qWarning() << "LightManager add failed: light already has an id:" << light->name();
        return InvalidLightId;
    }

    const LightId id = m_nextId++;
    light->setId(id);
    m_lights[id] = light;
    return id;
}

Light* LightManager::get(LightId id)
{
    LightMap::iterator it = m_lights.find(id);

    if (it == m_lights.end())
        return 0;

    return it->second;
}

const Light* LightManager::get(LightId id) const
{
    LightMap::const_iterator it = m_lights.find(id);

    if (it == m_lights.end())
        return 0;

    return it->second;
}

const Light* LightManager::firstEnabledDirectionalLight() const
{
    LightMap::const_iterator it = m_lights.begin();

    while (it != m_lights.end())
    {
        const Light* light = it->second;

        if (light->isEnabled() && light->type() == LightTypeDirectional)
            return light;

        ++it;
    }

    return 0;
}

void LightManager::enabledLights(std::vector<const Light*>& lights) const
{
    lights.clear();
    lights.reserve(m_lights.size());

    LightMap::const_iterator it = m_lights.begin();

    while (it != m_lights.end())
    {
        const Light* light = it->second;

        if (light != 0 && light->isEnabled())
            lights.push_back(light);

        ++it;
    }
}

bool LightManager::contains(LightId id) const
{
    return m_lights.find(id) != m_lights.end();
}

std::size_t LightManager::count() const
{
    return m_lights.size();
}

bool LightManager::remove(LightId id)
{
    LightMap::iterator it = m_lights.find(id);

    if (it == m_lights.end())
    {
        qWarning() << "LightManager remove failed: light does not exist:" << id;
        return false;
    }

    delete it->second;
    m_lights.erase(it);
    return true;
}

void LightManager::clear()
{
    LightMap::iterator it = m_lights.begin();

    while (it != m_lights.end())
    {
        delete it->second;
        ++it;
    }

    m_lights.clear();
}

/// 环境光

const QVector3D& LightManager::ambientColor() const
{
    return m_ambientColor;
}

float LightManager::ambientIntensity() const
{
    return m_ambientIntensity;
}

bool LightManager::setAmbientColor(const QVector3D& color)
{
    if (color.x() < 0.0f || color.y() < 0.0f || color.z() < 0.0f)
    {
        qWarning() << "LightManager setAmbientColor failed: color components cannot be negative.";
        return false;
    }

    m_ambientColor = color;
    return true;
}

bool LightManager::setAmbientIntensity(float intensity)
{
    if (intensity < 0.0f)
    {
        qWarning() << "LightManager setAmbientIntensity failed: intensity cannot be negative.";
        return false;
    }

    m_ambientIntensity = intensity;
    return true;
}
