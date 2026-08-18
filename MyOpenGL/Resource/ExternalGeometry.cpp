#include "ExternalGeometry.h"

#include <QDebug>

#include <algorithm>

ExternalGeometry::ExternalGeometry(const QString& name, ResourceUpdatePolicy updatePolicy)
    : Geometry(name, updatePolicy)
    , m_source(0)
    , m_structureRevision(0)
    , m_contentRevision(0)
    , m_vao(0)
    , m_indexBuffer(0)
{
    m_view.vertexCount = 0;
    m_view.renderType = Triangles;

    m_view.indices.data = 0;
    m_view.indices.byteSize = 0;
    m_view.indices.indexCount = 0;
    m_view.indices.indexType = GL_UNSIGNED_INT;

    resetSyncStatistics();
}

ExternalGeometry::~ExternalGeometry()
{
}

/// 自动数据源模式

bool ExternalGeometry::setDataSource(const ExternalGeometryDataSource* source)
{
    if (source == 0)
    {
        qWarning() << "ExternalGeometry setDataSource failed: source is null:" << name();
        return false;
    }

    ExternalGeometryDataView view;

    if (!source->dataView(view))
    {
        qWarning() << "ExternalGeometry setDataSource failed: source cannot provide DataView:" << name();
        return false;
    }

    if (!validateDataView(view))
        return false;

    m_source = source;
    m_view = view;
    m_structureRevision = source->structureRevision();
    m_contentRevision = source->contentRevision();
    m_dirtyRanges.clear();

    markFullDirty();
    return true;
}

const ExternalGeometryDataSource* ExternalGeometry::dataSource() const
{
    return m_source;
}

/// 手动数据视图模式

bool ExternalGeometry::setDataView(const ExternalGeometryDataView& view)
{
    if (!validateDataView(view))
        return false;

    m_source = 0;
    m_view = view;
    m_structureRevision = 0;
    m_contentRevision = 0;
    m_dirtyRanges.clear();

    markFullDirty();
    return true;
}

const ExternalGeometryDataView& ExternalGeometry::dataView() const
{
    return m_view;
}

/// 手动变化通知

bool ExternalGeometry::markVertexRangeDirty(int bufferIndex, std::size_t byteOffset, std::size_t byteSize)
{
    if (m_source != 0)
    {
        qWarning() << "ExternalGeometry markVertexRangeDirty failed: automatic DataSource mode is active:" << name();
        return false;
    }

    ExternalGeometryDirtyRange range;
    range.bufferIndex = bufferIndex;
    range.byteOffset = byteOffset;
    range.byteSize = byteSize;

    if (!validateDirtyRange(range))
        return false;

    appendDirtyRange(range);
    markPartialDirty();
    return true;
}

bool ExternalGeometry::markIndexRangeDirty(std::size_t byteOffset, std::size_t byteSize)
{
    if (m_source != 0)
    {
        qWarning() << "ExternalGeometry markIndexRangeDirty failed: automatic DataSource mode is active:" << name();
        return false;
    }

    ExternalGeometryDirtyRange range;
    range.bufferIndex = ExternalGeometryIndexBuffer;
    range.byteOffset = byteOffset;
    range.byteSize = byteSize;

    if (!validateDirtyRange(range))
        return false;

    appendDirtyRange(range);
    markPartialDirty();
    return true;
}

void ExternalGeometry::markAllDataDirty()
{
    m_dirtyRanges.clear();
    markFullDirty();
}

/// Revision 调试状态

ExternalGeometryRevision ExternalGeometry::synchronizedStructureRevision() const
{
    return m_structureRevision;
}

ExternalGeometryRevision ExternalGeometry::synchronizedContentRevision() const
{
    return m_contentRevision;
}

/// GPU 同步统计

const ExternalGeometrySyncStatistics& ExternalGeometry::syncStatistics() const
{
    return m_syncStatistics;
}

