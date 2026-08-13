#include "Resource.h"

#include <QDebug>

Resource::Resource(const QString& name, ResourceType type, ResourceUpdatePolicy updatePolicy)
    : m_id(InvalidResourceId)
    , m_name(name)
    , m_type(type)
    , m_updatePolicy(updatePolicy)
    , m_dirtyState(ResourceClean)
    , m_initialized(false)
{
}

Resource::~Resource()
{
    if (m_initialized)
        qWarning() << "Resource destroyed while GPU state is still initialized:" << m_name;
}

/// Resource 基本信息

ResourceId Resource::id() const
{
    return m_id;
}

const QString& Resource::name() const
{
    return m_name;
}

ResourceType Resource::type() const
{
    return m_type;
}

ResourceUpdatePolicy Resource::updatePolicy() const
{
    return m_updatePolicy;
}

/// GPU / Dirty 状态

bool Resource::isInitialized() const
{
    return m_initialized;
}

ResourceDirtyState Resource::dirtyState() const
{
    return m_dirtyState;
}

void Resource::markPartialDirty()
{
    if (m_dirtyState != ResourceFullDirty)
        m_dirtyState = ResourcePartialDirty;
}

void Resource::markFullDirty()
{
    m_dirtyState = ResourceFullDirty;
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
        return false;

    m_initialized = true;
    m_dirtyState = ResourceClean;
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

    m_dirtyState = ResourceClean;
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

    m_dirtyState = ResourceClean;
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
    m_dirtyState = ResourceClean;
    return true;
}

/// GPU 实现

bool Resource::onPrepareSync()
{
    return true;
}

/// ResourceManager

void Resource::setId(ResourceId id)
{
    m_id = id;
}

/// 调试名称

const char* resourceTypeName(ResourceType type)
{
    switch (type)
    {
    case ResourceTypeMesh:
        return "Mesh";
    case ResourceTypeTexture:
        return "Texture";
    case ResourceTypeCurve:
        return "Curve";
    case ResourceTypeCoordinateSystem:
        return "CoordinateSystem";
    case ResourceTypeViewNavigation:
        return "ViewNavigation";
    case ResourceTypeGridPlane:
        return "GridPlane";
    }

    return "Unknown";
}

const char* resourceUpdatePolicyName(ResourceUpdatePolicy policy)
{
    switch (policy)
    {
    case ResourceUpdateStatic:
        return "Static";
    case ResourceUpdateDynamic:
        return "Dynamic";
    }

    return "Unknown";
}

const char* resourceDirtyStateName(ResourceDirtyState state)
{
    switch (state)
    {
    case ResourceClean:
        return "Clean";
    case ResourcePartialDirty:
        return "PartialDirty";
    case ResourceFullDirty:
        return "FullDirty";
    }

    return "Unknown";
}