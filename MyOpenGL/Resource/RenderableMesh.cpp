#include "RenderableMesh.h"

const char* meshPrimitiveTypeName(MeshPrimitiveType type)
{
    switch (type)
    {
    case MeshPrimitiveTriangles:
        return "Triangles";
    case MeshPrimitiveLines:
        return "Lines";
    case MeshPrimitiveLineStrip:
        return "LineStrip";
    }

    return "Unknown";
}

/// 绘制同步

bool RenderableMesh::prepareDrawGL(QOpenGLFunctions_3_3_Core* gl) const
{
    Q_UNUSED(gl);

    // MyOpenGL 自有 Mesh 和 External CPU Mesh 的 GPU Cache 都由当前 Renderer Context 使用，不需要额外的 Draw Synchronization。
    return true;
}

void RenderableMesh::finishDrawGL(QOpenGLFunctions_3_3_Core* gl) const
{
    Q_UNUSED(gl);

    // 默认 Mesh 不需要在 Draw 后发布额外的 GPU Synchronization。
}