void ExternalGeometry::resetSyncStatistics()
{
    m_syncStatistics.fullSyncCount = 0;
    m_syncStatistics.partialSyncCount = 0;
    m_syncStatistics.vertexUploadCalls = 0;
    m_syncStatistics.indexUploadCalls = 0;
    m_syncStatistics.totalUploadedBytes = 0;
    m_syncStatistics.lastUploadedBytes = 0;
    m_syncStatistics.lastSyncType = ExternalGeometrySyncNone;
}

/// Renderer 接口

GLuint ExternalGeometry::vao() const
{
    return m_vao;
}

int ExternalGeometry::indexCount() const
{
    return m_view.indices.indexCount;
}

GLenum ExternalGeometry::indexType() const
{
    return m_view.indices.indexType;
}

RenderType ExternalGeometry::renderType() const
{
    return m_view.renderType;
}

bool ExternalGeometry::hasAttribute(GLuint location, GLint componentCount) const
{
    for (std::size_t i = 0; i < m_view.attributes.size(); ++i)
    {
        if (m_view.attributes[i].location == location && m_view.attributes[i].componentCount == componentCount)
            return true;
    }

    return false;
}

/// Resource GPU 实现

bool ExternalGeometry::onPrepareSync()
{
    if (m_source == 0)
        return true;

    const ExternalGeometryRevision currentStructureRevision = m_source->structureRevision();
    const ExternalGeometryRevision currentContentRevision = m_source->contentRevision();

    // Structure Revision 变化意味着旧 DataView 已不能保证继续有效，必须重新获取完整 View。
    if (currentStructureRevision != m_structureRevision)
    {
        ExternalGeometryDataView newView;

        if (!m_source->dataView(newView))
        {
            qWarning() << "ExternalGeometry prepare failed: DataSource cannot provide updated DataView:" << name();
            return false;
        }

        if (!validateDataView(newView))
            return false;

        m_view = newView;
        m_structureRevision = currentStructureRevision;
        m_contentRevision = currentContentRevision;
        m_dirtyRanges.clear();

        markFullDirty();
        return true;
    }

    // Revision 完全一致时不扫描 Geometry 数据，也不会产生任何 GPU Upload。
    if (currentContentRevision == m_contentRevision)
        return true;

    // 已经要求 Full Update 时无需再收集局部变化，当前外部内存会在 Full Upload 时直接读取。
    if (dirtyState() == ResourceFullDirty)
    {
        m_contentRevision = currentContentRevision;
        m_dirtyRanges.clear();
        return true;
    }

    ExternalGeometryChangeSet changeSet;

    if (!m_source->changesSince(m_contentRevision, changeSet))
    {
        // DataSource 无法提供完整历史不是错误；退化为 Full Update 即可保证结果正确。
        m_contentRevision = currentContentRevision;
        m_dirtyRanges.clear();
        markFullDirty();
        return true;
    }

    if (changeSet.fromRevision != m_contentRevision || changeSet.toRevision != currentContentRevision)
    {
        // Revision 链不连续时不能安全执行 Partial Update，因此退化为 Full Update。
        m_contentRevision = currentContentRevision;
        m_dirtyRanges.clear();
        markFullDirty();
        return true;
    }

    if (changeSet.dirtyRanges.empty())
    {
        // Content Revision 已变化但没有提供修改范围，无法证明 GPU Cache 与 CPU 数据一致。
        m_contentRevision = currentContentRevision;
        m_dirtyRanges.clear();
        markFullDirty();
        return true;
    }

    for (std::size_t i = 0; i < changeSet.dirtyRanges.size(); ++i)
    {
        if (!validateDirtyRange(changeSet.dirtyRanges[i]))
        {
            qWarning() << "ExternalGeometry prepare failed: DataSource returned invalid dirty range:" << name();
            return false;
        }

        appendDirtyRange(changeSet.dirtyRanges[i]);
    }

    m_contentRevision = currentContentRevision;
    markPartialDirty();
    return true;
}

bool ExternalGeometry::onInitializeGL(QOpenGLFunctions_3_3_Core* gl)
{
    if (!validateDataView(m_view))
        return false;

    if (!uploadFullGL(gl))
        return false;

    m_dirtyRanges.clear();
    return true;
}

