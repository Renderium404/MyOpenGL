#ifndef RESOURCEMANAGER_H
#define RESOURCEMANAGER_H

#include "Resource.h"

#include <map>

/// Resource 所有权和 GPU 同步管理器。
/// add() 成功后接管 Resource 指针，并统一执行 Prepare / Initialize / Partial / Full GPU Synchronization。
class ResourceManager
{
public:
    ResourceManager();
    ~ResourceManager();

    /// Resource 所有权
    ResourceId add(Resource* resource);
    bool remove(ResourceId id, QOpenGLFunctions_3_3_Core* gl = 0);
    void clear(QOpenGLFunctions_3_3_Core* gl = 0);

    /// Resource 查询
    Resource* get(ResourceId id);
    const Resource* get(ResourceId id) const;
    bool contains(ResourceId id) const;
    int count() const;

    /// Dirty 状态
    bool markFullDirty(ResourceId id);
    void markAllFullDirty();

    /// GPU 同步
    bool syncResource(ResourceId id, QOpenGLFunctions_3_3_Core* gl);
    bool syncAll(QOpenGLFunctions_3_3_Core* gl);
    bool releaseGL(QOpenGLFunctions_3_3_Core* gl);

private:
    ResourceId allocateId();

private:
    std::map<ResourceId, Resource*> m_resources; // ResourceManager 拥有的 Resource。
    ResourceId m_nextId;                         // 下一次 ResourceId 分配起点。
};

#endif // RESOURCEMANAGER_H
