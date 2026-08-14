#ifndef MODELINGGPUMESHADAPTER_H
#define MODELINGGPUMESHADAPTER_H

#include "ModelingGpuMesh.h"
#include "ModelingGpuMeshSync.h"

#include "Resource/ExternalGpuMeshDataSource.h"

/// ModelingGpuMesh GPU Storage 到 MyOpenGL ExternalGpuMeshDataSource 的 Adapter。
/// Adapter 只暴露外部 Buffer ID、GPU View Structure Revision 和 GPU Synchronization，不拥有 GPU Buffer。
class ModelingGpuMeshAdapter : public ExternalGpuMeshDataSource
{
public:
    explicit ModelingGpuMeshAdapter(const ModelingGpuMesh* mesh = 0, ModelingGpuMeshSync* sync = 0);

    /// Source
    void setMesh(const ModelingGpuMesh* mesh);
    const ModelingGpuMesh* mesh() const;

    /// GPU Synchronization
    void setSync(ModelingGpuMeshSync* sync);
    ModelingGpuMeshSync* sync() const;

    /// ExternalGpuMeshDataSource
    bool gpuView(ExternalGpuMeshView& view) const override;
    ExternalGpuMeshRevision structureRevision() const override;

    /// GPU View 同步
    bool prepareGpuViewGL(QOpenGLFunctions_3_3_Core* gl) const override; // MyOpenGL 获取新 GPU View 前等待 Worker Write Fence。
    void acknowledgeStructureRevision(ExternalGpuMeshRevision revision) const override; // VAO 成功切换后记录 Renderer 已采用的 GPU Structure Revision。

    /// GPU 读取同步
    bool beginReadGL(QOpenGLFunctions_3_3_Core* gl) const override; // Renderer 绑定 VAO 前等待 External Writer 完成共享 Buffer Update。
    void endReadGL(QOpenGLFunctions_3_3_Core* gl) const override;   // Renderer Draw 后发布 Read Fence，防止 Writer 过早覆盖共享 Buffer。

private:
    const ModelingGpuMesh* m_mesh; // 不拥有外部 GPU Storage。
    ModelingGpuMeshSync* m_sync;   // 不拥有 Renderer / Writer GPU Fence 和 Structure Acknowledgment 状态。
};

#endif // MODELINGGPUMESHADAPTER_H