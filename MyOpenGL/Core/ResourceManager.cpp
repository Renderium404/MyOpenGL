#include "ResourceManager.h"

#include <QDebug>

ResourceEntry::ResourceEntry()
    : resource(0)
    , ownership(ResourceOwnership::Owned)
{
}

ResourceManager::ResourceManager()
    : m_nextId(1)
{
}

ResourceManager::~ResourceManager()
{
    clear();
}

/// Resource 注册

ResourceId ResourceManager::adopt(Resource* resource)
{
    return registerInternal(resource, ResourceOwnership::Owned);
}

ResourceId ResourceManager::borrow(Resource* resource)
{
    return registerInternal(resource, ResourceOwnership::Borrowed);
}

ResourceId ResourceManager::registerInternal(Resource* resource, ResourceOwnership ownership)
{
    if (resource == 0)
    {
        qWarning() << "ResourceManager register failed: resource is null.";
        return InvalidResourceId;
    }

    if (resource->id() != InvalidResourceId)
    {
        qWarning()
            << "ResourceManager register failed: resource already has an ID:"
            << resource->name()
            << resource->id();

        return InvalidResourceId;
    }

    const ResourceId id = allocateId();

    if (id == InvalidResourceId)
    {
        qWarning()
            << "ResourceManager register failed: unable to allocate ResourceId:"
            << resource->name();

        return InvalidResourceId;
    }

    ResourceEntry entry;
    entry.resource = resource;
    entry.ownership = ownership;

    resource->setId(id);
    m_resources[id] = entry;

    return id;
}

/// Resource 查询

Resource* ResourceManager::get(ResourceId id)
{
    std::map<ResourceId, ResourceEntry>::iterator it = m_resources.find(id);

    if (it == m_resources.end())
        return 0;

    return it->second.resource;
}

const Resource* ResourceManager::get(ResourceId id) const
{
    std::map<ResourceId, ResourceEntry>::const_iterator it = m_resources.find(id);

    if (it == m_resources.end())
        return 0;

    return it->second.resource;
}

bool ResourceManager::contains(ResourceId id) const
{
    return m_resources.find(id) != m_resources.end();
}

std::size_t ResourceManager::count() const
{
    return m_resources.size();
}

bool ResourceManager::isOwned(ResourceId id) const
{
    std::map<ResourceId, ResourceEntry>::const_iterator it = m_resources.find(id);
    return it != m_resources.end() && it->second.ownership == ResourceOwnership::Owned;
}

bool ResourceManager::isBorrowed(ResourceId id) const
{
    std::map<ResourceId, ResourceEntry>::const_iterator it = m_resources.find(id);
    return it != m_resources.end() && it->second.ownership == ResourceOwnership::Borrowed;
}

/// Resource 移除

bool ResourceManager::remove(ResourceId id, QOpenGLFunctions_3_3_Core* gl)
{
    return removeInternal(id, gl);
}

bool ResourceManager::removeInternal(ResourceId id, QOpenGLFunctions_3_3_Core* gl)
{
    std::map<ResourceId, ResourceEntry>::iterator it = m_resources.find(id);

    if (it == m_resources.end())
        return false;

    ResourceEntry entry = it->second;
    Resource* resource = entry.resource;

    if (resource == 0)
    {
        m_resources.erase(it);
        return false;
    }

    if (resource->isInitialized())
    {
        if (gl == 0)
        {
            qWarning()
                << "ResourceManager remove failed:"
                << "initialized resource requires OpenGL functions:"
                << resource->name();

            return false;
        }

        if (!resource->releaseGL(gl))
        {
            qWarning()
                << "ResourceManager remove failed:"
                << "unable to release GPU state:"
                << resource->name();

            return false;
        }
    }

    // Resource 已经不再属于当前 Manager，
    // 因此必须恢复为 InvalidResourceId。
    resource->setId(InvalidResourceId);

    m_resources.erase(it);

    if (entry.ownership == ResourceOwnership::Owned)
        delete resource;

    return true;
}

void ResourceManager::clear(QOpenGLFunctions_3_3_Core* gl)
{
    std::map<ResourceId, ResourceEntry>::iterator it = m_resources.begin();

    while (it != m_resources.end())
    {
        ResourceEntry entry = it->second;
        Resource* resource = entry.resource;

        ++it;

        if (resource == 0)
            continue;

        if (resource->isInitialized())
        {
            if (gl != 0)
            {
                if (!resource->releaseGL(gl))
                {
                    qWarning()
                        << "ResourceManager clear:"
                        << "unable to release GPU state:"
                        << resource->name();
                }
            }
            else
            {
                qWarning()
                    << "ResourceManager clear:"
                    << "initialized resource cannot release GPU state without OpenGL functions:"
                    << resource->name();
            }
        }

        resource->setId(InvalidResourceId);

        if (entry.ownership == ResourceOwnership::Owned)
            delete resource;
    }

    m_resources.clear();
    m_nextId = 1;
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
    std::map<ResourceId, ResourceEntry>::iterator it = m_resources.begin();

    while (it != m_resources.end())
    {
        Resource* resource = it->second.resource;

        if (resource != 0)
            resource->markFullDirty();

        ++it;
    }
}

/// GPU 同步

bool ResourceManager::syncResource(ResourceId id, QOpenGLFunctions_3_3_Core* gl)
{
    if (gl == 0)
    {
        qWarning()
            << "ResourceManager syncResource failed:"
            << "OpenGL functions are null.";

        return false;
    }

    Resource* resource = get(id);

    if (resource == 0)
    {
        qWarning()
            << "ResourceManager syncResource failed:"
            << "resource does not exist:"
            << id;

        return false;
    }

    // External Resource 可以在这里通过 Revision
    // 检查并刷新自己的 DirtyState。
    if (!resource->prepareSync())
    {
        qWarning()
            << "ResourceManager syncResource failed:"
            << "resource preparation failed:"
            << resource->name();

        return false;
    }

    if (!resource->isInitialized())
        return resource->initializeGL(gl);

    switch (resource->dirtyState())
    {
    case ResourceDirtyState::Clean:
        return true;

    case ResourceDirtyState::Partial:
        return resource->updatePartialGL(gl);

    case ResourceDirtyState::Full:
        return resource->updateFullGL(gl);
    }

    qWarning()
        << "ResourceManager syncResource failed:"
        << "invalid dirty state:"
        << resource->name();

    return false;
}

bool ResourceManager::syncAll(QOpenGLFunctions_3_3_Core* gl)
{
    if (gl == 0)
    {
        qWarning()
            << "ResourceManager syncAll failed:"
            << "OpenGL functions are null.";

        return false;
    }

    std::map<ResourceId, ResourceEntry>::iterator it = m_resources.begin();

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
        qWarning()
            << "ResourceManager releaseGL failed:"
            << "OpenGL functions are null.";

        return false;
    }

    bool result = true;
    std::map<ResourceId, ResourceEntry>::iterator it = m_resources.begin();

    while (it != m_resources.end())
    {
        Resource* resource = it->second.resource;

        if (resource != 0 && resource->isInitialized() && !resource->releaseGL(gl))
        {
            qWarning()
                << "ResourceManager releaseGL failed:"
                << resource->name();

            result = false;
        }

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
