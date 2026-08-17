#ifndef RENDERABLEMESH_H
#define RENDERABLEMESH_H

#include <QOpenGLFunctions_3_3_Core>
#include <QString>

/// 渲染图元类型。
enum RenderType
{
    Triangles, // 每三个 Index 组成一个独立三角形。
    Lines,     // 每两个 Index 组成一条独立线段。
    LineStrip  // 所有 Index 按顺序连接成连续折线。
};

/// 获取 RenderType 的调试名称。
const char* renderTypeName(RenderType type);

/// Renderer 可以直接绘制的最小对象接口。
/// 只描述 GPU 绘制状态，不限定具体几何类型和数据来源。
class RenderableObject
{
public:
    virtual ~RenderableObject()
    {
    }

    /// 基本状态
    virtual const QString& objectName() const = 0;
    virtual bool objectInitialized() const = 0;

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
};

#endif // RENDERABLEMESH_H