bool ExternalGeometry::onUpdateFullGL(QOpenGLFunctions_3_3_Core* gl)
{
    if (!validateDataView(m_view))
        return false;

    if (!uploadFullGL(gl))
        return false;

    m_dirtyRanges.clear();
    return true;
}

bool ExternalGeometry::onUpdatePartialGL(QOpenGLFunctions_3_3_Core* gl)
{
    if (m_dirtyRanges.empty())
        return true;

    if (!validateDataView(m_view))
        return false;

    std::size_t uploadedBytes = 0;
    unsigned long long vertexCalls = 0;
    unsigned long long indexCalls = 0;

    for (std::size_t i = 0; i < m_dirtyRanges.size(); ++i)
    {
        const ExternalGeometryDirtyRange& range = m_dirtyRanges[i];

        if (range.bufferIndex >= 0)
        {
            const ExternalVertexBufferView& source = m_view.vertexBuffers[range.bufferIndex];
            const char* sourceBytes = static_cast<const char*>(source.data);

            gl->glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffers[range.bufferIndex]);
            gl->glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(range.byteOffset), static_cast<GLsizeiptr>(range.byteSize), sourceBytes + range.byteOffset);

            uploadedBytes += range.byteSize;
            ++vertexCalls;
        }
        else
        {
            const char* sourceBytes = static_cast<const char*>(m_view.indices.data);

            // EBO Binding 属于 VAO 状态，因此绑定当前 VAO 后更新其 Index Buffer。
            gl->glBindVertexArray(m_vao);
            gl->glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLintptr>(range.byteOffset), static_cast<GLsizeiptr>(range.byteSize), sourceBytes + range.byteOffset);
            gl->glBindVertexArray(0);

            uploadedBytes += range.byteSize;
            ++indexCalls;
        }
    }

    gl->glBindBuffer(GL_ARRAY_BUFFER, 0);

    recordPartialSync(uploadedBytes, vertexCalls, indexCalls);

    m_dirtyRanges.clear();
    return true;
}

void ExternalGeometry::onReleaseGL(QOpenGLFunctions_3_3_Core* gl)
{
    releaseGPUObjects(gl);
    m_dirtyRanges.clear();
}

/// 数据验证

