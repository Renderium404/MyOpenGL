#include "ModelingGpuMeshSync.h"

#include <QDebug>
#include <QMutexLocker>

ModelingGpuMeshSync::ModelingGpuMeshSync()
    : m_externalWriteFence(0)
    , m_rendererReadFence(0)
    , m_synchronizedStructureRevision(0)
{
}

ModelingGpuMeshSync::~ModelingGpuMeshSync()
{
    QMutexLocker locker(&m_mutex);

    if (m_externalWriteFence != 0 || m_rendererReadFence != 0)
        qWarning() << "ModelingGpuMeshSync destroyed while GLsync objects are still pending.";
}

/// External Writer -> Renderer

bool ModelingGpuMeshSync::publishExternalWriteCompleteGL(QOpenGLFunctions_3_3_Core* gl)
{
    if (gl == 0)
    {
        qWarning() << "ModelingGpuMeshSync publishExternalWriteCompleteGL failed: OpenGL functions are null.";
        return false;
    }

    GLsync fence = gl->glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    if (fence == 0)
    {
        qWarning() << "ModelingGpuMeshSync publishExternalWriteCompleteGL failed: glFenceSync returned null.";
        return false;
    }

    // Fence 属于另一个 Context 的 Command Stream。
    // 必须由产生 Fence 的 Writer Context 主动 Flush，Renderer 的 WaitSync 才能可靠等待该 Fence。
    gl->glFlush();

    replaceExternalWriteFence(gl, fence);

    qDebug() << "ModelingGpuMeshSync External Write Fence published.";

    return true;
}

bool ModelingGpuMeshSync::waitExternalWriteCompleteGL(QOpenGLFunctions_3_3_Core* gl)
{
    if (gl == 0)
    {
        qWarning() << "ModelingGpuMeshSync waitExternalWriteCompleteGL failed: OpenGL functions are null.";
        return false;
    }

    GLsync fence = takeExternalWriteFence();

    if (fence == 0)
        return true;

    if (gl->glIsSync(fence) != GL_TRUE)
    {
        qWarning() << "ModelingGpuMeshSync waitExternalWriteCompleteGL failed: shared Write Fence is invalid.";
        return false;
    }

    // WaitSync 不阻塞调用线程，而是在 Renderer GL Command Stream 中建立 GPU Server Wait。
    // Renderer 随后的 Buffer Re-attach / VAO Bind / Draw 必须排在这个 Wait 之后。
    gl->glWaitSync(fence, 0, GL_TIMEOUT_IGNORED);
    gl->glDeleteSync(fence);

    qDebug() << "ModelingGpuMeshSync Renderer queued WaitSync for External Write Fence.";

    return true;
}

/// Renderer -> External Writer

bool ModelingGpuMeshSync::publishRendererReadCompleteGL(QOpenGLFunctions_3_3_Core* gl)
{
    if (gl == 0)
    {
        qWarning() << "ModelingGpuMeshSync publishRendererReadCompleteGL failed: OpenGL functions are null.";
        return false;
    }

    GLsync fence = gl->glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    if (fence == 0)
    {
        qWarning() << "ModelingGpuMeshSync publishRendererReadCompleteGL failed: glFenceSync returned null.";
        return false;
    }

    // External Writer 将在另一个 Context 等待该 Fence，因此 Renderer Context 必须主动 Flush。
    gl->glFlush();

    replaceRendererReadFence(gl, fence);

    // Renderer 每帧都会发布 Read Fence，正常运行时不能让调试输出被逐帧日志淹没。
    return true;
}

bool ModelingGpuMeshSync::waitRendererReadCompleteGL(QOpenGLFunctions_3_3_Core* gl)
{
    if (gl == 0)
    {
        qWarning() << "ModelingGpuMeshSync waitRendererReadCompleteGL failed: OpenGL functions are null.";
        return false;
    }

    GLsync fence = takeRendererReadFence();

    if (fence == 0)
        return true;

    if (gl->glIsSync(fence) != GL_TRUE)
    {
        qWarning() << "ModelingGpuMeshSync waitRendererReadCompleteGL failed: shared Read Fence is invalid.";
        return false;
    }

    // Writer 后续的 glBufferData / glBufferSubData 排在 WaitSync 之后，
    // 因此不会覆盖 Renderer 尚未读取完成的共享 Buffer。
    gl->glWaitSync(fence, 0, GL_TIMEOUT_IGNORED);
    gl->glDeleteSync(fence);

    qDebug() << "ModelingGpuMeshSync External Writer queued WaitSync for Renderer Read Fence.";

    return true;
}

/// Structure Synchronization

void ModelingGpuMeshSync::acknowledgeStructureRevision(unsigned long long revision)
{
    bool changed = false;

    {
        QMutexLocker locker(&m_mutex);

        if (revision > m_synchronizedStructureRevision)
        {
            m_synchronizedStructureRevision = revision;
            changed = true;
        }
    }

    // Structure Revision 只在 VAO 真正切换时变化，因此这里属于低频事件日志。
    if (changed)
    {
        qDebug() << "ModelingGpuMeshSync Renderer synchronized GPU Structure Revision:"
                 << static_cast<qulonglong>(revision);
    }
}

unsigned long long ModelingGpuMeshSync::synchronizedStructureRevision()
{
    QMutexLocker locker(&m_mutex);
    return m_synchronizedStructureRevision;
}

/// GPU 生命周期

void ModelingGpuMeshSync::releaseGL(QOpenGLFunctions_3_3_Core* gl)
{
    if (gl == 0)
        return;

    GLsync writeFence = takeExternalWriteFence();
    GLsync readFence = takeRendererReadFence();

    if (writeFence != 0)
        gl->glDeleteSync(writeFence);

    if (readFence != 0)
        gl->glDeleteSync(readFence);

    QMutexLocker locker(&m_mutex);
    m_synchronizedStructureRevision = 0;
}

/// Fence Transfer

GLsync ModelingGpuMeshSync::takeExternalWriteFence()
{
    QMutexLocker locker(&m_mutex);

    GLsync fence = m_externalWriteFence;
    m_externalWriteFence = 0;

    return fence;
}

GLsync ModelingGpuMeshSync::takeRendererReadFence()
{
    QMutexLocker locker(&m_mutex);

    GLsync fence = m_rendererReadFence;
    m_rendererReadFence = 0;

    return fence;
}

void ModelingGpuMeshSync::replaceExternalWriteFence(QOpenGLFunctions_3_3_Core* gl, GLsync fence)
{
    GLsync previousFence = 0;

    {
        QMutexLocker locker(&m_mutex);

        previousFence = m_externalWriteFence;
        m_externalWriteFence = fence;
    }

    // 如果 Renderer 还没有消费旧 Fence，而 Writer 已经产生更新的 Fence，
    // 等待最新 Fence 已经包含同一 Writer Command Stream 中更早的所有写操作。
    if (previousFence != 0)
        gl->glDeleteSync(previousFence);
}

void ModelingGpuMeshSync::replaceRendererReadFence(QOpenGLFunctions_3_3_Core* gl, GLsync fence)
{
    GLsync previousFence = 0;

    {
        QMutexLocker locker(&m_mutex);

        previousFence = m_rendererReadFence;
        m_rendererReadFence = fence;
    }

    // 同一 Renderer Command Stream 中后产生的 Fence 已经覆盖此前所有 Draw，
    // 因此尚未被 Writer 消费的旧 Fence 可以被最新 Fence 取代。
    if (previousFence != 0)
        gl->glDeleteSync(previousFence);
}