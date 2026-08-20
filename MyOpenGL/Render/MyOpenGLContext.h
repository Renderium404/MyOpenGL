#ifndef MYOPENGLCONTEXT_H
#define MYOPENGLCONTEXT_H

#include <QOpenGLFunctions_3_3_Core>

/// MyOpenGL 对当前 Qt OpenGL Context 的访问封装。
/// 不拥有 QOpenGLContext，只负责验证 OpenGL 环境并提供 OpenGL 3.3 Core 函数入口。
class MyOpenGLContext
{
public:
    MyOpenGLContext();

    /// Context 状态
    bool initialize();
    bool isInitialized() const;

    /// OpenGL API
    QOpenGLFunctions_3_3_Core* gl();
    const QOpenGLFunctions_3_3_Core* gl() const;

private:
    QOpenGLFunctions_3_3_Core m_gl;
    bool m_initialized;
};

#endif // MYOPENGLCONTEXT_H