bool ExternalGeometry::validateDataView(const ExternalGeometryDataView& view) const
{
    if (view.vertexCount <= 0)
    {
        qWarning() << "ExternalGeometry validation failed: vertex count must be greater than zero:" << name();
        return false;
    }

    if (view.vertexBuffers.empty())
    {
        qWarning() << "ExternalGeometry validation failed: vertex buffers are empty:" << name();
        return false;
    }

    if (view.attributes.empty())
    {
        qWarning() << "ExternalGeometry validation failed: attributes are empty:" << name();
        return false;
    }

    for (std::size_t i = 0; i < view.vertexBuffers.size(); ++i)
    {
        const ExternalVertexBufferView& buffer = view.vertexBuffers[i];

        if (buffer.data == 0 || buffer.byteSize == 0 || buffer.stride <= 0)
        {
            qWarning() << "ExternalGeometry validation failed: invalid vertex buffer:" << i << name();
            return false;
        }
    }

    for (std::size_t i = 0; i < view.attributes.size(); ++i)
    {
        const ExternalVertexAttribute& attribute = view.attributes[i];

        if (attribute.bufferIndex < 0 || attribute.bufferIndex >= static_cast<int>(view.vertexBuffers.size()))
        {
            qWarning() << "ExternalGeometry validation failed: attribute buffer index is invalid:" << name();
            return false;
        }

        if (attribute.componentCount <= 0 || attribute.componentCount > 4)
        {
            qWarning() << "ExternalGeometry validation failed: attribute component count must be between 1 and 4:" << name();
            return false;
        }

        const std::size_t valueSize = componentTypeSize(attribute.componentType);

        if (valueSize == 0)
        {
            qWarning() << "ExternalGeometry validation failed: unsupported attribute component type:" << attribute.componentType << name();
            return false;
        }

        const ExternalVertexBufferView& buffer = view.vertexBuffers[attribute.bufferIndex];
        const std::size_t attributeByteSize = static_cast<std::size_t>(attribute.componentCount) * valueSize;
        const std::size_t stride = static_cast<std::size_t>(buffer.stride);

        if (attribute.byteOffset > stride || attributeByteSize > stride - attribute.byteOffset)
        {
            qWarning() << "ExternalGeometry validation failed: attribute exceeds vertex stride:" << name();
            return false;
        }

        const std::size_t requiredBytes = static_cast<std::size_t>(view.vertexCount - 1) * stride + attribute.byteOffset + attributeByteSize;

        if (requiredBytes > buffer.byteSize)
        {
            qWarning() << "ExternalGeometry validation failed: vertex buffer is smaller than declared vertex count:" << name();
            return false;
        }

        for (std::size_t j = i + 1; j < view.attributes.size(); ++j)
        {
            if (attribute.location == view.attributes[j].location)
            {
                qWarning() << "ExternalGeometry validation failed: duplicate attribute location:" << attribute.location << name();
                return false;
            }
        }
    }

    if (view.indices.data == 0 || view.indices.byteSize == 0 || view.indices.indexCount <= 0)
    {
        qWarning() << "ExternalGeometry validation failed: index buffer is invalid:" << name();
        return false;
    }

    const std::size_t indexSize = indexTypeSize(view.indices.indexType);

    if (indexSize == 0)
    {
        qWarning() << "ExternalGeometry validation failed: unsupported index type:" << name();
        return false;
    }

    if (static_cast<std::size_t>(view.indices.indexCount) * indexSize > view.indices.byteSize)
    {
        qWarning() << "ExternalGeometry validation failed: index buffer is smaller than declared index count:" << name();
        return false;
    }

    if (view.renderType == Triangles && view.indices.indexCount % 3 != 0)
    {
        qWarning() << "ExternalGeometry validation failed: triangle index count must be divisible by 3:" << name();
        return false;
    }

    if (view.renderType == Lines && view.indices.indexCount % 2 != 0)
    {
        qWarning() << "ExternalGeometry validation failed: line index count must be divisible by 2:" << name();
        return false;
    }

    if (view.renderType == LineStrip && view.indices.indexCount < 2)
    {
        qWarning() << "ExternalGeometry validation failed: line strip requires at least 2 indices:" << name();
        return false;
    }

    return true;
}

bool ExternalGeometry::validateDirtyRange(const ExternalGeometryDirtyRange& range) const
{
    if (range.byteSize == 0)
    {
        qWarning() << "ExternalGeometry validation failed: dirty range byteSize is zero:" << name();
        return false;
    }

    std::size_t bufferByteSize = 0;

    if (range.bufferIndex == ExternalGeometryIndexBuffer)
    {
        bufferByteSize = m_view.indices.byteSize;
    }
    else
    {
        if (range.bufferIndex < 0 || range.bufferIndex >= static_cast<int>(m_view.vertexBuffers.size()))
        {
            qWarning() << "ExternalGeometry validation failed: dirty range buffer index is invalid:" << name();
            return false;
        }

        bufferByteSize = m_view.vertexBuffers[range.bufferIndex].byteSize;
    }

    if (range.byteOffset > bufferByteSize || range.byteSize > bufferByteSize - range.byteOffset)
    {
        qWarning() << "ExternalGeometry validation failed: dirty range exceeds buffer:" << name();
        return false;
    }

    return true;
}

/// GPU Cache

