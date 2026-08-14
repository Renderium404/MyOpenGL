#ifndef MODELINGGPUMESHSYNC_H
#define MODELINGGPUMESHSYNC_H

#include <QMutex>
#include <QOpenGLFunctions_3_3_Core>

/// External GPU Writer Context 与 MyOpenGL Renderer Context 之间的 GPU Fence 交换状态。
/// GLsync 属于 OpenGL Share Group，因此两个共享 Context 都可以等待和删除这些 Sync Object。
class ModelingGpuMeshSync
{
public:
    ModelingGpuMeshSync();
    ~ModelingGpuMeshSync();

    /// External Writer -> Renderer
    bool publishExternalWriteCompleteGL(QOpenGLFunctions_3_3_Core* gl); // Writer 完成 Buffer Update 后创建 Fence 并 Flush。
    bool waitExternalWriteCompleteGL(QOpenGLFunctions_3_3_Core* gl);    // Renderer 在读取 / Attach External GPU Buffer 前等待最新 Writer Fence。

    /// Renderer -> External Writer
    bool publishRendererReadCompleteGL(QOpenGLFunctions_3_3_Core* gl); // Renderer Draw 提交后创建 Fence 并 Flush。
    bool waitRendererReadCompleteGL(QOpenGLFunctions_3_3_Core* gl);    // Writer 在再次修改共享 Buffer 前等待最新 Renderer Fence。

    /// Structure Synchronization
    void acknowledgeStructureRevision(unsigned long long revision); // Renderer VAO 成功切换后发布 CPU Structure Acknowledgment。
    unsigned long long synchronizedStructureRevision();              // 获取 Renderer 已确认的最新 GPU Structure Revision。

    /// GPU 生命周期
    void releaseGL(QOpenGLFunctions_3_3_Core* gl); // 删除当前尚未被另一端消费的共享 Sync Object。

private:
    /// Fence Transfer
    GLsync takeExternalWriteFence();
    GLsync takeRendererReadFence();
    void replaceExternalWriteFence(QOpenGLFunctions_3_3_Core* gl, GLsync fence);
    void replaceRendererReadFence(QOpenGLFunctions_3_3_Core* gl, GLsync fence);

private:
    QMutex m_mutex;                                    // 保护 GLsync Handle 和 Structure Acknowledgment 的 CPU 线程间状态。
    GLsync m_externalWriteFence;                       // External Writer 最近一次 GPU Buffer Update 完成 Fence。
    GLsync m_rendererReadFence;                        // Renderer 最近一次读取共享 Buffer 完成 Fence。
    unsigned long long m_synchronizedStructureRevision; // Renderer VAO 已成功采用的最新 GPU Structure Revision。
};

#endif // MODELINGGPUMESHSYNC_H