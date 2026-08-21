#ifndef RESOURCEMANAGER_H
#define RESOURCEMANAGER_H

#include "Resource.h"

#include <cstddef>
#include <map>

/// Resource 在 ResourceManager 中的对象所有权类型。
enum class ResourceOwnership
{
    Owned,   // ResourceManager 接管 Resource 对象生命周期，移除时负责 delete。
    Borrowed // ResourceManager 只负责 GPU 生命周期和同步，不负责 delete。
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

    /// 接管 Resource 对象生命周期。
    ResourceId adopt(Resource* resource);

    /// 借用外部拥有的 Resource。
    /// ResourceManager 负责 GPU 生命周期和同步，但不负责 delete。
    ResourceId borrow(Resource* resource);

    /// Resource 查询
    Resource* get(ResourceId id);
    const Resource* get(ResourceId id) const;

    bool contains(ResourceId id) const;
    std::size_t count() const;

    bool isOwned(ResourceId id) const;
    bool isBorrowed(ResourceId id) const;

    /// Resource 移除

    /// 从 ResourceManager 中移除指定 Resource。
    /// Owned Resource 会被 delete；Borrowed Resource 只解除注册。
    bool remove(ResourceId id, QOpenGLFunctions_3_3_Core* gl = 0);

    /// 清除所有注册资源。
    /// Resource 析构时会通过自身调试警告提示 GPU 状态未释放。
    void clear(QOpenGLFunctions_3_3_Core* gl = 0);

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
    ResourceId registerInternal(Resource* resource, ResourceOwnership ownership);

    /// 从 Manager 中解除一条 ResourceEntry。
    bool removeInternal(ResourceId id, QOpenGLFunctions_3_3_Core* gl);

    ResourceId allocateId();

private:
    std::map<ResourceId, ResourceEntry> m_resources; // 当前注册的所有 Resource。
    ResourceId m_nextId;                             // 下一次 ResourceId 分配起点。
};

#endif // RESOURCEMANAGER_H
