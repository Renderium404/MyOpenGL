#ifndef MESHRESOURCE_H
#define MESHRESOURCE_H

#include "Core/Resource.h"
#include "Resource/RenderableObject.h"

#include <QOpenGLFunctions_3_3_Core>

#include <cstddef>
#include <vector>

/// Owned Mesh 的 Vertex Attribute 描述。
/// valueOffset 使用 GLfloat 数量作为单位，而不是 Byte。
struct MeshVertexAttribute
{
    GLuint location;       // Shader Attribute Location。
    GLint componentCount;  // Attribute 分量数量，例如 vec3 = 3。
    int valueOffset;       // Attribute 在一个 Vertex 中的 GLfloat 偏移。
};

/// MyOpenGL 自有 CPU 数据的网格资源。
/// 使用一个 Interleaved VBO 和一个 EBO，适合辅助几何、Curve 和库内生成的小型 Mesh。
class MeshResource : public Resource, public RenderableObject
{
public:
    MeshResource(const QString& name, ResourceUpdatePolicy updatePolicy = ResourceUpdateDynamic, RenderType renderType = Triangles);
    ~MeshResource() override;

    /// Mesh 基本信息
    RenderType renderType() const override;

    /// 顶点布局
    void setVertexLayout(int valuesPerVertex, const std::vector<MeshVertexAttribute>& attributes);
    int valuesPerVertex() const;
    const std::vector<MeshVertexAttribute>& attributes() const;

    /// CPU 数据
    void setVertexData(const std::vector<GLfloat>& vertices);
    void setIndexData(const std::vector<GLuint>& indices);
    const std::vector<GLfloat>& vertexData() const;
    const std::vector<GLuint>& indexData() const;
    int vertexCount() const;
    int indexCount() const override;

    /// 增量更新
    bool updateVertexData(int valueOffset, const GLfloat* data, int valueCount);
    bool updateIndexData(int indexOffset, const GLuint* data, int indexCount);

    /// GPU 对象
    GLuint vao() const override;
    GLuint vbo() const;
    GLuint ebo() const;

    /// Renderer 接口
    const QString& objectName() const override;
    bool objectInitialized() const override;
    GLenum indexType() const override;
    bool hasAttribute(GLuint location, GLint componentCount) const override;

protected:
    MeshResource(const QString& name, ResourceType type, ResourceUpdatePolicy updatePolicy, RenderType renderType);

    /// Resource GPU 实现
    bool onInitializeGL(QOpenGLFunctions_3_3_Core* gl) override;
    bool onUpdateFullGL(QOpenGLFunctions_3_3_Core* gl) override;
    bool onUpdatePartialGL(QOpenGLFunctions_3_3_Core* gl) override;
    void onReleaseGL(QOpenGLFunctions_3_3_Core* gl) override;

private:
    struct DirtyRange
    {
        std::size_t byteOffset;
        std::size_t byteSize;
    };

    /// 内部辅助
    bool validateData() const;
    void configureVertexAttributes(QOpenGLFunctions_3_3_Core* gl);
    void releaseGPUObjects(QOpenGLFunctions_3_3_Core* gl);
    GLenum bufferUsage() const;

private:
    GLuint m_vao;                                      // 当前 Mesh VAO。
    GLuint m_vbo;                                      // 当前 Interleaved Vertex Buffer。
    GLuint m_ebo;                                      // 当前 Index Buffer。
    RenderType m_primitiveType;                 // 当前网格绘制图元。
    int m_valuesPerVertex;                             // 一个 Vertex 包含的 GLfloat 数量。
    std::vector<MeshVertexAttribute> m_attributes;     // 当前 Vertex Layout。
    std::vector<GLuint> m_enabledAttributeLocations;   // 当前 VAO 已启用的 Attribute Location。
    std::vector<GLfloat> m_vertices;                   // MyOpenGL 拥有的完整 CPU Vertex 数据。
    std::vector<GLuint> m_indices;                     // MyOpenGL 拥有的完整 CPU Index 数据。
    std::vector<DirtyRange> m_dirtyVertexRanges;       // 等待增量同步的 Vertex 字节区间。
    std::vector<DirtyRange> m_dirtyIndexRanges;        // 等待增量同步的 Index 字节区间。
};

#endif // MESHRESOURCE_H