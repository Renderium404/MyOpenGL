#ifndef GEOMETRY_H
#define GEOMETRY_H

#include "MyOpenGL/Core/Resource.h"

/// 几何绘制类型。
enum RenderType
{
    Triangles, // 每三个 Index 组成一个独立三角形。
    Lines,     // 每两个 Index 组成一条独立线段。
    LineStrip  // 所有 Index 按顺序连接成一条连续折线。
};

/// 获取 RenderType 的调试名称。
const char* renderTypeName(RenderType type);

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
    /// 检查当前 Vertex Layout 是否满足指定 Shader Attribute 要求。
    virtual bool hasAttribute(GLuint location, GLint componentCount) const = 0;

    /// 绘制同步
    virtual bool prepareDrawGL(QOpenGLFunctions_3_3_Core* gl) const; // Draw 前准备外部 GPU 数据访问。
    virtual void finishDrawGL(QOpenGLFunctions_3_3_Core* gl) const;  // Draw 后结束外部 GPU 数据访问。

protected:
    Geometry(const QString& name, ResourceUpdatePolicy updatePolicy);
};

#endif // GEOMETRY_H
