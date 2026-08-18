#include "BufferGeometry.h"

#include <QDebug>

#include <algorithm>

BufferGeometry::BufferGeometry(const QString& name, ResourceUpdatePolicy updatePolicy, RenderType renderType)
    : Geometry(name, updatePolicy)
    , m_vao(0)
    , m_vbo(0)
    , m_ebo(0)
    , m_renderType(renderType)
    , m_valuesPerVertex(0)
{
}

BufferGeometry::~BufferGeometry()
{
}

/// Geometry 基本信息

RenderType BufferGeometry::renderType() const
{
    return m_renderType;
}

/// 顶点布局

void BufferGeometry::setVertexLayout(int valuesPerVertex, const std::vector<GeometryVertexAttribute>& attributes)
{
    if (valuesPerVertex <= 0)
    {
        qWarning() << "BufferGeometry setVertexLayout failed: valuesPerVertex must be greater than zero:" << name();
        return;
    }

    if (attributes.empty())
    {
        qWarning() << "BufferGeometry setVertexLayout failed: attributes cannot be empty:" << name();
        return;
    }

    m_valuesPerVertex = valuesPerVertex;
    m_attributes = attributes;
    m_dirtyVertexRanges.clear();
    markFullDirty();
}

int BufferGeometry::valuesPerVertex() const
{
    return m_valuesPerVertex;
}

const std::vector<GeometryVertexAttribute>& BufferGeometry::attributes() const
{
    return m_attributes;
}

/// CPU 数据

void BufferGeometry::setVertexData(const std::vector<GLfloat>& vertices)
{
    m_vertices = vertices;
    m_dirtyVertexRanges.clear();
    markFullDirty();
}

void BufferGeometry::setIndexData(const std::vector<GLuint>& indices)
{
    m_indices = indices;
    m_dirtyIndexRanges.clear();
    markFullDirty();
}

const std::vector<GLfloat>& BufferGeometry::vertexData() const
{
    return m_vertices;
}

const std::vector<GLuint>& BufferGeometry::indexData() const
{
    return m_indices;
}

int BufferGeometry::vertexCount() const
{
    if (m_valuesPerVertex <= 0)
        return 0;

    return static_cast<int>(m_vertices.size() / static_cast<std::size_t>(m_valuesPerVertex));
}

int BufferGeometry::indexCount() const
{
    return static_cast<int>(m_indices.size());
}

/// 增量更新

bool BufferGeometry::updateVertexData(int valueOffset, const GLfloat* data, int valueCount)
{
    if (data == 0 || valueCount <= 0)
    {
        qWarning() << "BufferGeometry updateVertexData failed: update data is invalid:" << name();
        return false;
    }

    if (valueOffset < 0 || static_cast<std::size_t>(valueOffset + valueCount) > m_vertices.size())
    {
        qWarning() << "BufferGeometry updateVertexData failed: update range exceeds vertex data:" << name();
        return false;
    }

    std::copy(data, data + valueCount, m_vertices.begin() + valueOffset);

    DirtyRange range;
    range.byteOffset = static_cast<std::size_t>(valueOffset) * sizeof(GLfloat);
    range.byteSize = static_cast<std::size_t>(valueCount) * sizeof(GLfloat);
    m_dirtyVertexRanges.push_back(range);

    markPartialDirty();
    return true;
}

bool BufferGeometry::updateIndexData(int indexOffset, const GLuint* data, int indexCount)
{
    if (data == 0 || indexCount <= 0)
    {
        qWarning() << "BufferGeometry updateIndexData failed: update data is invalid:" << name();
        return false;
    }

    if (indexOffset < 0 || static_cast<std::size_t>(indexOffset + indexCount) > m_indices.size())
    {
        qWarning() << "BufferGeometry updateIndexData failed: update range exceeds index data:" << name();
        return false;
    }

    std::copy(data, data + indexCount, m_indices.begin() + indexOffset);

    DirtyRange range;
    range.byteOffset = static_cast<std::size_t>(indexOffset) * sizeof(GLuint);
    range.byteSize = static_cast<std::size_t>(indexCount) * sizeof(GLuint);
    m_dirtyIndexRanges.push_back(range);

    markPartialDirty();
    return true;
}

/// GPU 对象

GLuint BufferGeometry::vao() const
{
    return m_vao;
}

