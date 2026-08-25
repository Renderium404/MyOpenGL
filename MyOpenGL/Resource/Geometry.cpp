#include "Geometry.h"

Geometry::Geometry(const QString& name)
    : Resource(name, ResourceType::Geometry)
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
    case RenderType::Triangles:
        return "Triangles";
    case RenderType::Lines:
        return "Lines";
    case RenderType::LineStrip:
        return "LineStrip";
    }

    return "Unknown";
}

const char* bufferUsageName(BufferUsage usage)
{
    switch (usage)
    {
    case BufferUsage::Static:
        return "Static";
    case BufferUsage::Dynamic:
        return "Dynamic";
    }

    return "Unknown";
}
AttributeIterator Geometry::attributeBegin(GLuint location) const
{
    const std::shared_ptr<const GeometryAttributeIteratorAccessor> accessor = createAttributeIteratorAccessor(location);

    if (!accessor)
        return AttributeIterator();

    return AttributeIterator(accessor, 0);
}

AttributeIterator Geometry::attributeEnd(GLuint location) const
{
    const std::shared_ptr<const GeometryAttributeIteratorAccessor> accessor = createAttributeIteratorAccessor(location);

    if (!accessor)
        return AttributeIterator();

    return AttributeIterator(accessor, accessor->size());
}

IndexIterator Geometry::indexBegin() const
{
    const std::shared_ptr<const GeometryIndexIteratorAccessor> accessor = createIndexIteratorAccessor();

    if (!accessor)
        return IndexIterator();

    return IndexIterator(accessor, 0);
}

IndexIterator Geometry::indexEnd() const
{
    const std::shared_ptr<const GeometryIndexIteratorAccessor> accessor = createIndexIteratorAccessor();

    if (!accessor)
        return IndexIterator();

    return IndexIterator(accessor, accessor->size());
}

std::shared_ptr<const GeometryAttributeIteratorAccessor> Geometry::createAttributeIteratorAccessor(GLuint location) const
{
    Q_UNUSED(location);
    return std::shared_ptr<const GeometryAttributeIteratorAccessor>();
}

std::shared_ptr<const GeometryIndexIteratorAccessor> Geometry::createIndexIteratorAccessor() const
{
    return std::shared_ptr<const GeometryIndexIteratorAccessor>();
}