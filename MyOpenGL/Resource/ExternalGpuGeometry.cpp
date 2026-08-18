#include "ExternalGpuGeometry.h"

#include <QDebug>

ExternalGpuGeometry::ExternalGpuGeometry(const QString& name)
    : Geometry(name, ResourceUpdateStatic)
    , m_source(0)
    , m_observedStructureRevision(0)
    , m_synchronizedStructureRevision(0)
    , m_vao(0)
{
    m_view.renderType = Triangles;
    m_view.indices.bufferId = 0;
    m_view.indices.indexCount = 0;
    m_view.indices.indexType = GL_UNSIGNED_INT;
}

ExternalGpuGeometry::~ExternalGpuGeometry()
{
}

/// 自动数据源模式

bool ExternalGpuGeometry::setDataSource(const ExternalGpuGeometryDataSource* source)
{
    if (source == 0)
    {
        qWarning() << "ExternalGpuGeometry setDataSource failed: source is null:" << name();
        return false;
    }

    m_source = source;
    m_observedStructureRevision = source->structureRevision();
    m_synchronizedStructureRevision = 0;

    // 第一次绑定 DataSource 后需要在 OpenGL Context 当前时取得 GPU View，
    // 等待可能存在的 External Write Fence，并创建 / 配置 MyOpenGL VAO。
    markFullDirty();
    return true;
}

const ExternalGpuGeometryDataSource* ExternalGpuGeometry::dataSource() const
{
    return m_source;
}

/// 手动 GPU View 模式

bool ExternalGpuGeometry::setGpuView(const ExternalGpuGeometryView& view)
{
    if (!validateGpuView(view))
        return false;

    m_source = 0;
    m_view = view;
    m_observedStructureRevision = 0;
    m_synchronizedStructureRevision = 0;

    // Buffer ID 或 Attribute Layout 变化时，只需要重新配置 MyOpenGL 自己的 VAO。
    markFullDirty();
    return true;
}

const ExternalGpuGeometryView& ExternalGpuGeometry::gpuView() const
{
    return m_view;
}

/// Revision 调试状态

ExternalGpuGeometryRevision ExternalGpuGeometry::structureRevision() const
{
    return m_observedStructureRevision;
}

ExternalGpuGeometryRevision ExternalGpuGeometry::synchronizedStructureRevision() const
{
    return m_synchronizedStructureRevision;
}

/// Renderer 接口

GLuint ExternalGpuGeometry::vao() const
{
    return m_vao;
}

int ExternalGpuGeometry::indexCount() const
{
    return m_view.indices.indexCount;
}

GLenum ExternalGpuGeometry::indexType() const
{
    return m_view.indices.indexType;
}

RenderType ExternalGpuGeometry::renderType() const
{
    return m_view.renderType;
}

bool ExternalGpuGeometry::hasAttribute(GLuint location, GLint componentCount) const
{
    for (std::size_t i = 0; i < m_view.attributes.size(); ++i)
    {
        if (m_view.attributes[i].location == location && m_view.attributes[i].componentCount == componentCount)
            return true;
    }

    return false;
}

/// 绘制同步

bool ExternalGpuGeometry::prepareDrawGL(QOpenGLFunctions_3_3_Core* gl) const
{
    if (gl == 0)
    {
        qWarning() << "ExternalGpuGeometry prepareDrawGL failed: OpenGL functions are null:" << name();
        return false;
    }

    if (m_source == 0)
        return true;

    // 必须在 Renderer 重新绑定 VAO 之前完成 External GPU Write → Renderer Read 的同步。
    // 即使 Structure Revision 没有变化，外部库仍可能修改相同 VBO / EBO 的 Data Store 或内容。
    if (!m_source->beginReadGL(gl))
    {
        qWarning() << "ExternalGpuGeometry prepareDrawGL failed: DataSource cannot acquire GPU read access:" << name();
        return false;
    }

    return true;
}

void ExternalGpuGeometry::finishDrawGL(QOpenGLFunctions_3_3_Core* gl) const
{
    if (gl == 0)
    {
        qWarning() << "ExternalGpuGeometry finishDrawGL failed: OpenGL functions are null:" << name();
        return;
    }

    if (m_source == 0)
        return;

    // Draw 已经提交到 Renderer Command Stream，DataSource 可在这里建立 Render → External Writer 的同步点。
    m_source->endReadGL(gl);
}