GLuint BufferGeometry::vbo() const
{
    return m_vbo;
}

GLuint BufferGeometry::ebo() const
{
    return m_ebo;
}

/// Renderer 接口

GLenum BufferGeometry::indexType() const
{
    return GL_UNSIGNED_INT;
}

bool BufferGeometry::hasAttribute(GLuint location, GLint componentCount) const
{
    for (std::size_t i = 0; i < m_attributes.size(); ++i)
    {
        if (m_attributes[i].location == location && m_attributes[i].componentCount == componentCount)
            return true;
    }

    return false;
}

/// Resource GPU 实现

bool BufferGeometry::onInitializeGL(QOpenGLFunctions_3_3_Core* gl)
{
    if (!validateData())
        return false;

    gl->glGenVertexArrays(1, &m_vao);
    gl->glGenBuffers(1, &m_vbo);
    gl->glGenBuffers(1, &m_ebo);

    if (m_vao == 0 || m_vbo == 0 || m_ebo == 0)
    {
        qWarning() << "BufferGeometry initialize failed: OpenGL object creation failed:" << name();
        releaseGPUObjects(gl);
        return false;
    }

    gl->glBindVertexArray(m_vao);

    gl->glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    gl->glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_vertices.size() * sizeof(GLfloat)), &m_vertices[0], bufferUsage());

    gl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    gl->glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_indices.size() * sizeof(GLuint)), &m_indices[0], bufferUsage());

    configureVertexAttributes(gl);

    gl->glBindBuffer(GL_ARRAY_BUFFER, 0);

    // EBO Binding 属于 VAO 状态，因此先解绑 VAO，不能在当前 VAO 上将 EBO 解绑为 0。
    gl->glBindVertexArray(0);

    m_dirtyVertexRanges.clear();
    m_dirtyIndexRanges.clear();
    return true;
}

bool BufferGeometry::onUpdateFullGL(QOpenGLFunctions_3_3_Core* gl)
{
    if (!validateData())
        return false;

    gl->glBindVertexArray(m_vao);

    gl->glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    gl->glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_vertices.size() * sizeof(GLfloat)), &m_vertices[0], bufferUsage());

    gl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    gl->glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_indices.size() * sizeof(GLuint)), &m_indices[0], bufferUsage());

    // Layout 可能在 Full Update 前发生变化，因此重新配置当前 VAO Attribute。
    configureVertexAttributes(gl);

    gl->glBindBuffer(GL_ARRAY_BUFFER, 0);
    gl->glBindVertexArray(0);

    m_dirtyVertexRanges.clear();
    m_dirtyIndexRanges.clear();
    return true;
}

bool BufferGeometry::onUpdatePartialGL(QOpenGLFunctions_3_3_Core* gl)
{
    if (!validateData())
        return false;

    if (!m_dirtyVertexRanges.empty())
    {
        const char* vertexBytes = reinterpret_cast<const char*>(&m_vertices[0]);

        gl->glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

        for (std::size_t i = 0; i < m_dirtyVertexRanges.size(); ++i)
        {
            const DirtyRange& range = m_dirtyVertexRanges[i];
            gl->glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(range.byteOffset), static_cast<GLsizeiptr>(range.byteSize), vertexBytes + range.byteOffset);
        }

        gl->glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    if (!m_dirtyIndexRanges.empty())
    {
        const char* indexBytes = reinterpret_cast<const char*>(&m_indices[0]);

        // GL_ELEMENT_ARRAY_BUFFER 属于 VAO 状态；绑定当前 Geometry VAO 后即可访问它保存的 EBO。
        gl->glBindVertexArray(m_vao);

        for (std::size_t i = 0; i < m_dirtyIndexRanges.size(); ++i)
        {
            const DirtyRange& range = m_dirtyIndexRanges[i];
            gl->glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLintptr>(range.byteOffset), static_cast<GLsizeiptr>(range.byteSize), indexBytes + range.byteOffset);
        }

        gl->glBindVertexArray(0);
    }

    m_dirtyVertexRanges.clear();
    m_dirtyIndexRanges.clear();
    return true;
}

void BufferGeometry::onReleaseGL(QOpenGLFunctions_3_3_Core* gl)
{
    releaseGPUObjects(gl);
    m_dirtyVertexRanges.clear();
    m_dirtyIndexRanges.clear();
}