bool ExternalGeometry::uploadFullGL(QOpenGLFunctions_3_3_Core* gl)
{
    if (m_vao == 0)
        gl->glGenVertexArrays(1, &m_vao);

    if (m_vao == 0)
    {
        qWarning() << "ExternalGeometry upload failed: VAO creation failed:" << name();
        return false;
    }

    if (m_vertexBuffers.size() != m_view.vertexBuffers.size())
    {
        if (!m_vertexBuffers.empty())
            gl->glDeleteBuffers(static_cast<GLsizei>(m_vertexBuffers.size()), &m_vertexBuffers[0]);

        m_vertexBuffers.assign(m_view.vertexBuffers.size(), 0);
        gl->glGenBuffers(static_cast<GLsizei>(m_vertexBuffers.size()), &m_vertexBuffers[0]);
    }

    for (std::size_t i = 0; i < m_vertexBuffers.size(); ++i)
    {
        if (m_vertexBuffers[i] == 0)
        {
            qWarning() << "ExternalGeometry upload failed: VBO creation failed:" << i << name();
            releaseGPUObjects(gl);
            return false;
        }
    }

    if (m_indexBuffer == 0)
        gl->glGenBuffers(1, &m_indexBuffer);

    if (m_indexBuffer == 0)
    {
        qWarning() << "ExternalGeometry upload failed: EBO creation failed:" << name();
        releaseGPUObjects(gl);
        return false;
    }

    std::size_t uploadedBytes = 0;
    unsigned long long vertexCalls = 0;
    unsigned long long indexCalls = 0;

    gl->glBindVertexArray(m_vao);

    for (std::size_t i = 0; i < m_enabledAttributes.size(); ++i)
        gl->glDisableVertexAttribArray(m_enabledAttributes[i]);

    m_enabledAttributes.clear();

    for (std::size_t i = 0; i < m_view.vertexBuffers.size(); ++i)
    {
        const ExternalVertexBufferView& source = m_view.vertexBuffers[i];

        gl->glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffers[i]);
        gl->glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(source.byteSize), source.data, bufferUsage());

        uploadedBytes += source.byteSize;
        ++vertexCalls;
    }

    for (std::size_t i = 0; i < m_view.attributes.size(); ++i)
    {
        const ExternalVertexAttribute& attribute = m_view.attributes[i];
        const ExternalVertexBufferView& source = m_view.vertexBuffers[attribute.bufferIndex];

        gl->glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffers[attribute.bufferIndex]);
        gl->glVertexAttribPointer(attribute.location, attribute.componentCount, attribute.componentType, attribute.normalized ? GL_TRUE : GL_FALSE, source.stride, reinterpret_cast<const void*>(attribute.byteOffset));
        gl->glEnableVertexAttribArray(attribute.location);

        m_enabledAttributes.push_back(attribute.location);
    }

    gl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
    gl->glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_view.indices.byteSize), m_view.indices.data, bufferUsage());

    uploadedBytes += m_view.indices.byteSize;
    ++indexCalls;

    gl->glBindBuffer(GL_ARRAY_BUFFER, 0);

    // EBO Binding 属于 VAO，因此不能在解绑 VAO 前将 GL_ELEMENT_ARRAY_BUFFER 绑定为 0。
    gl->glBindVertexArray(0);

    recordFullSync(uploadedBytes, vertexCalls, indexCalls);

    return true;
}

void ExternalGeometry::releaseGPUObjects(QOpenGLFunctions_3_3_Core* gl)
{
    if (!m_vertexBuffers.empty())
    {
        gl->glDeleteBuffers(static_cast<GLsizei>(m_vertexBuffers.size()), &m_vertexBuffers[0]);
        m_vertexBuffers.clear();
    }

    if (m_indexBuffer != 0)
    {
        gl->glDeleteBuffers(1, &m_indexBuffer);
        m_indexBuffer = 0;
    }

    if (m_vao != 0)
    {
        gl->glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }

    m_enabledAttributes.clear();
}

/// GPU 统计

