#ifndef RENDERABLEMESH_H
#define RENDERABLEMESH_H

#include <QOpenGLFunctions_3_3_Core>
#include <QString>

/// Mesh 图元类型。
/// 描述 Renderer 应使用哪种 OpenGL Primitive 绘制当前网格。
enum MeshPrimitiveType
{
    MeshPrimitiveTriangles,
    MeshPrimitiveLines,
    MeshPrimitiveLineStrip
};

/// 获取 Mesh 图元类型的调试名称。
const char* meshPrimitiveTypeName(MeshPrimitiveType type);

/// Renderer 所需的最小网格接口。
/// 不规定 CPU 网格如何存储，只暴露已经同步到 GPU 的绘制状态。
class RenderableMesh
{
public:
    virtual ~RenderableMesh()
    {
    }

    /// 调试状态
    virtual const QString& renderMeshName() const = 0;
    virtual bool renderMeshInitialized() const = 0;

    /// GPU 绘制状态
    virtual GLuint vao() const = 0;
    virtual int indexCount() const = 0;
    virtual GLenum indexType() const = 0;
    virtual MeshPrimitiveType primitiveType() const = 0;

    /// Vertex Layout
    virtual bool hasAttribute(GLuint location, GLint componentCount) const = 0;

    /// 绘制同步
    virtual bool prepareDrawGL(QOpenGLFunctions_3_3_Core* gl) const; // Draw 前准备数据可见性；普通 Mesh 默认无需额外同步。
    virtual void finishDrawGL(QOpenGLFunctions_3_3_Core* gl) const;  // Draw 提交后结束本次读取；普通 Mesh 默认无需额外同步。
};

#endif // RENDERABLEMESH_H