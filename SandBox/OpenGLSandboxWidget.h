#ifndef OPENGLSANDBOXWIDGET_H
#define OPENGLSANDBOXWIDGET_H

#include "Viewer/OpenGLViewerWidget.h"

#include "ModelingGpuMesh.h"
#include "ModelingGpuMeshAdapter.h"
#include "ModelingGpuMeshSync.h"
#include "ModelingGpuMeshWorker.h"
#include "ModelingMesh.h"
#include "ModelingMeshAdapter.h"

class ExternalGpuMeshResource;
class ExternalMeshResource;
class Light;
class Material;
class MeshResource;
class MeshResourcePrimitivePickSource;
class PrimitivePickSource;
class QOffscreenSurface;
class QOpenGLContext;
class RenderItem;
class TextureResource;

/// OpenGL 框架功能验证窗口。
/// 只保留 SandBox 测试模型、External CPU/GPU A/B 数据路径、Worker/Fence 和故障注入；
/// 通用 Viewer 生命周期、Camera、Viewer 内部辅助显示、Picking 交互状态与 Highlight 由 OpenGLViewerWidget 提供。
class OpenGLSandboxWidget : public OpenGLViewerWidget
{
public:
    explicit OpenGLSandboxWidget(QWidget* parent = 0);
    ~OpenGLSandboxWidget() override;

protected:
    /// Viewer 扩展
    bool initializeViewerContentGL(QOpenGLFunctions_3_3_Core* gl) override;
    void buildViewerContentItems() override;
    bool handleViewerKeyPress(QKeyEvent* event) override;
    void viewerStateChanged() override;
    void afterViewerGLReleased() override;

private:
    /// SandBox Scene 创建
    void buildSandboxContent();
    void buildExternalMesh();
    void updateExternalRenderItemBounds(); // 根据两条 External Path 当前一致的 ModelingMesh 更新两个 Scene Item 的 Local Bounds。
    bool initializeExternalGpuMesh();       // 创建 Shared Context Worker，在独立线程初始化 External GPU Storage。
    MeshResource* createCube();
    TextureResource* createCheckerTexture();

    /// External Mesh A/B Test
    bool applyMatchedContentChange();                  // CPU / GPU Source 同时执行完全相同的局部建模操作。
    bool rebuildMatchedExternalMeshes();               // 根据当前 Width / Topology 状态重建两份相同 ModelingMesh。
    bool verifyModelingMeshConsistency(const char* operation) const; // 在进入两种 MyOpenGL 接入路径前验证 Modeling 数据完全一致。
    bool syncExternalGpuWorker(const char* operation); // 请求 Worker Context 更新 External GPU Storage，并等待 Write Fence 发布。
    bool replaceExternalGpuBuffers();                  // 不修改 ModelingMesh，只替换 External GPU Storage 的整套 VBO / EBO。

    /// SandBox 状态
    void updateWindowTitle();

    /// External GPU 生命周期
    void shutdownExternalGpuWorker(); // 停止 Worker，并在 GUI Thread 销毁 Shared Context / Offscreen Surface。

private:
    // CPU Path 和 GPU Path 各自拥有一份 ModelingMesh，但数据结构、初始数据和所有建模操作必须完全相同。
    ModelingMesh m_modelingMesh;                       // External CPU Path 的 ModelingMesh Source。
    ModelingMeshAdapter m_modelingMeshAdapter;         // ModelingMesh → ExternalMeshResource Adapter。
    ModelingMesh m_modelingGpuSourceMesh;              // External GPU Path 的同类型 ModelingMesh Source。
    ModelingGpuMesh m_modelingGpuMesh;                 // 只负责把 m_modelingGpuSourceMesh 同步到外部 VBO / EBO。
    ModelingGpuMeshSync m_modelingGpuMeshSync;         // Shared Writer Context 与 Renderer Context 的双向 GPU Fence。
    ModelingGpuMeshAdapter m_modelingGpuMeshAdapter;   // GPU Storage → ExternalGpuMeshResource Adapter。
    ModelingGpuMeshWorker m_modelingGpuMeshWorker;     // 独立线程中的 External GPU Writer。
    QOpenGLContext* m_externalGpuContext;              // Worker Thread 使用的 Shared OpenGL Context。
    QOffscreenSurface* m_externalGpuSurface;           // Worker Context 使用的 Offscreen Surface，由 GUI Thread 创建/销毁。

    MeshResource* m_cube;                              // MyOpenGL Owned Lit Cube。
    ExternalMeshResource* m_externalMesh;              // External CPU ModelingMesh 对应的 MyOpenGL GPU Cache。
    ExternalGpuMeshResource* m_externalGpuMesh;        // External GPU Storage 对应的 MyOpenGL VAO Resource。
    TextureResource* m_cubeTexture;                    // 测试 Diffuse Texture。
    Material* m_cubeMaterial;                          // 三种 Mesh 共用的 Lit Material。
    Light* m_sun;                                      // 主方向光。
    ResourceId m_cubeTextureId;                        // Diffuse Texture ResourceId。

    RenderItem* m_cubeItem;                            // MyOpenGL Owned Cube Scene Item。
    RenderItem* m_externalMeshItem;                    // External CPU Mesh Scene Item。
    RenderItem* m_externalGpuMeshItem;                 // External GPU Mesh Scene Item。

    MeshResourcePrimitivePickSource* m_cubePrimitivePicker; // Owned Cube CPU Geometry Picker，SandBox 拥有 Adapter。
    PrimitivePickSource* m_externalCpuPrimitivePicker;      // External CPU ModelingMesh Primitive Picker，SandBox 拥有 Adapter。
    PrimitivePickSource* m_externalGpuPrimitivePicker;      // External GPU Path 使用同源 ModelingMesh CPU Picker，不做 GPU Readback。

    bool m_externalWide;                               // 两条 External Path 当前是否使用 Wide Geometry。
    bool m_externalSplit;                              // 两条 External Path 当前是否使用 Split Quad Topology。
};

#endif // OPENGLSANDBOXWIDGET_H