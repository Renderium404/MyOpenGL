#ifndef EXTERNALGPUMESHDATASOURCE_H
#define EXTERNALGPUMESHDATASOURCE_H

#include "Resource/ExternalGpuMeshData.h"

#include <QOpenGLFunctions_3_3_Core>

typedef unsigned long long ExternalGpuMeshRevision;

/// 外部 GPU Mesh 的数据源接口。
/// MyOpenGL 不拥有 DataSource；外部 GPU Library 通常通过 Application Adapter 实现该接口。
class ExternalGpuMeshDataSource
{
public:
    virtual ~ExternalGpuMeshDataSource()
    {
    }

    /// GPU View
    virtual bool gpuView(ExternalGpuMeshView& view) const = 0; // 获取当前外部 VBO / EBO 和 Vertex Layout。

    /// Structure Revision
    virtual ExternalGpuMeshRevision structureRevision() const = 0; // Buffer ID、Layout、Index Count 等结构变化时递增。

    /// GPU View 同步
    virtual bool prepareGpuViewGL(QOpenGLFunctions_3_3_Core* gl) const = 0; // MyOpenGL 获取新 GPU View / 重配 VAO 前调用；共享 Context 数据源必须保证最新 Structure Write 已进入当前 Context 的执行顺序。
    virtual void acknowledgeStructureRevision(ExternalGpuMeshRevision revision) const = 0; // MyOpenGL 成功配置该 Revision 的 VAO 后调用；Producer 可据此退休旧 GPU Object。

    /// GPU 读取同步
    virtual bool beginReadGL(QOpenGLFunctions_3_3_Core* gl) const = 0; // Renderer 绑定 VAO 前调用；共享 Context 数据源必须保证外部 GPU 写入已经完成并对当前 Context 可见。
    virtual void endReadGL(QOpenGLFunctions_3_3_Core* gl) const = 0;   // Draw 提交后调用；共享 Context 数据源可在此发布 Renderer 已完成读取的同步信息。
};

#endif // EXTERNALGPUMESHDATASOURCE_H