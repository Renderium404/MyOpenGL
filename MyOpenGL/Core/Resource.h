#ifndef RESOURCE_H
#define RESOURCE_H

#include <QOpenGLFunctions_3_3_Core>
#include <QString>

typedef unsigned int ResourceId;

const ResourceId InvalidResourceId = 0;

/// Resource 语义类型。
enum ResourceType
{
    ResourceTypeMesh,
    ResourceTypeTexture,
    ResourceTypeCurve,
    ResourceTypeCoordinateSystem,
    ResourceTypeViewNavigation,
    ResourceTypeGridPlane
};

/// Resource CPU 数据的典型更新频率。
enum ResourceUpdatePolicy
{
    ResourceUpdateStatic,
    ResourceUpdateDynamic
};

/// Resource CPU 数据相对于 GPU Cache 的同步状态。
enum ResourceDirtyState
{
    ResourceClean,
    ResourcePartialDirty,
    ResourceFullDirty
};

/// Resource 调试名称。
const char* resourceTypeName(ResourceType type);
const char* resourceUpdatePolicyName(ResourceUpdatePolicy policy);
const char* resourceDirtyStateName(ResourceDirtyState state);

class ResourceManager;

/// GPU Resource 基类。
/// 统一管理 ResourceId、Dirty State 和 GPU 生命周期，但不规定具体 GPU 对象类型。
class Resource
{
public:
    virtual ~Resource();

    /// Resource 基本信息
    ResourceId id() const;
    const QString& name() const;
    ResourceType type() const;
    ResourceUpdatePolicy updatePolicy() const;

    /// GPU / Dirty 状态
    bool isInitialized() const;
    ResourceDirtyState dirtyState() const;
    void markPartialDirty(); // 标记部分 CPU 数据发生变化；不会覆盖已经存在的 FullDirty。
    void markFullDirty();    // 标记整个 Resource GPU Cache 需要重新同步。

    /// 同步准备
    bool prepareSync(); // 在 ResourceManager 判断 DirtyState 前刷新外部依赖状态。

    /// GPU 生命周期
    bool initializeGL(QOpenGLFunctions_3_3_Core* gl); // 初始化失败时自动调用 onReleaseGL() 回滚已经创建的部分 GPU 状态。
    bool updateFullGL(QOpenGLFunctions_3_3_Core* gl);
    bool updatePartialGL(QOpenGLFunctions_3_3_Core* gl);
    bool releaseGL(QOpenGLFunctions_3_3_Core* gl);

protected:
    Resource(const QString& name, ResourceType type, ResourceUpdatePolicy updatePolicy);

    /// GPU 实现
    virtual bool onPrepareSync(); // 默认无操作；外部数据 Resource 可在这里检查 Revision。
    virtual bool onInitializeGL(QOpenGLFunctions_3_3_Core* gl) = 0; // 失败时 Resource 基类会立即调用 onReleaseGL() 执行事务回滚。
    virtual bool onUpdateFullGL(QOpenGLFunctions_3_3_Core* gl) = 0;
    virtual bool onUpdatePartialGL(QOpenGLFunctions_3_3_Core* gl) = 0;
    virtual void onReleaseGL(QOpenGLFunctions_3_3_Core* gl) = 0; // 必须能够安全释放完整或部分创建的 GPU 状态。

private:
    friend class ResourceManager;

    void setId(ResourceId id);

private:
    ResourceId m_id;                         // ResourceManager 分配的唯一 ID。
    QString m_name;                          // Resource 调试名称。
    ResourceType m_type;                     // Resource 语义类型。
    ResourceUpdatePolicy m_updatePolicy;     // CPU 数据典型更新频率。
    ResourceDirtyState m_dirtyState;         // 当前 CPU 相对于 GPU Cache 的同步状态。
    bool m_initialized;                      // 当前 GPU 对象是否已经完整创建。
};

#endif // RESOURCE_H