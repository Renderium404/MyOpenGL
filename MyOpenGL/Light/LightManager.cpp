#include "LightManager.h"

#include <QDebug>

LightManager::LightManager()
    : m_nextId(1)
{
}

LightManager::~LightManager()
{
    clear();
}

/// 灯光管理

Light* LightManager::createLight(const QString& name)
{
    const LightId id = allocateId();

    if (id == InvalidLightId)
    {
        qWarning() << "LightManager createLight failed: unable to allocate LightId:" << name;
        return 0;
    }

    Light* light = new Light(name);

    light->setId(id);
    m_lights[id] = light;

    return light;
}

Light* LightManager::get(LightId id)
{
    LightMap::iterator it = m_lights.find(id);
    return it != m_lights.end() ? it->second : 0;
}

const Light* LightManager::get(LightId id) const
{
    LightMap::const_iterator it = m_lights.find(id);
    return it != m_lights.end() ? it->second : 0;
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

    Light* light = it->second;

    m_lights.erase(it);

    if (light != 0)
    {
        light->setId(InvalidLightId);
        delete light;
    }

    return true;
}

void LightManager::clear()
{
    LightMap::iterator it = m_lights.begin();

    while (it != m_lights.end())
    {
        Light* light = it->second;

        if (light != 0)
        {
            light->setId(InvalidLightId);
            delete light;
        }

        ++it;
    }

    m_lights.clear();
    m_nextId = 1;
}

/// 灯光查询

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

/// LightId

LightId LightManager::allocateId()
{
    while (m_nextId == InvalidLightId || contains(m_nextId))
        ++m_nextId;

    const LightId id = m_nextId;

    ++m_nextId;

    return id;
}