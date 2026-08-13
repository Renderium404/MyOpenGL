#ifndef RENDERCONTEXT_H
#define RENDERCONTEXT_H

#include <QOpenGLFunctions_3_3_Core>

/// 渲染上下文访问器。
/// 不拥有 QOpenGLContext，只在当前有效 Context 上初始化并提供 OpenGL 3.3 Core 函数入口。
class RenderContext
{
public:
    RenderContext();

    /// Context 状态
    bool initialize();                           // 在当前 OpenGL Context 上初始化 3.3 Core 函数入口。
    bool isInitialized() const;
    QOpenGLFunctions_3_3_Core* gl();             // 获取可用于资源同步和渲染的 OpenGL 函数接口。
    const QOpenGLFunctions_3_3_Core* gl() const;

private:
    QOpenGLFunctions_3_3_Core m_gl;              // 当前 Context 对应的 OpenGL 3.3 Core 函数接口。
    bool m_initialized;                          // OpenGL 函数入口是否已经初始化成功。
};

#endif // RENDERCONTEXT_H