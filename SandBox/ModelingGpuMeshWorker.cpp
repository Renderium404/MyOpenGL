#include "ModelingGpuMeshWorker.h"

#include "ModelingGpuMesh.h"
#include "ModelingGpuMeshSync.h"

#include <QDebug>
#include <QMutexLocker>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>

ModelingGpuMeshWorker::ModelingGpuMeshWorker()
    : m_context(0)
    , m_surface(0)
    , m_gpuMesh(0)
    , m_sync(0)
    , m_returnThread(0)
    , m_requestedWork(WorkRequestNone)
    , m_requestedGeneration(0)
    , m_completedGeneration(0)
    , m_startFinished(false)
    , m_startSucceeded(false)
    , m_stopRequested(false)
    , m_failed(false)
{
}

ModelingGpuMeshWorker::~ModelingGpuMeshWorker()
{
    if (isRunning())
    {
        qWarning() << "ModelingGpuMeshWorker destroyed while Worker Thread is still running.";
        stopAndWait();
    }
}

/// Worker 配置

bool ModelingGpuMeshWorker::configure(QOpenGLContext* context, QOffscreenSurface* surface, ModelingGpuMesh* gpuMesh, ModelingGpuMeshSync* sync, QThread* returnThread)
{
    if (isRunning())
    {
        qWarning() << "ModelingGpuMeshWorker configure failed: Worker Thread is already running.";
        return false;
    }

    if (context == 0 || surface == 0 || gpuMesh == 0 || sync == 0 || returnThread == 0)
    {
        qWarning() << "ModelingGpuMeshWorker configure failed: invalid argument.";
        return false;
    }

    m_context = context;
    m_surface = surface;
    m_gpuMesh = gpuMesh;
    m_sync = sync;
    m_returnThread = returnThread;

    return true;
}

/// Worker 生命周期

bool ModelingGpuMeshWorker::startAndWait()
{
    QMutexLocker locker(&m_mutex);

    if (isRunning())
    {
        qWarning() << "ModelingGpuMeshWorker startAndWait failed: Worker Thread is already running.";
        return false;
    }

    if (m_context == 0 || m_surface == 0 || m_gpuMesh == 0 || m_sync == 0 || m_returnThread == 0)
    {
        qWarning() << "ModelingGpuMeshWorker startAndWait failed: Worker is not configured.";
        return false;
    }

    m_requestedWork = WorkRequestNone;
    m_requestedGeneration = 0;
    m_completedGeneration = 0;
    m_startFinished = false;
    m_startSucceeded = false;
    m_stopRequested = false;
    m_failed = false;

    start();

    while (!m_startFinished)
        m_condition.wait(&m_mutex);

    return m_startSucceeded;
}

void ModelingGpuMeshWorker::stopAndWait()
{
    {
        QMutexLocker locker(&m_mutex);

        if (!isRunning())
            return;

        m_stopRequested = true;
        m_condition.wakeAll();
    }

    wait();
}

/// GPU Storage 请求

bool ModelingGpuMeshWorker::requestSyncAndWait()
{
    return requestWorkAndWait(WorkRequestSync);
}

bool ModelingGpuMeshWorker::requestBufferReplacementAndWait()
{
    return requestWorkAndWait(WorkRequestReplaceBuffers);
}

/// Worker Thread