/// Resource GPU 实现

bool ExternalGpuGeometry::onPrepareSync()
{
    if (m_source == 0)
        return true;

    const ExternalGpuGeometryRevision currentStructureRevision = m_source->structureRevision();

    // GPU Buffer 内容变化不需要通知 MyOpenGL；只有 Structure Revision 变化才重新获取 GPU View。
    if (currentStructureRevision == m_observedStructureRevision)
        return true;

    // 这里只记录“已经观察到”的 Revision。
    // 真正提交给 Renderer 的 Revision 必须等 GPU View 验证和 VAO 配置全部成功后才能前移。
    m_observedStructureRevision = currentStructureRevision;

    // Structure 变化只要求重新配置 VAO，不会复制或上传任何 Geometry 数据。
    markFullDirty();
    return true;
}

bool ExternalGpuGeometry::onInitializeGL(QOpenGLFunctions_3_3_Core* gl)
{
    return synchronizeGpuViewGL(gl);
}

bool ExternalGpuGeometry::onUpdateFullGL(QOpenGLFunctions_3_3_Core* gl)
{
    return synchronizeGpuViewGL(gl);
}

bool ExternalGpuGeometry::onUpdatePartialGL(QOpenGLFunctions_3_3_Core* gl)
{
    Q_UNUSED(gl);

    // 外部 GPU Buffer 内容由外部库直接维护，因此 MyOpenGL 不存在 Partial GPU Upload。
    return true;
}

void ExternalGpuGeometry::onReleaseGL(QOpenGLFunctions_3_3_Core* gl)
{
    // 只释放 MyOpenGL 自己创建的 VAO，绝不能删除外部库拥有的 VBO / EBO。
    releaseVAO(gl);

    // VAO 已不存在，因此当前 Context 已经没有任何已提交的 External GPU Structure。
    // Observed Revision 保留，用于 Context 重建后的重新初始化；Synchronized Revision 必须清零。
    m_synchronizedStructureRevision = 0;
}

/// GPU View 同步

bool ExternalGpuGeometry::synchronizeGpuViewGL(QOpenGLFunctions_3_3_Core* gl)
{
    if (gl == 0)
    {
        qWarning() << "ExternalGpuGeometry synchronizeGpuViewGL failed: OpenGL functions are null:" << name();
        return false;
    }

    if (m_source == 0)
    {
        if (!validateGpuView(m_view))
            return false;

        if (!validateGpuObjects(gl, m_view))
            return false;

        return configureVAO(gl, m_view, 0);
    }

    // Structure Revision 和 GPU View 是由外部 Producer 发布的两个相关状态。
    // 正常 DataSource 应提供一致快照；这里允许最多 3 次重试，用于吸收恰好发生在快照期间的并发 Structure Change。
    const int maximumSnapshotAttempts = 3;

    for (int attempt = 0; attempt < maximumSnapshotAttempts; ++attempt)
    {
        // 必须先等待 Producer 对新 Buffer / 新 Data Store 的写入进入当前 Context 的执行顺序，
        // 再读取 GPU View、glIsBuffer() 并重新 Attach 到当前 VAO。
        if (!m_source->prepareGpuViewGL(gl))
        {
            qWarning() << "ExternalGpuGeometry synchronizeGpuViewGL failed: DataSource cannot prepare GPU View:" << name();
            return false;
        }

        const ExternalGpuGeometryRevision revisionBeforeView = m_source->structureRevision();

        ExternalGpuGeometryView newView;

        if (!m_source->gpuView(newView))
        {
            qWarning() << "ExternalGpuGeometry synchronizeGpuViewGL failed: DataSource cannot provide GPU View:" << name();
            return false;
        }

        const ExternalGpuGeometryRevision revisionAfterView = m_source->structureRevision();

        // 如果获取 View 的过程中 Producer 又发布了下一版 Structure，
        // 当前 View 无法再可靠地和某一个 Revision 对应，重新等待并取得下一份稳定快照。
        if (revisionBeforeView != revisionAfterView)
        {
            m_observedStructureRevision = revisionAfterView;
            continue;
        }

        if (!validateGpuView(newView))
            return false;

        if (!validateGpuObjects(gl, newView))
            return false;

        const ExternalGpuGeometryRevision oldSynchronizedRevision = m_synchronizedStructureRevision;

        // configureVAO() 成功之前不修改 m_view 和 Synchronized Revision。
        // 因此任何失败都不会把“观察到的新 Revision”伪装成“已经提交的 Revision”。
        if (!configureVAO(gl, newView, revisionAfterView))
            return false;

        m_view = newView;
        m_observedStructureRevision = revisionAfterView;
        m_synchronizedStructureRevision = revisionAfterView;

        if (oldSynchronizedRevision != m_synchronizedStructureRevision)
        {
            qDebug() << "ExternalGpuGeometry Structure Revision synchronized:"
                     << name()
                     << "OldRevision=" << static_cast<qulonglong>(oldSynchronizedRevision)
                     << "NewRevision=" << static_cast<qulonglong>(m_synchronizedStructureRevision)
                     << "IndexCount=" << m_view.indices.indexCount;
        }

        // 只有 GPU View 验证和 VAO 配置全部成功后才能确认该 Revision。
        // Producer 可以利用这个确认回收已经被更新 Structure 完全替代的旧 GPU Object。
        m_source->acknowledgeStructureRevision(m_synchronizedStructureRevision);

        return true;
    }

    qWarning() << "ExternalGpuGeometry synchronizeGpuViewGL failed: GPU Structure changed continuously while acquiring View:" << name();
    return false;
}

