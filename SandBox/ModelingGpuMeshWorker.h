#ifndef MODELINGGPUMESHWORKER_H
#define MODELINGGPUMESHWORKER_H

#include <QMutex>
#include <QThread>
#include <QWaitCondition>

class ModelingGpuMesh;
class ModelingGpuMeshSync;
class QOffscreenSurface;
class QOpenGLContext;

/// External GPU Library Worker Thread。
/// 使用独立 Shared OpenGL Context，把 ModelingMesh Source 同步到外部 GPU Storage。
class ModelingGpuMeshWorker : public QThread
{
public:
    ModelingGpuMeshWorker();
    ~ModelingGpuMeshWorker() override;

    /// Worker 配置
    bool configure(QOpenGLContext* context, QOffscreenSurface* surface, ModelingGpuMesh* gpuMesh, ModelingGpuMeshSync* sync, QThread* returnThread);

    /// Worker 生命周期
    bool startAndWait(); // 启动 Worker，并等待 Shared Context 中的初始 GPU Storage 和 Write Fence 创建完成。
    void stopAndWait();  // 请求 Worker 释放外部 GPU Storage，退出线程并等待结束。

    /// GPU Storage 请求
    bool requestSyncAndWait();              // 请求一次 ModelingMesh -> GPU Storage Sync；只等待 Write Fence 发布。
    bool requestBufferReplacementAndWait(); // 请求使用相同 ModelingMesh Source 创建全新的 VBO / EBO；只等待 Write Fence 发布。

protected:
    void run() override;

private:
    enum WorkRequest
    {
        WorkRequestNone,
        WorkRequestSync,
        WorkRequestReplaceBuffers
    };

    /// Worker 请求
    bool requestWorkAndWait(WorkRequest request);

    /// Worker 状态
    void publishStartResult(bool succeeded);
    void publishWorkResult(unsigned long long generation, bool succeeded);

private:
    QOpenGLContext* m_context;               // Worker 使用的 Shared OpenGL Context，不拥有。
    QOffscreenSurface* m_surface;            // Worker makeCurrent 使用的 Offscreen Surface，不拥有。
    ModelingGpuMesh* m_gpuMesh;              // Worker 维护的 External GPU Storage，不拥有。
    ModelingGpuMeshSync* m_sync;             // Renderer / Writer GPU Fence 交换状态，不拥有。
    QThread* m_returnThread;                  // Worker 退出前把 QOpenGLContext 移回的 GUI Thread。

    QMutex m_mutex;                           // Worker Request / Completion CPU 同步。
    QWaitCondition m_condition;               // Worker Request / Completion 等待条件。
    WorkRequest m_requestedWork;              // 当前 GUI 请求的 External GPU Storage 操作。
    unsigned long long m_requestedGeneration; // GUI 已请求的最新 Work Generation。
    unsigned long long m_completedGeneration; // Worker 已发布 Write Fence 的最新 Generation。
    bool m_startFinished;                     // Worker 初始化阶段是否结束。
    bool m_startSucceeded;                    // Shared Context 和初始 GPU Storage 是否初始化成功。
    bool m_stopRequested;                     // GUI 是否请求 Worker 退出。
    bool m_failed;                            // Worker 是否发生不可恢复错误。
};

#endif // MODELINGGPUMESHWORKER_H