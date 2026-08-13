#ifndef RENDERABLEMESH_H
#define RENDERABLEMESH_H

#include <QOpenGLFunctions_3_3_Core>
#include <QString>

/// Mesh 图元类型。
/// 描述 Renderer 应如何解释当前 MeshResource 的索引数据。
enum MeshPrimitiveType
{
    MeshPrimitiveTriangles,    // 每三个索引组成一个独立三角形。
    MeshPrimitiveLines,        // 每两个索引组成一条独立线段。
    MeshPrimitiveLineStrip     // 相邻索引连续组成折线。
};

/// 获取 Mesh 图元类型的调试名称。
const char* meshPrimitiveTypeName(MeshPrimitiveType type);

/// Renderer 所需的最小 Mesh 接口。
/// 不规定 CPU 网格如何存储，只暴露已经建立好的 GPU Mesh 状态。
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
};

#endif // RENDERABLEMESH_H