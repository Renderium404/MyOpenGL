#ifndef MODELINGGPUMESH_H
#define MODELINGGPUMESH_H

#include "ModelingMesh.h"

#include <QOpenGLFunctions_3_3_Core>

#include <vector>

/// 模拟外部建模 / 计算库为 ModelingMesh 建立的 GPU Storage。
/// 本类不定义任何建模操作，只负责把 ModelingMesh 同步到自己拥有的 VBO / EBO。
/// MyOpenGL 只能借用这些 Buffer ID，绝不能负责删除或修改这些 Buffer。
class ModelingGpuMesh
{
public:
    explicit ModelingGpuMesh(const ModelingMesh* mesh = 0);
    ~ModelingGpuMesh();

    /// Source
    bool setMesh(const ModelingMesh* mesh); // GPU Buffer 初始化后不允许替换 Source。
    const ModelingMesh* mesh() const;

    /// GPU 生命周期
    bool initializeGL(QOpenGLFunctions_3_3_Core* gl); // 根据当前 ModelingMesh 创建并初始化外部库自己的 VBO / EBO。
    bool syncGL(QOpenGLFunctions_3_3_Core* gl);       // 根据 ModelingMesh Revision 执行 Full 或 Partial GPU Storage Update。
    void releaseGL(QOpenGLFunctions_3_3_Core* gl);    // 释放当前和仍未退休完成的所有外部 GPU Buffer。

    /// GPU Buffer Replacement
    bool replaceBuffersGL(QOpenGLFunctions_3_3_Core* gl); // 使用相同 ModelingMesh 数据创建全新的 VBO / EBO，并递增 GPU View Structure Revision。
    int collectRetiredBufferSetsGL(QOpenGLFunctions_3_3_Core* gl, unsigned long long synchronizedStructureRevision); // 回收 Renderer 已确认不再引用的旧 Buffer Set。

    /// GPU Buffer
    GLuint positionBuffer() const;
    GLuint normalBuffer() const;
    GLuint uvBuffer() const;
    GLuint indexBuffer() const;
    int indexCount() const;

    /// GPU View Structure Revision
    unsigned long long structureRevision() const; // Buffer ID、Layout 或 Index Count 变化时递增。

private:
    struct BufferSet
    {
        BufferSet()
            : positionBuffer(0)
            , normalBuffer(0)
            , uvBuffer(0)
            , indexBuffer(0)
        {
        }

        GLuint positionBuffer; // Position VBO。
        GLuint normalBuffer;   // Normal VBO。
        GLuint uvBuffer;       // UV VBO。
        GLuint indexBuffer;    // Triangle EBO。
    };

    struct RetiredBufferSet
    {
        BufferSet buffers;                              // 已被新 GPU Structure 替代的旧 Buffer Set。
        unsigned long long retireAfterStructureRevision; // Renderer 确认达到该 Revision 后，旧 Buffer 不再被 VAO 引用。
    };

    /// GPU Buffer Set
    bool createBufferSetGL(QOpenGLFunctions_3_3_Core* gl, BufferSet& buffers);
    void deleteBufferSetGL(QOpenGLFunctions_3_3_Core* gl, BufferSet& buffers);
    bool bufferSetInitialized(const BufferSet& buffers) const;

    /// GPU 上传
    bool uploadFullGL(QOpenGLFunctions_3_3_Core* gl);
    bool uploadFullToBufferSetGL(QOpenGLFunctions_3_3_Core* gl, const BufferSet& buffers);
    bool uploadChangesGL(QOpenGLFunctions_3_3_Core* gl, const std::vector<ModelingMeshChange>& changes, std::size_t& uploadedBytes, int& uploadCalls);

    /// 状态
    bool gpuInitialized() const;

private:
    const ModelingMesh* m_mesh;                         // 不拥有 ModelingMesh Source。
    BufferSet m_buffers;                                // 当前 ExternalGpuMeshResource 正在观察的 GPU Buffer Set。
    std::vector<RetiredBufferSet> m_retiredBufferSets;  // 等待 Renderer Structure Acknowledgment 后回收的旧 Buffer Set。
    int m_indexCount;                                   // 当前 GPU EBO 对应的 Index Count。
    unsigned long long m_sourceStructureRevision;       // GPU Storage 当前已经同步的 ModelingMesh Structure Revision。
    unsigned long long m_sourceContentRevision;         // GPU Storage 当前已经同步的 ModelingMesh Content Revision。
    unsigned long long m_structureRevision;             // ExternalGpuMeshResource 实际需要观察的 GPU View Structure Revision。
};

#endif // MODELINGGPUMESH_H