/// 内部辅助

bool BufferGeometry::validateData() const
{
    if (m_valuesPerVertex <= 0)
    {
        qWarning() << "BufferGeometry validation failed: vertex layout is not configured:" << name();
        return false;
    }

    if (m_attributes.empty())
    {
        qWarning() << "BufferGeometry validation failed: vertex attributes are empty:" << name();
        return false;
    }

    if (m_vertices.empty())
    {
        qWarning() << "BufferGeometry validation failed: vertex data is empty:" << name();
        return false;
    }

    if (m_indices.empty())
    {
        qWarning() << "BufferGeometry validation failed: index data is empty:" << name();
        return false;
    }

    if (m_vertices.size() % static_cast<std::size_t>(m_valuesPerVertex) != 0)
    {
        qWarning() << "BufferGeometry validation failed: vertex data does not match vertex layout:" << name();
        return false;
    }

    const int geometryVertexCount = vertexCount();

    for (std::size_t i = 0; i < m_indices.size(); ++i)
    {
        if (m_indices[i] >= static_cast<GLuint>(geometryVertexCount))
        {
            qWarning() << "BufferGeometry validation failed: index exceeds vertex count:" << m_indices[i] << name();
            return false;
        }
    }

    if (m_renderType == Triangles && m_indices.size() % 3 != 0)
    {
        qWarning() << "BufferGeometry validation failed: triangle index count must be divisible by 3:" << name();
        return false;
    }

    if (m_renderType == Lines && m_indices.size() % 2 != 0)
    {
        qWarning() << "BufferGeometry validation failed: line index count must be divisible by 2:" << name();
        return false;
    }

    if (m_renderType == LineStrip && m_indices.size() < 2)
    {
        qWarning() << "BufferGeometry validation failed: line strip requires at least 2 indices:" << name();
        return false;
    }

    for (std::size_t i = 0; i < m_attributes.size(); ++i)
    {
        const GeometryVertexAttribute& attribute = m_attributes[i];

        if (attribute.componentCount <= 0 || attribute.componentCount > 4)
        {
            qWarning() << "BufferGeometry validation failed: attribute componentCount must be between 1 and 4:" << name();
            return false;
        }

        if (attribute.valueOffset < 0 || attribute.valueOffset + attribute.componentCount > m_valuesPerVertex)
        {
            qWarning() << "BufferGeometry validation failed: attribute exceeds vertex layout:" << name();
            return false;
        }

        for (std::size_t j = i + 1; j < m_attributes.size(); ++j)
        {
            if (attribute.location == m_attributes[j].location)
            {
                qWarning() << "BufferGeometry validation failed: duplicate attribute location:" << attribute.location << name();
                return false;
            }
        }
    }

    return true;
}

void BufferGeometry::configureVertexAttributes(QOpenGLFunctions_3_3_Core* gl)
{
    for (std::size_t i = 0; i < m_enabledAttributeLocations.size(); ++i)
        gl->glDisableVertexAttribArray(m_enabledAttributeLocations[i]);

    m_enabledAttributeLocations.clear();

    const GLsizei stride = static_cast<GLsizei>(m_valuesPerVertex * sizeof(GLfloat));

    for (std::size_t i = 0; i < m_attributes.size(); ++i)
    {
        const GeometryVertexAttribute& attribute = m_attributes[i];
        const std::size_t byteOffset = static_cast<std::size_t>(attribute.valueOffset) * sizeof(GLfloat);

        gl->glVertexAttribPointer(attribute.location, attribute.componentCount, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(byteOffset));
        gl->glEnableVertexAttribArray(attribute.location);
        m_enabledAttributeLocations.push_back(attribute.location);
    }
}

void BufferGeometry::releaseGPUObjects(QOpenGLFunctions_3_3_Core* gl)
{
    if (m_ebo != 0)
    {
        gl->glDeleteBuffers(1, &m_ebo);
        m_ebo = 0;
    }

    if (m_vbo != 0)
    {
        gl->glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }

    if (m_vao != 0)
    {
        gl->glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }

    m_enabledAttributeLocations.clear();
}

GLenum BufferGeometry::bufferUsage() const
{
    return updatePolicy() == ResourceUpdateDynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
}
