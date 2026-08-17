#include "RenderableObject.h"

const char* renderTypeName(RenderType type)
{
    switch (type)
    {
    case Triangles:
        return "Triangles";
    case Lines:
        return "Lines";
    case LineStrip:
        return "LineStrip";
    }

    return "Unknown";
}

/// 绘制同步

bool RenderableObject::prepareDrawGL(QOpenGLFunctions_3_3_Core* gl) const
{
    Q_UNUSED(gl);

    // MyOpenGL 自有 Mesh 和 External CPU Mesh 的 GPU Cache 都由当前 Renderer Context 使用，不需要额外的 Draw Synchronization。
    return true;
}

void RenderableObject::finishDrawGL(QOpenGLFunctions_3_3_Core* gl) const
{
    Q_UNUSED(gl);

    // 默认 Mesh 不需要在 Draw 后发布额外的 GPU Synchronization。
}