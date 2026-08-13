#ifndef EXTERNALMESHRESOURCE_H
#define EXTERNALMESHRESOURCE_H

#include "Core/Resource.h"
#include "Resource/ExternalMeshData.h"
#include "Resource/ExternalMeshDataSource.h"
#include "Resource/RenderableMesh.h"

#include <QOpenGLFunctions_3_3_Core>

#include <cstddef>
#include <vector>

/// External Mesh 最近一次实际执行的 GPU 同步类型。
enum ExternalMeshSyncType
{
    ExternalMeshSyncNone,
    ExternalMeshSyncFull,
    ExternalMeshSyncPartial
};

/// 获取 External Mesh GPU 同步类型的调试名称。
const char* externalMeshSyncTypeName(ExternalMeshSyncType type);

/// External Mesh GPU 上传统计。
/// 只统计实际传递给 glBufferData / glBufferSubData 的数据量，不包含 Driver 内部复制和 VAO State 操作。
struct ExternalMeshSyncStatistics
{
    unsigned long long fullSyncCount;       // 实际执行 Full GPU Upload 的次数。
    unsigned long long partialSyncCount;    // 实际执行 Partial GPU Upload 的次数。
    unsigned long long vertexUploadCalls;   // Vertex Buffer Upload API 调用次数。
    unsigned long long indexUploadCalls;    // Index Buffer Upload API 调用次数。
    unsigned long long totalUploadedBytes;  // Resource 生命周期内累计提交到 GPU Buffer API 的字节数。
    std::size_t lastUploadedBytes;           // 最近一次同步提交的总字节数。
    ExternalMeshSyncType lastSyncType;       // 最近一次实际 GPU 同步类型。
};

/// 外部建模网格对应的 GPU Cache。
/// CPU 数据始终属于外部建模库，本 Resource 只借用 DataView 并维护 VAO / VBO / EBO。
class ExternalMeshResource : public Resource, public RenderableMesh
{
public:
    explicit ExternalMeshResource(const QString& name = "ExternalMesh", ResourceUpdatePolicy updatePolicy = ResourceUpdateDynamic);
    ~ExternalMeshResource() override;

    /// 自动数据源模式
    bool setDataSource(const ExternalMeshDataSource* source); // 绑定非拥有的外部 DataSource，并启用 Revision 自动同步。
    const ExternalMeshDataSource* dataSource() const;

    /// 手动数据视图模式
    bool setDataView(const ExternalMeshDataView& view); // 直接借用一个 DataView；调用后解除自动 DataSource。
    const ExternalMeshDataView& dataView() const;

    /// 手动变化通知
    bool markVertexRangeDirty(int bufferIndex, std::size_t byteOffset, std::size_t byteSize); // 手动模式下通知一个 Vertex Stream 的局部变化。
    bool markIndexRangeDirty(std::size_t byteOffset, std::size_t byteSize);                    // 手动模式下通知 Index Buffer 的局部变化。
    void markAllDataDirty();                                                                   // 当前 DataView 仍有效，但整个 Buffer 内容需要重新上传。

    /// Revision 调试状态
    ExternalMeshRevision synchronizedStructureRevision() const; // 获取当前 Resource 已观察到的 Structure Revision。
    ExternalMeshRevision synchronizedContentRevision() const;   // 获取当前 Resource 已观察到的 Content Revision。

    /// GPU 同步统计
    const ExternalMeshSyncStatistics& syncStatistics() const; // 获取当前 Resource 生命周期累计的 GPU Upload 统计。
    void resetSyncStatistics();                               // 清零调试统计，不改变 GPU Resource 和 DirtyState。

    /// Renderer 接口
    const QString& renderMeshName() const override;
    bool renderMeshInitialized() const override;
    GLuint vao() const override;
    int indexCount() const override;
    GLenum indexType() const override;
    MeshPrimitiveType primitiveType() const override;
    bool hasAttribute(GLuint location, GLint componentCount) const override;

protected:
    /// Resource GPU 实现
    bool onPrepareSync() override;
    bool onInitializeGL(QOpenGLFunctions_3_3_Core* gl) override;
    bool onUpdateFullGL(QOpenGLFunctions_3_3_Core* gl) override;
    bool onUpdatePartialGL(QOpenGLFunctions_3_3_Core* gl) override;
    void onReleaseGL(QOpenGLFunctions_3_3_Core* gl) override;

private:
    /// 数据验证
    bool validateDataView(const ExternalMeshDataView& view) const;
    bool validateDirtyRange(const ExternalMeshDirtyRange& range) const;

    /// GPU Cache
    bool uploadFullGL(QOpenGLFunctions_3_3_Core* gl);
    void releaseGPUObjects(QOpenGLFunctions_3_3_Core* gl);

    /// GPU 统计
    void recordFullSync(std::size_t uploadedBytes, unsigned long long vertexCalls, unsigned long long indexCalls);
    void recordPartialSync(std::size_t uploadedBytes, unsigned long long vertexCalls, unsigned long long indexCalls);

    /// 内部辅助
    void appendDirtyRange(const ExternalMeshDirtyRange& range);
    std::size_t componentTypeSize(GLenum type) const;
    std::size_t indexTypeSize(GLenum type) const;
    GLenum bufferUsage() const;

private:
    const ExternalMeshDataSource* m_source;            // 当前自动外部数据源，不拥有该对象。
    ExternalMeshDataView m_view;                       // 当前借用的外部 Mesh DataView。
    ExternalMeshRevision m_structureRevision;          // 当前 Resource 已观察到的 Structure Revision。
    ExternalMeshRevision m_contentRevision;            // 当前 Resource 已观察到的 Content Revision。

    GLuint m_vao;                                      // 当前 External Mesh VAO。
    std::vector<GLuint> m_vertexBuffers;               // 每个外部 Vertex Stream 对应的 GPU VBO。
    GLuint m_indexBuffer;                              // 当前 GPU EBO。
    std::vector<GLuint> m_enabledAttributes;           // 当前 VAO 已启用的 Attribute Locations。
    std::vector<ExternalMeshDirtyRange> m_dirtyRanges; // 等待 Partial Update 的局部字节范围。

    ExternalMeshSyncStatistics m_syncStatistics;       // 当前 External Mesh GPU Upload 调试统计。
};

#endif // EXTERNALMESHRESOURCE_H