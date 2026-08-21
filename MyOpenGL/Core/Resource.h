#ifndef RESOURCE_H
#define RESOURCE_H

#include <QOpenGLFunctions_3_3_Core>
#include <QString>

class ResourceManager;

/// Resource 唯一标识类型，由 ResourceManager 统一分配。
typedef unsigned int ResourceId;

/// 无效 Resource ID。
const ResourceId InvalidResourceId = 0;

/// Resource 系统级资源类别。
enum class ResourceType
{
    Geometry, // 可直接提供几何绘制数据的资源。
    Texture   // 提供 Shader 采样数据的纹理资源。
};

/// Resource CPU 数据相对于 GPU Cache 的同步状态。
enum class ResourceDirtyState
{
    Clean,   // CPU 数据与 GPU Cache 当前一致，不需要同步。
    Partial, // 只有部分 CPU 数据发生变化，可以执行局部 GPU 更新。
    Full     // Resource 整体数据发生变化，需要执行完整 GPU 更新。
};

/// Resource 调试名称。
const char* resourceTypeName(ResourceType type);
const char* resourceDirtyStateName(ResourceDirtyState state);

/// GPU Resource 基类。
/// 统一管理 ResourceId、Dirty State 和 GPU 生命周期，不规定具体资源数据和绘制方式。
class Resource
{
public:
    virtual ~Resource();

    /// Resource 基本信息
    ResourceId id() const { return m_id; }
    const QString& name() const { return m_name; }

    QString type() const;
    ResourceType resourceType() const { return m_type; }

    /// GPU / Dirty 状态
    bool isInitialized() const { return m_initialized; }
    ResourceDirtyState dirtyState() const { return m_dirtyState; }

protected:
    Resource(const QString& name, ResourceType type);

    /// Dirty 状态
    void markPartialDirty(); // 标记部分 CPU 数据发生变化；不会覆盖已经存在的 Full Dirty。
    void markFullDirty();    // 标记整个 Resource GPU Cache 需要重新同步。

    /// GPU 实现
    virtual bool onPrepareSync(); // 默认无操作；外部数据 Resource 可在这里检查 Revision。
    virtual bool onInitializeGL(QOpenGLFunctions_3_3_Core* gl) = 0; // 失败时 Resource 基类会立即调用 onReleaseGL() 执行事务回滚。
    virtual bool onUpdateFullGL(QOpenGLFunctions_3_3_Core* gl) = 0;
    virtual bool onUpdatePartialGL(QOpenGLFunctions_3_3_Core* gl) = 0;
    virtual void onReleaseGL(QOpenGLFunctions_3_3_Core* gl) = 0; // 必须能够安全释放完整或部分创建的 GPU 状态。

private:
    friend class ResourceManager;

    /// ResourceManager 内部接口
    void setId(ResourceId id) { m_id = id; }

    /// 同步准备
    bool prepareSync(); // 在 ResourceManager 判断 DirtyState 前刷新外部依赖状态。

    /// GPU 生命周期
    bool initializeGL(QOpenGLFunctions_3_3_Core* gl); // 初始化失败时自动调用 onReleaseGL() 回滚已经创建的部分 GPU 状态。
    bool updateFullGL(QOpenGLFunctions_3_3_Core* gl);
    bool updatePartialGL(QOpenGLFunctions_3_3_Core* gl);
    bool releaseGL(QOpenGLFunctions_3_3_Core* gl);

private:
    ResourceId m_id;                 // ResourceManager 分配的唯一 ID。
    QString m_name;                  // Resource 调试名称。
    ResourceType m_type;             // 当前系统级资源类别。
    ResourceDirtyState m_dirtyState; // 当前 CPU 相对于 GPU Cache 的同步状态。
    bool m_initialized;              // 当前 GPU 对象是否已经完整创建。
};

#endif // RESOURCE_H