/// 数据验证

bool ExternalGpuGeometry::validateGpuView(const ExternalGpuGeometryView& view) const
{
    if (view.vertexBuffers.empty())
    {
        qWarning() << "ExternalGpuGeometry validation failed: vertex buffers are empty:" << name();
        return false;
    }

    if (view.attributes.empty())
    {
        qWarning() << "ExternalGpuGeometry validation failed: attributes are empty:" << name();
        return false;
    }

    for (std::size_t i = 0; i < view.vertexBuffers.size(); ++i)
    {
        if (view.vertexBuffers[i].bufferId == 0)
        {
            qWarning() << "ExternalGpuGeometry validation failed: vertex buffer ID is zero:" << i << name();
            return false;
        }

        if (view.vertexBuffers[i].stride <= 0)
        {
            qWarning() << "ExternalGpuGeometry validation failed: vertex buffer stride must be greater than zero:" << i << name();
            return false;
        }
    }

    for (std::size_t i = 0; i < view.attributes.size(); ++i)
    {
        const ExternalGpuVertexAttribute& attribute = view.attributes[i];

        if (attribute.bufferIndex < 0 || attribute.bufferIndex >= static_cast<int>(view.vertexBuffers.size()))
        {
            qWarning() << "ExternalGpuGeometry validation failed: attribute buffer index is invalid:" << name();
            return false;
        }

        if (attribute.componentCount <= 0 || attribute.componentCount > 4)
        {
            qWarning() << "ExternalGpuGeometry validation failed: attribute component count must be between 1 and 4:" << name();
            return false;
        }

        if (!isSupportedComponentType(attribute.componentType))
        {
            qWarning() << "ExternalGpuGeometry validation failed: unsupported attribute component type:" << attribute.componentType << name();
            return false;
        }

        for (std::size_t j = i + 1; j < view.attributes.size(); ++j)
        {
            if (attribute.location == view.attributes[j].location)
            {
                qWarning() << "ExternalGpuGeometry validation failed: duplicate attribute location:" << attribute.location << name();
                return false;
            }
        }
    }

    if (view.indices.bufferId == 0)
    {
        qWarning() << "ExternalGpuGeometry validation failed: index buffer ID is zero:" << name();
        return false;
    }

    if (view.indices.indexCount <= 0)
    {
        qWarning() << "ExternalGpuGeometry validation failed: index count must be greater than zero:" << name();
        return false;
    }

    if (!isSupportedIndexType(view.indices.indexType))
    {
        qWarning() << "ExternalGpuGeometry validation failed: unsupported index type:" << name();
        return false;
    }

    if (view.renderType == Triangles && view.indices.indexCount % 3 != 0)
    {
        qWarning() << "ExternalGpuGeometry validation failed: triangle index count must be divisible by 3:" << name();
        return false;
    }

    if (view.renderType == Lines && view.indices.indexCount % 2 != 0)
    {
        qWarning() << "ExternalGpuGeometry validation failed: line index count must be divisible by 2:" << name();
        return false;
    }

    if (view.renderType == LineStrip && view.indices.indexCount < 2)
    {
        qWarning() << "ExternalGpuGeometry validation failed: line strip requires at least 2 indices:" << name();
        return false;
    }

    return true;
}

