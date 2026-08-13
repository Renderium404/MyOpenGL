#include "ResourceManager.h"

#include <QDebug>

ResourceManager::ResourceManager()
    : m_nextId(1)
{
}

ResourceManager::~ResourceManager()
{
    clear();
}

/// Resource 所有权

ResourceId ResourceManager::add(Resource* resource)
{
    if (resource == 0)
    {
        qWarning() << "ResourceManager add failed: resource is null.";
        return InvalidResourceId;
    }

    if (resource->id() != InvalidResourceId)
    {
        qWarning() << "ResourceManager add failed: resource already has an ID:" << resource->name();
        return InvalidResourceId;
    }

    const ResourceId id = allocateId();

    if (id == InvalidResourceId)
    {
        qWarning() << "ResourceManager add failed: unable to allocate ResourceId:" << resource->name();
        return InvalidResourceId;
    }

    resource->setId(id);
    m_resources[id] = resource;
    return id;
}

bool ResourceManager::remove(ResourceId id, QOpenGLFunctions_3_3_Core* gl)
{
    std::map<ResourceId, Resource*>::iterator it = m_resources.find(id);

    if (it == m_resources.end())
        return false;

    Resource* resource = it->second;

    if (resource->isInitialized())
    {
        if (gl == 0)
        {
            qWarning() << "ResourceManager remove failed: initialized resource requires OpenGL functions:" << resource->name();
            return false;
        }

        if (!resource->releaseGL(gl))
            return false;
    }

    delete resource;
    m_resources.erase(it);
    return true;
}

void ResourceManager::clear(QOpenGLFunctions_3_3_Core* gl)
{
    std::map<ResourceId, Resource*>::iterator it = m_resources.begin();

    while (it != m_resources.end())
    {
        Resource* resource = it->second;

        if (resource->isInitialized() && gl != 0)
            resource->releaseGL(gl);

        delete resource;
        ++it;
    }

    m_resources.clear();
    m_nextId = 1;
}

/// Resource 查询

Resource* ResourceManager::get(ResourceId id)
{
    std::map<ResourceId, Resource*>::iterator it = m_resources.find(id);

    if (it == m_resources.end())
        return 0;

    return it->second;
}

const Resource* ResourceManager::get(ResourceId id) const
{
    std::map<ResourceId, Resource*>::const_iterator it = m_resources.find(id);

    if (it == m_resources.end())
        return 0;

    return it->second;
}

bool ResourceManager::contains(ResourceId id) const
{
    return m_resources.find(id) != m_resources.end();
}

int ResourceManager::count() const
{
    return static_cast<int>(m_resources.size());
}

/// Dirty 状态

bool ResourceManager::markFullDirty(ResourceId id)
{
    Resource* resource = get(id);

    if (resource == 0)
        return false;

    resource->markFullDirty();
    return true;
}

void ResourceManager::markAllFullDirty()
{
    std::map<ResourceId, Resource*>::iterator it = m_resources.begin();

    while (it != m_resources.end())
    {
        it->second->markFullDirty();
        ++it;
    }
}

/// GPU 同步

bool ResourceManager::syncResource(ResourceId id, QOpenGLFunctions_3_3_Core* gl)
{
    if (gl == 0)
    {
        qWarning() << "ResourceManager syncResource failed: OpenGL functions are null.";
        return false;
    }

    Resource* resource = get(id);

    if (resource == 0)
    {
        qWarning() << "ResourceManager syncResource failed: resource does not exist:" << id;
        return false;
    }

    // External Resource 可在这里通过 Revision 检查刷新自己的 DirtyState。
    if (!resource->prepareSync())
    {
        qWarning() << "ResourceManager syncResource failed: resource preparation failed:" << resource->name();
        return false;
    }

    if (!resource->isInitialized())
        return resource->initializeGL(gl);

    switch (resource->dirtyState())
    {
    case ResourceClean:
        return true;

    case ResourcePartialDirty:
        return resource->updatePartialGL(gl);

    case ResourceFullDirty:
        return resource->updateFullGL(gl);
    }

    qWarning() << "ResourceManager syncResource failed: invalid dirty state:" << resource->name();
    return false;
}

bool ResourceManager::syncAll(QOpenGLFunctions_3_3_Core* gl)
{
    if (gl == 0)
    {
        qWarning() << "ResourceManager syncAll failed: OpenGL functions are null.";
        return false;
    }

    std::map<ResourceId, Resource*>::iterator it = m_resources.begin();

    while (it != m_resources.end())
    {
        if (!syncResource(it->first, gl))
            return false;

        ++it;
    }

    return true;
}

bool ResourceManager::releaseGL(QOpenGLFunctions_3_3_Core* gl)
{
    if (gl == 0)
    {
        qWarning() << "ResourceManager releaseGL failed: OpenGL functions are null.";
        return false;
    }

    bool result = true;

    std::map<ResourceId, Resource*>::iterator it = m_resources.begin();

    while (it != m_resources.end())
    {
        if (it->second->isInitialized() && !it->second->releaseGL(gl))
            result = false;

        ++it;
    }

    return result;
}

/// ResourceId

ResourceId ResourceManager::allocateId()
{
    while (m_nextId == InvalidResourceId || contains(m_nextId))
        ++m_nextId;

    const ResourceId id = m_nextId;
    ++m_nextId;

    return id;
}