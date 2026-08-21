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

Material* MaterialManager::createMaterial(const QString& name)
{
    const MaterialId id = allocateId();

    if (id == InvalidMaterialId)
    {
        qWarning() << "MaterialManager createMaterial failed: unable to allocate MaterialId:" << name;
        return 0;
    }

    Material* material = new Material(name);

    material->setId(id);
    m_materials[id] = material;

    return material;
}

Material* MaterialManager::get(MaterialId id)
{
    MaterialMap::iterator it = m_materials.find(id);
    return it != m_materials.end() ? it->second : 0;
}

const Material* MaterialManager::get(MaterialId id) const
{
    MaterialMap::const_iterator it = m_materials.find(id);
    return it != m_materials.end() ? it->second : 0;
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

    Material* material = it->second;

    m_materials.erase(it);

    if (material != 0)
    {
        material->setId(InvalidMaterialId);
        delete material;
    }

    return true;
}

void MaterialManager::clear()
{
    MaterialMap::iterator it = m_materials.begin();

    while (it != m_materials.end())
    {
        Material* material = it->second;

        if (material != 0)
        {
            material->setId(InvalidMaterialId);
            delete material;
        }

        ++it;
    }

    m_materials.clear();
    m_nextId = 1;
}

/// MaterialId

MaterialId MaterialManager::allocateId()
{
    while (m_nextId == InvalidMaterialId || contains(m_nextId))
        ++m_nextId;

    const MaterialId id = m_nextId;

    ++m_nextId;

    return id;
}