bool ExternalGpuGeometry::validateGpuObjects(QOpenGLFunctions_3_3_Core* gl, const ExternalGpuGeometryView& view) const
{
    for (std::size_t i = 0; i < view.vertexBuffers.size(); ++i)
    {
        const GLuint bufferId = view.vertexBuffers[i].bufferId;

        if (gl->glIsBuffer(bufferId) != GL_TRUE)
        {
            qWarning() << "ExternalGpuGeometry validation failed: external vertex buffer is not valid in current OpenGL Context:"
                       << bufferId
                       << name();
            return false;
        }
    }

    if (gl->glIsBuffer(view.indices.bufferId) != GL_TRUE)
    {
        qWarning() << "ExternalGpuGeometry validation failed: external index buffer is not valid in current OpenGL Context:"
                   << view.indices.bufferId
                   << name();
        return false;
    }

    return true;
}

/// VAO State

bool ExternalGpuGeometry::configureVAO(QOpenGLFunctions_3_3_Core* gl, const ExternalGpuGeometryView& view, ExternalGpuGeometryRevision revision)
{
    if (m_vao == 0)
        gl->glGenVertexArrays(1, &m_vao);

    if (m_vao == 0)
    {
        qWarning() << "ExternalGpuGeometry configureVAO failed: VAO creation failed:" << name();
        return false;
    }

    gl->glBindVertexArray(m_vao);

    // Full Update 前先清除旧 Layout，避免新的 GPU View 删除 Attribute 后旧 VAO 状态继续残留。
    for (std::size_t i = 0; i < m_enabledAttributes.size(); ++i)
        gl->glDisableVertexAttribArray(m_enabledAttributes[i]);

    m_enabledAttributes.clear();

    for (std::size_t i = 0; i < view.attributes.size(); ++i)
    {
        const ExternalGpuVertexAttribute& attribute = view.attributes[i];
        const ExternalGpuVertexBufferView& buffer = view.vertexBuffers[attribute.bufferIndex];

        gl->glBindBuffer(GL_ARRAY_BUFFER, buffer.bufferId);
        gl->glVertexAttribPointer(attribute.location, attribute.componentCount, attribute.componentType, attribute.normalized ? GL_TRUE : GL_FALSE, buffer.stride, reinterpret_cast<const void*>(attribute.byteOffset));
        gl->glEnableVertexAttribArray(attribute.location);

        m_enabledAttributes.push_back(attribute.location);
    }

    // GL_ELEMENT_ARRAY_BUFFER Binding 属于 VAO，因此把外部 EBO 直接记录在当前 MyOpenGL VAO 中。
    gl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, view.indices.bufferId);

    gl->glBindBuffer(GL_ARRAY_BUFFER, 0);
    gl->glBindVertexArray(0);

    qDebug() << "ExternalGpuGeometry VAO configured:"
             << name()
             << "StructureRevision=" << static_cast<qulonglong>(revision)
             << "VAO=" << m_vao
             << "ExternalVBOs=" << static_cast<int>(view.vertexBuffers.size())
             << "ExternalEBO=" << view.indices.bufferId
             << "IndexCount=" << view.indices.indexCount
             << "MyOpenGLUpload=0 bytes";

    return true;
}

void ExternalGpuGeometry::releaseVAO(QOpenGLFunctions_3_3_Core* gl)
{
    if (m_vao != 0)
    {
        gl->glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }

    m_enabledAttributes.clear();
}

/// 内部辅助

bool ExternalGpuGeometry::isSupportedComponentType(GLenum type) const
{
    switch (type)
    {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:
    case GL_SHORT:
    case GL_UNSIGNED_SHORT:
    case GL_INT:
    case GL_UNSIGNED_INT:
    case GL_HALF_FLOAT:
    case GL_FLOAT:
    case GL_DOUBLE:
        return true;
    }

    return false;
}

bool ExternalGpuGeometry::isSupportedIndexType(GLenum type) const
{
    return type == GL_UNSIGNED_SHORT || type == GL_UNSIGNED_INT;
}
