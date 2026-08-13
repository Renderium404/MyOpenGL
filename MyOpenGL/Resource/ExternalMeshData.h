#ifndef EXTERNALMESHDATA_H
#define EXTERNALMESHDATA_H

#include "Resource/RenderableMesh.h"

#include <QOpenGLFunctions_3_3_Core>

#include <cstddef>
#include <vector>

typedef unsigned long long ExternalMeshRevision;

/// 外部 Vertex Buffer 的只读 CPU 内存视图。
/// data 的所有权始终属于外部建模库。
struct ExternalVertexBufferView
{
    const void* data;       // 当前 Vertex Stream 首地址。
    std::size_t byteSize;   // 当前 Stream 总字节数。
    GLsizei stride;         // 相邻 Vertex Element 的字节距离。
};

/// 外部 Vertex Attribute 到 Vertex Stream 的映射。
struct ExternalVertexAttribute
{
    GLuint location;          // Shader Attribute Location。
    GLint componentCount;     // Attribute 分量数量。
    int bufferIndex;          // Attribute 所属 Vertex Stream。
    std::size_t byteOffset;   // Attribute 在一个 Vertex Element 内的字节偏移。
    GLenum componentType;     // GL_FLOAT、GL_DOUBLE 等底层数据类型。
    bool normalized;          // 整数类型是否标准化后送入浮点 Shader Attribute。
};

/// 外部 Index Buffer 的只读 CPU 内存视图。
struct ExternalIndexBufferView
{
    const void* data;       // 当前 Index Buffer 首地址。
    std::size_t byteSize;   // 当前 Index Buffer 总字节数。
    int indexCount;         // DrawElements 使用的 Index 数量。
    GLenum indexType;       // GL_UNSIGNED_SHORT 或 GL_UNSIGNED_INT。
};

/// 外部 Mesh 当前完整内存布局。
/// 该结构只描述外部内存，不拥有任何 Vertex / Index 数据。
struct ExternalMeshDataView
{
    int vertexCount;                                      // 当前 Mesh Vertex 总数量。
    MeshPrimitiveType primitiveType;                      // 当前绘制图元。
    std::vector<ExternalVertexBufferView> vertexBuffers;  // 外部 Mesh 的 Vertex Streams。
    std::vector<ExternalVertexAttribute> attributes;      // Shader Attribute Mapping。
    ExternalIndexBufferView indices;                      // 当前 Index Buffer。
};

/// ExternalMeshDirtyRange::bufferIndex 的特殊值。
const int ExternalMeshIndexBuffer = -1;

/// External Mesh 一个局部修改的字节范围。
struct ExternalMeshDirtyRange
{
    int bufferIndex;          // >=0 表示 Vertex Stream，-1 表示 Index Buffer。
    std::size_t byteOffset;   // 当前 Buffer 中的起始字节。
    std::size_t byteSize;     // 当前变化区域字节数。
};

/// 两个 Content Revision 之间的增量变化集合。
struct ExternalMeshChangeSet
{
    ExternalMeshRevision fromRevision;                 // ChangeSet 起始 Content Revision。
    ExternalMeshRevision toRevision;                   // ChangeSet 结束 Content Revision。
    std::vector<ExternalMeshDirtyRange> dirtyRanges;   // 两个 Revision 之间所有已知局部变化。
};

#endif // EXTERNALMESHDATA_H