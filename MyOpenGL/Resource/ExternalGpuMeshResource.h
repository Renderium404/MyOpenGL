#ifndef EXTERNALGPUMESHRESOURCE_H
#define EXTERNALGPUMESHRESOURCE_H

#include "Core/Resource.h"
#include "Resource/ExternalGpuMeshData.h"
#include "Resource/ExternalGpuMeshDataSource.h"
#include "Resource/RenderableObject.h"

#include <QOpenGLFunctions_3_3_Core>

#include <vector>

/// 外部 GPU Mesh 的轻量渲染包装。
/// MyOpenGL 只拥有 VAO，不拥有也不修改外部 VBO / EBO；Buffer 内容变化不会产生 MyOpenGL DirtyState。
class ExternalGpuMeshResource : public Resource, public RenderableObject
{
public:
    explicit ExternalGpuMeshResource(const QString& name = "ExternalGpuMesh");
    ~ExternalGpuMeshResource() override;

    /// 自动数据源模式
    bool setDataSource(const ExternalGpuMeshDataSource* source); // 绑定非拥有的外部 GPU DataSource，并自动观察 Structure Revision。
    const ExternalGpuMeshDataSource* dataSource() const;

    /// 手动 GPU View 模式
    bool setGpuView(const ExternalGpuMeshView& view); // 直接借用外部 GPU View；调用后解除自动 DataSource。
    const ExternalGpuMeshView& gpuView() const;

    /// Revision 调试状态
    ExternalGpuMeshRevision structureRevision() const;             // 获取当前 Resource 已观察到的外部 GPU Structure Revision。
    ExternalGpuMeshRevision synchronizedStructureRevision() const; // 获取当前 VAO 已经成功绑定的外部 GPU Structure Revision。

    /// Renderer 接口
    const QString& objectName() const override;
    bool objectInitialized() const override;
    GLuint vao() const override;
    int indexCount() const override;
    GLenum indexType() const override;
    RenderType renderType() const override;
    bool hasAttribute(GLuint location, GLint componentCount) const override;

    /// 绘制同步
    bool prepareDrawGL(QOpenGLFunctions_3_3_Core* gl) const override; // External DataSource 在 VAO 绑定前取得 GPU Read 权限。
    void finishDrawGL(QOpenGLFunctions_3_3_Core* gl) const override;  // Draw 提交后将 GPU Read 完成信息交还 External DataSource。

protected:
    /// Resource GPU 实现
    bool onPrepareSync() override;
    bool onInitializeGL(QOpenGLFunctions_3_3_Core* gl) override;
    bool onUpdateFullGL(QOpenGLFunctions_3_3_Core* gl) override;
    bool onUpdatePartialGL(QOpenGLFunctions_3_3_Core* gl) override;
    void onReleaseGL(QOpenGLFunctions_3_3_Core* gl) override;

private:
    /// GPU View 同步
    bool synchronizeGpuViewGL(QOpenGLFunctions_3_3_Core* gl);

    /// 数据验证
    bool validateGpuView(const ExternalGpuMeshView& view) const;
    bool validateGpuObjects(QOpenGLFunctions_3_3_Core* gl, const ExternalGpuMeshView& view) const;

    /// VAO State
    bool configureVAO(QOpenGLFunctions_3_3_Core* gl, const ExternalGpuMeshView& view, ExternalGpuMeshRevision revision);
    void releaseVAO(QOpenGLFunctions_3_3_Core* gl);

    /// 内部辅助
    bool isSupportedComponentType(GLenum type) const;
    bool isSupportedIndexType(GLenum type) const;

private:
    const ExternalGpuMeshDataSource* m_source;                         // 当前自动 GPU DataSource，不拥有该对象。
    ExternalGpuMeshView m_view;                                        // 当前 VAO 已经成功同步的外部 GPU Buffer 和 Vertex Layout。
    ExternalGpuMeshRevision m_observedStructureRevision;               // 当前 Resource 从 DataSource 观察到的最新 Structure Revision。
    ExternalGpuMeshRevision m_synchronizedStructureRevision;           // 当前 MyOpenGL VAO 已经成功绑定的 Structure Revision。
    GLuint m_vao;                                                       // MyOpenGL 自己拥有的 VAO。
    std::vector<GLuint> m_enabledAttributes;                            // 当前 VAO 已启用的 Attribute Locations。
};

#endif // EXTERNALGPUMESHRESOURCE_H