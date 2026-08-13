#include "RenderContext.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QSurfaceFormat>

RenderContext::RenderContext()
    : m_initialized(false)
{
}

/// Context 状态

bool RenderContext::initialize()
{
    QOpenGLContext* context = QOpenGLContext::currentContext();

    if (context == 0)
    {
        qWarning() << "RenderContext initialize failed: no current OpenGL context.";
        return false;
    }

    const QSurfaceFormat format = context->format();

    if (format.majorVersion() < 3 || (format.majorVersion() == 3 && format.minorVersion() < 3))
    {
        qWarning() << "RenderContext initialize failed: OpenGL 3.3 or newer is required.";
        return false;
    }

    if (format.profile() != QSurfaceFormat::CoreProfile)
    {
        qWarning() << "RenderContext initialize failed: OpenGL Core Profile is required.";
        return false;
    }

    if (!m_gl.initializeOpenGLFunctions())
    {
        qWarning() << "RenderContext initialize failed: OpenGL 3.3 Core functions are unavailable.";
        return false;
    }

    m_initialized = true;
    return true;
}

bool RenderContext::isInitialized() const
{
    return m_initialized;
}

QOpenGLFunctions_3_3_Core* RenderContext::gl()
{
    if (!m_initialized)
    {
        qWarning() << "RenderContext gl failed: context functions are not initialized.";
        return 0;
    }

    return &m_gl;
}

const QOpenGLFunctions_3_3_Core* RenderContext::gl() const
{
    if (!m_initialized)
    {
        qWarning() << "RenderContext gl failed: context functions are not initialized.";
        return 0;
    }

    return &m_gl;
}