void ExternalGeometry::recordFullSync(std::size_t uploadedBytes, unsigned long long vertexCalls, unsigned long long indexCalls)
{
    ++m_syncStatistics.fullSyncCount;
    m_syncStatistics.vertexUploadCalls += vertexCalls;
    m_syncStatistics.indexUploadCalls += indexCalls;
    m_syncStatistics.totalUploadedBytes += static_cast<unsigned long long>(uploadedBytes);
    m_syncStatistics.lastUploadedBytes = uploadedBytes;
    m_syncStatistics.lastSyncType = ExternalGeometrySyncFull;

    qDebug() << "ExternalGeometry Full GPU Sync:"
             << name()
             << "StructureRevision=" << static_cast<qulonglong>(m_structureRevision)
             << "ContentRevision=" << static_cast<qulonglong>(m_contentRevision)
             << "UploadedBytes=" << static_cast<qulonglong>(uploadedBytes)
             << "VertexCalls=" << static_cast<qulonglong>(vertexCalls)
             << "IndexCalls=" << static_cast<qulonglong>(indexCalls);
}

void ExternalGeometry::recordPartialSync(std::size_t uploadedBytes, unsigned long long vertexCalls, unsigned long long indexCalls)
{
    ++m_syncStatistics.partialSyncCount;
    m_syncStatistics.vertexUploadCalls += vertexCalls;
    m_syncStatistics.indexUploadCalls += indexCalls;
    m_syncStatistics.totalUploadedBytes += static_cast<unsigned long long>(uploadedBytes);
    m_syncStatistics.lastUploadedBytes = uploadedBytes;
    m_syncStatistics.lastSyncType = ExternalGeometrySyncPartial;

    qDebug() << "ExternalGeometry Partial GPU Sync:"
             << name()
             << "ContentRevision=" << static_cast<qulonglong>(m_contentRevision)
             << "UploadedBytes=" << static_cast<qulonglong>(uploadedBytes)
             << "VertexCalls=" << static_cast<qulonglong>(vertexCalls)
             << "IndexCalls=" << static_cast<qulonglong>(indexCalls);
}

/// 内部辅助

void ExternalGeometry::appendDirtyRange(const ExternalGeometryDirtyRange& range)
{
    // 相同 Buffer 上发生重叠或相邻的 Dirty Range 时直接合并，减少 glBufferSubData 调用数量。
    for (std::size_t i = 0; i < m_dirtyRanges.size(); ++i)
    {
        ExternalGeometryDirtyRange& existing = m_dirtyRanges[i];

        if (existing.bufferIndex != range.bufferIndex)
            continue;

        const std::size_t existingEnd = existing.byteOffset + existing.byteSize;
        const std::size_t rangeEnd = range.byteOffset + range.byteSize;

        if (range.byteOffset > existingEnd || existing.byteOffset > rangeEnd)
            continue;

        const std::size_t mergedStart = std::min(existing.byteOffset, range.byteOffset);
        const std::size_t mergedEnd = std::max(existingEnd, rangeEnd);

        existing.byteOffset = mergedStart;
        existing.byteSize = mergedEnd - mergedStart;
        return;
    }

    m_dirtyRanges.push_back(range);
}

std::size_t ExternalGeometry::componentTypeSize(GLenum type) const
{
    switch (type)
    {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:
        return 1;

    case GL_SHORT:
    case GL_UNSIGNED_SHORT:
    case GL_HALF_FLOAT:
        return 2;

    case GL_INT:
    case GL_UNSIGNED_INT:
    case GL_FLOAT:
        return 4;

    case GL_DOUBLE:
        return 8;
    }

    return 0;
}

std::size_t ExternalGeometry::indexTypeSize(GLenum type) const
{
    switch (type)
    {
    case GL_UNSIGNED_SHORT:
        return 2;

    case GL_UNSIGNED_INT:
        return 4;
    }

    return 0;
}

GLenum ExternalGeometry::bufferUsage() const
{
    return updatePolicy() == ResourceUpdateDynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
}

/// 调试名称

const char* externalGeometrySyncTypeName(ExternalGeometrySyncType type)
{
    switch (type)
    {
    case ExternalGeometrySyncNone:
        return "None";
    case ExternalGeometrySyncFull:
        return "Full";
    case ExternalGeometrySyncPartial:
        return "Partial";
    }

    return "Unknown";
}