void ModelingGpuMeshWorker::run()
{
    bool contextCurrent = false;
    bool initialized = false;

    if (!m_context->makeCurrent(m_surface))
    {
        qWarning() << "ModelingGpuMeshWorker failed: unable to make Shared OpenGL Context current.";
    }
    else
    {
        contextCurrent = true;

        QOpenGLFunctions_3_3_Core* gl = m_context->versionFunctions<QOpenGLFunctions_3_3_Core>();

        if (gl == 0)
        {
            qWarning() << "ModelingGpuMeshWorker failed: OpenGL 3.3 Core functions are unavailable.";
        }
        else if (!m_gpuMesh->initializeGL(gl))
        {
            qWarning() << "ModelingGpuMeshWorker failed: external GPU Storage initialization failed.";
        }
        else if (!m_sync->publishExternalWriteCompleteGL(gl))
        {
            qWarning() << "ModelingGpuMeshWorker failed: initial External Write Fence creation failed.";
            m_gpuMesh->releaseGL(gl);
        }
        else
        {
            initialized = true;

            qDebug() << "ModelingGpuMeshWorker initialized:"
                     << "Thread=" << QThread::currentThreadId()
                     << "SharedContext=" << m_context
                     << "PositionVBO=" << m_gpuMesh->positionBuffer()
                     << "NormalVBO=" << m_gpuMesh->normalBuffer()
                     << "UVVBO=" << m_gpuMesh->uvBuffer()
                     << "EBO=" << m_gpuMesh->indexBuffer();
        }
    }

    publishStartResult(initialized);

    if (!initialized)
    {
        if (contextCurrent)
            m_context->doneCurrent();

        // QOpenGLContext 必须回到 GUI Thread，之后才能由 OpenGLSandboxWidget 安全销毁。
        m_context->moveToThread(m_returnThread);
        return;
    }

    QOpenGLFunctions_3_3_Core* gl = m_context->versionFunctions<QOpenGLFunctions_3_3_Core>();

    while (true)
    {
        unsigned long long generation = 0;
        WorkRequest request = WorkRequestNone;

        {
            QMutexLocker locker(&m_mutex);

            while (!m_stopRequested && m_requestedGeneration == m_completedGeneration)
                m_condition.wait(&m_mutex);

            if (m_stopRequested)
                break;

            generation = m_requestedGeneration;
            request = m_requestedWork;
        }

        bool succeeded = true;

        // 在覆盖或替换共享 VBO / EBO 之前，先等待 Renderer 对上一版本 Buffer 的 Draw 完成。
        if (!m_sync->waitRendererReadCompleteGL(gl))
            succeeded = false;

        if (succeeded)
        {
            // Structure Acknowledgment 是 CPU 侧的“VAO 已切换”确认。
            // 每次 Worker 真正开始下一项工作时顺便回收已满足退休条件的旧 Buffer Set。
            const unsigned long long synchronizedStructureRevision = m_sync->synchronizedStructureRevision();
            m_gpuMesh->collectRetiredBufferSetsGL(gl, synchronizedStructureRevision);
        }

        if (succeeded)
        {
            switch (request)
            {
            case WorkRequestSync:
                succeeded = m_gpuMesh->syncGL(gl);
                break;

            case WorkRequestReplaceBuffers:
                succeeded = m_gpuMesh->replaceBuffersGL(gl);
                break;

            case WorkRequestNone:
                qWarning() << "ModelingGpuMeshWorker failed: WorkRequest is empty.";
                succeeded = false;
                break;
            }
        }

        // GPU Storage Update 或 Buffer Replacement 后创建 Fence + Flush。
        // GUI Thread 只等待 Fence 被发布，真正的 GPU Completion 由 Renderer Context 的 WaitSync 保证。
        if (succeeded && !m_sync->publishExternalWriteCompleteGL(gl))
            succeeded = false;

        publishWorkResult(generation, succeeded);

        if (!succeeded)
            break;
    }

    // Renderer Resource / VAO 会先在 GUI Context 中释放。
    // Worker 再等待最后一个 Renderer Read Fence，然后才释放当前以及仍未退休的 External GPU Buffer。
    m_sync->waitRendererReadCompleteGL(gl);
    m_sync->releaseGL(gl);
    m_gpuMesh->releaseGL(gl);

    gl->glFlush();

    m_context->doneCurrent();

    // QOpenGLContext 创建于 GUI Thread，Worker 结束前恢复其 QObject Thread Affinity。
    m_context->moveToThread(m_returnThread);

    qDebug() << "ModelingGpuMeshWorker stopped.";
}

/// Worker 请求

bool ModelingGpuMeshWorker::requestWorkAndWait(WorkRequest request)
{
    QMutexLocker locker(&m_mutex);

    if (!isRunning() || !m_startSucceeded || m_failed)
    {
        qWarning() << "ModelingGpuMeshWorker requestWorkAndWait failed: Worker is unavailable.";
        return false;
    }

    if (request == WorkRequestNone)
    {
        qWarning() << "ModelingGpuMeshWorker requestWorkAndWait failed: WorkRequest is empty.";
        return false;
    }

    m_requestedWork = request;

    const unsigned long long generation = ++m_requestedGeneration;
    m_condition.wakeAll();

    // ModelingMesh Source 在这个等待期间不会被 GUI Thread 再次修改。
    // Worker 因此可以直接借用 Source 数据，而不需要把建模库本身改造成线程安全容器。
    while (m_completedGeneration < generation && !m_failed)
        m_condition.wait(&m_mutex);

    return !m_failed;
}

/// Worker 状态

void ModelingGpuMeshWorker::publishStartResult(bool succeeded)
{
    QMutexLocker locker(&m_mutex);

    m_startSucceeded = succeeded;
    m_startFinished = true;
    m_failed = !succeeded;

    m_condition.wakeAll();
}

void ModelingGpuMeshWorker::publishWorkResult(unsigned long long generation, bool succeeded)
{
    QMutexLocker locker(&m_mutex);

    if (succeeded)
    {
        m_completedGeneration = generation;
    }
    else
    {
        m_failed = true;

        qWarning() << "ModelingGpuMeshWorker GPU operation failed:"
                   << "Generation=" << static_cast<qulonglong>(generation);
    }

    m_condition.wakeAll();
}