#ifndef GEOMETRY_H
#define GEOMETRY_H

#include "MyOpenGL/Core/Resource.h"
#include "GeometryIterator.h"
/// 几何绘制类型。
enum class RenderType
{
    Triangles, // 每三个 Index 组成一个独立三角形。
    Lines,     // 每两个 Index 组成一条独立线段。
    LineStrip  // 所有 Index 按顺序连接成一条连续折线。
};

/// GPU Buffer 数据的典型更新频率。
enum class BufferUsage
{
    Static,  // Buffer 数据通常创建后很少变化，对应 GL_STATIC_DRAW。
    Dynamic  // Buffer 数据会持续或频繁变化，对应 GL_DYNAMIC_DRAW。
};

/// MyOpenGL 统一 Vertex Attribute Location。
/// 外部 Geometry Adapter 必须将外部数据语义映射到这些固定 Location。
namespace GeometryAttribute
{
const GLuint Position = 0; // Vertex Position。
const GLuint Normal = 1;   // Vertex Normal。
const GLuint TexCoord = 2; // Vertex Texture Coordinate。
const GLuint Color = 3;    // Vertex Color。
}

/// 获取 Geometry 调试名称。
const char* renderTypeName(RenderType type);
const char* bufferUsageName(BufferUsage usage);

/// 可直接提供几何绘制数据的 Resource 基类。
/// 统一描述 Renderer 执行 Indexed Draw 所需要的 GPU 状态。
class Geometry : public Resource
{
public:
    virtual ~Geometry();

    /// GPU 绘制状态
    virtual GLuint vao() const = 0;
    virtual int indexCount() const = 0;
    virtual GLenum indexType() const = 0;
    virtual RenderType renderType() const = 0;

    /// Vertex Layout
    /// 检查当前 Vertex Layout 是否满足指定 MyOpenGL Attribute Location 和分量数量。
    virtual bool hasAttribute(GLuint location, GLint componentCount) const = 0;

    /// 绘制同步
    virtual bool prepareDrawGL(QOpenGLFunctions_3_3_Core* gl) const; // Draw 前准备外部 GPU 数据访问。
    virtual void finishDrawGL(QOpenGLFunctions_3_3_Core* gl) const;  // Draw 后结束外部 GPU 数据访问。
    ///数据访问
    AttributeIterator attributeBegin(GLuint location) const;
    AttributeIterator attributeEnd(GLuint location) const;
    ///索引访问
    IndexIterator indexBegin() const;
    IndexIterator indexEnd() const;
protected:
    explicit Geometry(const QString& name);
    virtual std::shared_ptr<const GeometryAttributeIteratorAccessor> createAttributeIteratorAccessor(GLuint location) const;
    virtual std::shared_ptr<const GeometryIndexIteratorAccessor> createIndexIteratorAccessor() const;
};

#endif // GEOMETRY_H
