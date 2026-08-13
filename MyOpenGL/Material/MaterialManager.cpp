#include "MaterialManager.h"

#include <QDebug>

MaterialManager::MaterialManager()
    : m_nextId(1)
{
}

MaterialManager::~MaterialManager()
{
    clear();
}

/// 材质管理

MaterialId MaterialManager::add(Material* material)
{
    if (material == 0)
    {
        qWarning() << "MaterialManager add failed: material is null.";
        return InvalidMaterialId;
    }

    if (material->id() != InvalidMaterialId)
    {
        qWarning() << "MaterialManager add failed: material already has an id:" << material->name();
        return InvalidMaterialId;
    }

    const MaterialId id = m_nextId++;
    material->setId(id);
    m_materials[id] = material;
    return id;
}

Material* MaterialManager::get(MaterialId id)
{
    MaterialMap::iterator it = m_materials.find(id);

    if (it == m_materials.end())
        return 0;

    return it->second;
}

const Material* MaterialManager::get(MaterialId id) const
{
    MaterialMap::const_iterator it = m_materials.find(id);

    if (it == m_materials.end())
        return 0;

    return it->second;
}

bool MaterialManager::contains(MaterialId id) const
{
    return m_materials.find(id) != m_materials.end();
}

std::size_t MaterialManager::count() const
{
    return m_materials.size();
}

bool MaterialManager::remove(MaterialId id)
{
    MaterialMap::iterator it = m_materials.find(id);

    if (it == m_materials.end())
    {
        qWarning() << "MaterialManager remove failed: material does not exist:" << id;
        return false;
    }

    delete it->second;
    m_materials.erase(it);
    return true;
}

void MaterialManager::clear()
{
    MaterialMap::iterator it = m_materials.begin();

    while (it != m_materials.end())
    {
        delete it->second;
        ++it;
    }

    m_materials.clear();
}