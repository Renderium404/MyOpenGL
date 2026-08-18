#include "Geometry.h"

Geometry::Geometry(const QString& name, ResourceUpdatePolicy updatePolicy)
    : Resource(name, ResourceTypeGeometry, updatePolicy)
{
}

Geometry::~Geometry()
{
}

/// 绘制同步

bool Geometry::prepareDrawGL(QOpenGLFunctions_3_3_Core* gl) const
{
    Q_UNUSED(gl);

    // 普通 Geometry Resource 由当前 Renderer Context 直接访问，不需要额外 Draw Synchronization。
    return true;
}

void Geometry::finishDrawGL(QOpenGLFunctions_3_3_Core* gl) const
{
    Q_UNUSED(gl);

    // 默认 Geometry Resource 不需要在 Draw 后发布额外 GPU Synchronization。
}

/// 调试名称

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
