#include "Resource.h"

#include <QDebug>

Resource::Resource(const QString& name, ResourceType type)
    : m_id(InvalidResourceId)
    , m_name(name)
    , m_type(type)
    , m_dirtyState(ResourceDirtyState::Clean)
    , m_initialized(false)
{
}

Resource::~Resource()
{
    if (m_initialized)
        qWarning() << "Resource destroyed while GPU state is still initialized:" << m_name;
}

/// Resource 基本信息

QString Resource::type() const
{
    return QString::fromLatin1(resourceTypeName(m_type));
}

/// Dirty 状态

void Resource::markPartialDirty()
{
    if (m_dirtyState != ResourceDirtyState::Full)
        m_dirtyState = ResourceDirtyState::Partial;
}

void Resource::markFullDirty()
{
    m_dirtyState = ResourceDirtyState::Full;
}

/// 同步准备

bool Resource::prepareSync()
{
    return onPrepareSync();
}

/// GPU 生命周期

bool Resource::initializeGL(QOpenGLFunctions_3_3_Core* gl)
{
    if (gl == 0)
    {
        qWarning() << "Resource initializeGL failed: OpenGL functions are null:" << m_name;
        return false;
    }

    if (m_initialized)
    {
        qWarning() << "Resource initializeGL failed: resource is already initialized:" << m_name;
        return false;
    }

    if (!onInitializeGL(gl))
    {
        // onInitializeGL() 可能已经成功创建了一部分 VAO / VBO / Texture 等 GPU Object。
        // Resource 此时还不能标记为 Initialized，因此必须立即调用 onReleaseGL() 执行事务回滚，
        // 否则 ResourceManager 后续会因为 isInitialized()==false 而无法发现这些半初始化 GPU 状态。
        onReleaseGL(gl);

        m_initialized = false;

        // GPU Cache 当前不存在，因此 CPU 数据仍然需要一次完整初始化。
        // 下次 syncResource() 会再次进入 initializeGL()，FullDirty 同时保持调试语义正确。
        m_dirtyState = ResourceDirtyState::Full;

        qWarning() << "Resource initializeGL failed: partial GPU state rolled back:" << m_name;
        return false;
    }

    m_initialized = true;
    m_dirtyState = ResourceDirtyState::Clean;
    return true;
}

bool Resource::updateFullGL(QOpenGLFunctions_3_3_Core* gl)
{
    if (gl == 0)
    {
        qWarning() << "Resource updateFullGL failed: OpenGL functions are null:" << m_name;
        return false;
    }

    if (!m_initialized)
    {
        qWarning() << "Resource updateFullGL failed: resource is not initialized:" << m_name;
        return false;
    }

    if (!onUpdateFullGL(gl))
        return false;

    m_dirtyState = ResourceDirtyState::Clean;
    return true;
}

bool Resource::updatePartialGL(QOpenGLFunctions_3_3_Core* gl)
{
    if (gl == 0)
    {
        qWarning() << "Resource updatePartialGL failed: OpenGL functions are null:" << m_name;
        return false;
    }

    if (!m_initialized)
    {
        qWarning() << "Resource updatePartialGL failed: resource is not initialized:" << m_name;
        return false;
    }

    if (!onUpdatePartialGL(gl))
        return false;

    m_dirtyState = ResourceDirtyState::Clean;
    return true;
}

bool Resource::releaseGL(QOpenGLFunctions_3_3_Core* gl)
{
    if (!m_initialized)
        return true;

    if (gl == 0)
    {
        qWarning() << "Resource releaseGL failed: OpenGL functions are null:" << m_name;
        return false;
    }

    onReleaseGL(gl);

    m_initialized = false;
    m_dirtyState = ResourceDirtyState::Clean;
    return true;
}

/// GPU 实现

bool Resource::onPrepareSync()
{
    return true;
}

/// 调试名称

const char* resourceTypeName(ResourceType type)
{
    switch (type)
    {
    case ResourceType::Geometry:
        return "Geometry";
    case ResourceType::Texture:
        return "Texture";
    }

    return "Unknown";
}

const char* resourceDirtyStateName(ResourceDirtyState state)
{
    switch (state)
    {
    case ResourceDirtyState::Clean:
        return "Clean";
    case ResourceDirtyState::Partial:
        return "Partial";
    case ResourceDirtyState::Full:
        return "Full";
    }

    return "Unknown";
}
