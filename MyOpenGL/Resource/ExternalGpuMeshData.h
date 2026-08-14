#ifndef EXTERNALGPUMESHDATA_H
#define EXTERNALGPUMESHDATA_H

#include "Resource/RenderableMesh.h"

#include <QOpenGLFunctions_3_3_Core>

#include <cstddef>
#include <vector>

/// 外部 GPU Vertex Buffer 的借用视图。
/// bufferId 的所有权始终属于外部库，MyOpenGL 不创建也不删除该 Buffer。
struct ExternalGpuVertexBufferView
{
    GLuint bufferId;     // 外部 OpenGL Vertex Buffer Object。
    GLsizei stride;      // 相邻 Vertex Element 之间的字节距离。
};

/// 外部 GPU Vertex Attribute 描述。
/// Attribute Layout 由 MyOpenGL VAO 保存，但实际 Vertex 数据位于外部 VBO。
struct ExternalGpuVertexAttribute
{
    GLuint location;          // Shader Attribute Location。
    GLint componentCount;     // Attribute 分量数量，例如 position.xyz = 3。
    int bufferIndex;          // Attribute 所属 ExternalGpuVertexBufferView。
    std::size_t byteOffset;   // Attribute 在一个 Vertex Element 中的字节偏移。
    GLenum componentType;     // GL_FLOAT、GL_DOUBLE 等 OpenGL 输入类型。
    bool normalized;          // 整数输入是否标准化后送入浮点 Shader Attribute。
};

/// 外部 GPU Index Buffer 的借用视图。
/// bufferId 的生命周期由外部库负责，并且必须覆盖 ExternalGpuMeshResource 的使用周期。
struct ExternalGpuIndexBufferView
{
    GLuint bufferId;     // 外部 OpenGL Element Buffer Object。
    int indexCount;      // 当前 DrawElements 使用的 Index 数量。
    GLenum indexType;    // GL_UNSIGNED_SHORT 或 GL_UNSIGNED_INT。
};

/// 外部 GPU Mesh 的完整绘制视图。
/// 所有 Buffer 必须属于当前 OpenGL Context，或者属于与当前 Context 共享对象的 Share Group。
struct ExternalGpuMeshView
{
    MeshPrimitiveType primitiveType;                         // 当前网格图元类型。
    std::vector<ExternalGpuVertexBufferView> vertexBuffers;  // 外部 GPU Vertex Streams。
    std::vector<ExternalGpuVertexAttribute> attributes;      // Shader Attribute 到外部 VBO 的映射。
    ExternalGpuIndexBufferView indices;                      // 外部 GPU Index Buffer。
};

#endif // EXTERNALGPUMESHDATA_H