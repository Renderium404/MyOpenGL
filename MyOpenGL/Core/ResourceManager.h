#ifndef RESOURCEMANAGER_H
#define RESOURCEMANAGER_H

#include "Resource.h"

#include <map>

/// Resource 在 ResourceManager 中的对象所有权类型。
enum ResourceOwnership
{
    ResourceManagerOwned, // ResourceManager 接管 Resource 对象生命周期，移除时负责 delete。
    ResourceExternalOwned // ResourceManager 只负责 GPU 生命周期和同步，不负责 delete。
};

/// ResourceManager 内部资源记录。
/// 除了保存 Resource 指针，还明确记录该对象是否由 ResourceManager 拥有。
struct ResourceEntry
{
    ResourceEntry();

    Resource* resource;
    ResourceOwnership ownership;
};

/// Resource GPU 生命周期和同步管理器。
/// 无论哪种方式，Resource 同一时间都只能注册到一个 ResourceManager。
class ResourceManager
{
public:
    ResourceManager();
    ~ResourceManager();



    /// Resource 注册
    /// 添加 Resource 并接管对象生命周期。
    ResourceId add(Resource* resource);
    /// 登记外部拥有的 Resource。
    /// ResourceManager 负责 GPU 生命周期，但不负责 delete。
    ResourceId registerResource(Resource* resource);
    /// 移除 add() 添加的 Resource。
    bool remove(ResourceId id, QOpenGLFunctions_3_3_Core* gl = 0);
    /// 取消 registerResource() 登记的 Resource。
    bool unregisterResource(ResourceId id, QOpenGLFunctions_3_3_Core* gl = 0);

    /// 清除所有注册资源。
    /// Resource 析构时会通过自身调试警告提示 GPU 状态未释放。
    void clear(QOpenGLFunctions_3_3_Core* gl = 0);

    /// Resource 查询
    Resource* get(ResourceId id);
    const Resource* get(ResourceId id) const;

    bool contains(ResourceId id) const;
    int count() const;

    ResourceOwnership ownership(ResourceId id) const;
    bool isOwned(ResourceId id) const;
    bool isBorrowed(ResourceId id) const;

    /// Dirty 状态
    bool markFullDirty(ResourceId id);
    void markAllFullDirty();

    /// GPU 同步
    bool syncResource(ResourceId id, QOpenGLFunctions_3_3_Core* gl);
    bool syncAll(QOpenGLFunctions_3_3_Core* gl);

    /// 只释放所有 Resource 的 GPU 状态。
    /// 不解除注册，也不 delete Resource 对象。
    bool releaseGL(QOpenGLFunctions_3_3_Core* gl);

private:
    /// 统一执行 Resource 注册。
    ResourceId addInternal(Resource* resource, ResourceOwnership ownership);

    /// 从 Manager 中解除一条 ResourceEntry。
    /// deleteResource 决定是否真正 delete 对象。
    bool removeInternal(
        ResourceId id,
        QOpenGLFunctions_3_3_Core* gl,
        bool deleteResource);

    ResourceId allocateId();

private:
    std::map<ResourceId, ResourceEntry> m_resources; // 当前注册的所有 Resource。
    ResourceId m_nextId;                             // 下一次 ResourceId 分配起点。
};

#endif // RESOURCEMANAGER_H
