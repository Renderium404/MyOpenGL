#ifndef BUFFERGEOMETRY_H
#define BUFFERGEOMETRY_H

#include "Geometry.h"

#include <QOpenGLFunctions_3_3_Core>

#include <cstddef>
#include <vector>

/// Owned Geometry 的 Vertex Attribute 描述。
/// valueOffset 使用 GLfloat 数量作为单位，而不是 Byte。
struct GeometryVertexAttribute
{
    GLuint location;      // MyOpenGL 统一 Vertex Attribute Location。
    GLint componentCount; // Attribute 分量数量，例如 vec3 = 3。
    int valueOffset;      // Attribute 在一个 Vertex 中的 GLfloat 偏移。
};

/// MyOpenGL 自己拥有 CPU 数据和 GPU Buffer 的几何资源。
/// 使用一个 Interleaved VBO 和一个 EBO，支持 Triangles、Lines 和 LineStrip。
class BufferGeometry : public Geometry
{
public:
    BufferGeometry(const QString& name, BufferUsage usage = BufferUsage::Dynamic, RenderType renderType = RenderType::Triangles);
    ~BufferGeometry() override;

    /// Geometry 基本信息
    RenderType renderType() const override;
    BufferUsage bufferUsage() const { return m_usage; }

    /// 顶点布局
    void setVertexLayout(int valuesPerVertex, const std::vector<GeometryVertexAttribute>& attributes);
    int valuesPerVertex() const;
    const std::vector<GeometryVertexAttribute>& attributes() const;

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
    GLenum indexType() const override;
    bool hasAttribute(GLuint location, GLint componentCount) const override;

protected:
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
    GLenum glBufferUsage() const;
    std::shared_ptr<const GeometryAttributeIteratorAccessor> createAttributeIteratorAccessor(GLuint location) const override;
    std::shared_ptr<const GeometryIndexIteratorAccessor> createIndexIteratorAccessor() const override;
private:
    GLuint m_vao;                                            // 当前 Geometry VAO。
    GLuint m_vbo;                                            // 当前 Interleaved Vertex Buffer。
    GLuint m_ebo;                                            // 当前 Index Buffer。
    BufferUsage m_usage;                                     // 当前 GPU Buffer 使用策略。
    RenderType m_renderType;                                 // 当前几何绘制类型。
    int m_valuesPerVertex;                                   // 一个 Vertex 包含的 GLfloat 数量。
    std::vector<GeometryVertexAttribute> m_attributes;       // 当前 Vertex Layout。
    std::vector<GLuint> m_enabledAttributeLocations;         // 当前 VAO 已启用的 Attribute Location。
    std::vector<GLfloat> m_vertices;                         // MyOpenGL 拥有的完整 CPU Vertex 数据。
    std::vector<GLuint> m_indices;                           // MyOpenGL 拥有的完整 CPU Index 数据。
    std::vector<DirtyRange> m_dirtyVertexRanges;             // 等待增量同步的 Vertex 字节区间。
    std::vector<DirtyRange> m_dirtyIndexRanges;              // 等待增量同步的 Index 字节区间。
};

#endif // BUFFERGEOMETRY_H
