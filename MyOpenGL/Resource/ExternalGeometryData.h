#ifndef EXTERNALGEOMETRYDATA_H
#define EXTERNALGEOMETRYDATA_H

#include "Geometry.h"

#include <QOpenGLFunctions_3_3_Core>

#include <cstddef>
#include <vector>

typedef unsigned long long ExternalGeometryRevision;

/// 外部 Vertex Buffer 的只读 CPU 内存视图。
/// data 的所有权始终属于外部库。
struct ExternalVertexBufferView
{
    const void* data;     // 当前 Vertex Stream 首地址。
    std::size_t byteSize; // 当前 Stream 总字节数。
    GLsizei stride;       // 相邻 Vertex Element 的字节距离。
};

/// 外部 Vertex Attribute 到 Vertex Stream 的映射。
struct ExternalVertexAttribute
{
    GLuint location;        // MyOpenGL 统一 Vertex Attribute Location，由 Adapter 按数据语义映射。
    GLint componentCount;   // Attribute 分量数量。
    int bufferIndex;        // Attribute 所属 Vertex Stream。
    std::size_t byteOffset; // Attribute 在一个 Vertex Element 内的字节偏移。
    GLenum componentType;   // GL_FLOAT、GL_DOUBLE 等底层数据类型。
    bool normalized;        // 整数类型是否标准化后送入浮点 Shader Attribute。
};

/// 外部 Index Buffer 的只读 CPU 内存视图。
struct ExternalIndexBufferView
{
    const void* data;     // 当前 Index Buffer 首地址。
    std::size_t byteSize; // 当前 Index Buffer 总字节数。
    int indexCount;       // DrawElements 使用的 Index 数量。
    GLenum indexType;     // GL_UNSIGNED_SHORT 或 GL_UNSIGNED_INT。
};

/// 外部 Geometry 当前完整内存布局。
/// 支持 Triangles、Lines 和 LineStrip；该结构只描述外部内存，不拥有任何 Vertex / Index 数据。
struct ExternalGeometryDataView
{
    int vertexCount;                                      // 当前 Geometry Vertex 总数量。
    RenderType renderType;                                // 当前几何绘制类型。
    std::vector<ExternalVertexBufferView> vertexBuffers;  // 外部 Geometry 的 Vertex Streams。
    std::vector<ExternalVertexAttribute> attributes;      // 外部数据到 MyOpenGL Vertex Attribute 的映射。
    ExternalIndexBufferView indices;                      // 当前 Index Buffer。
};

/// ExternalGeometryDirtyRange::bufferIndex 的特殊值。
const int ExternalGeometryIndexBuffer = -1;

/// External Geometry 一个局部修改的字节范围。
struct ExternalGeometryDirtyRange
{
    int bufferIndex;        // >=0 表示 Vertex Stream，-1 表示 Index Buffer。
    std::size_t byteOffset; // 当前 Buffer 中的起始字节。
    std::size_t byteSize;   // 当前变化区域字节数。
};

/// 两个 Content Revision 之间的增量变化集合。
struct ExternalGeometryChangeSet
{
    ExternalGeometryRevision fromRevision;                  // ChangeSet 起始 Content Revision。
    ExternalGeometryRevision toRevision;                    // ChangeSet 结束 Content Revision。
    std::vector<ExternalGeometryDirtyRange> dirtyRanges;    // 两个 Revision 之间所有已知局部变化。
};

#endif // EXTERNALGEOMETRYDATA_H
