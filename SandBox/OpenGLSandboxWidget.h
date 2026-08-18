#ifndef OPENGLSANDBOXWIDGET_H
#define OPENGLSANDBOXWIDGET_H

#include "Viewer/OpenGLViewerWidget.h"

#include "ModelingGpuMesh.h"
#include "ModelingGpuMeshAdapter.h"
#include "ModelingGpuMeshSync.h"
#include "ModelingGpuMeshWorker.h"
#include "ModelingMesh.h"
#include "ModelingMeshAdapter.h"

class ExternalGpuGeometry;
class ExternalGeometry;
class Curve;
class Light;
class Material;
class BufferGeometry;
class BufferGeometryPickSource;
class PrimitivePickSource;
class QOffscreenSurface;
class QOpenGLContext;
class RenderItem;
class Texture;

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
    void buildExternalGeometry();
    void updateExternalRenderItemBounds(); // 根据两条 External Path 当前一致的 ModelingMesh 更新两个 Scene Item 的 Local Bounds。
    bool initializeExternalGpuGeometry();       // 创建 Shared Context Worker，在独立线程初始化 External GPU Storage。
    BufferGeometry* createCube();
    Curve* createDemoCurve();
    Texture* createCheckerTexture();

    /// Multi-Part RenderItem Test
    void buildMultiPartValidationItem();        // 创建一个 Item 下的 101 / 102 / 103 三个显式 RenderPart。
    bool cycleMultiPartValidation();            // N：通过最小 RenderPartUpdate 执行 Remove 102 -> Replace 103 -> Add 104 -> Reset。
    bool resetMultiPartValidation();            // 通过批量 RenderPartUpdate 恢复 101 / 102 / 103，并移除 104。
    const char* multiPartStateName() const;     // 返回当前 Multi-Part 测试状态名称。

    /// External Mesh A/B Test
    bool applyMatchedContentChange();                  // CPU / GPU Source 同时执行完全相同的局部建模操作。
    bool rebuildMatchedExternalMeshes();               // 根据当前 Width / Topology 状态重建两份相同 ModelingMesh。
    bool verifyModelingMeshConsistency(const char* operation) const; // 在进入两种 MyOpenGL 接入路径前验证 Modeling 数据完全一致。
    bool syncExternalGpuWorker(const char* operation); // 请求 Worker Context 更新 External GPU Storage，并等待 Write Fence 发布。
    bool replaceExternalGpuBuffers();                  // 不修改 ModelingMesh，只替换 External GPU Storage 的整套 VBO / EBO。

    /// Lighting Test
    void applyLightPreset();                // 根据 m_lightPreset 设置 Directional / Point / Spot Enabled 状态。
    const char* lightPresetName() const;    // 返回当前 SandBox 灯光预设名称。

    /// SandBox 状态
    void updateWindowTitle();

    /// External GPU 生命周期
    void shutdownExternalGpuWorker(); // 停止 Worker，并在 GUI Thread 销毁 Shared Context / Offscreen Surface。

private:
    // CPU Path 和 GPU Path 各自拥有一份 ModelingMesh，但数据结构、初始数据和所有建模操作必须完全相同。
    ModelingMesh m_modelingMesh;                       // External CPU Path 的 ModelingMesh Source。
    ModelingMeshAdapter m_modelingMeshAdapter;         // ModelingMesh → ExternalGeometry Adapter。
    ModelingMesh m_modelingGpuSourceMesh;              // External GPU Path 的同类型 ModelingMesh Source。
    ModelingGpuMesh m_modelingGpuMesh;                 // 只负责把 m_modelingGpuSourceMesh 同步到外部 VBO / EBO。
    ModelingGpuMeshSync m_modelingGpuMeshSync;         // Shared Writer Context 与 Renderer Context 的双向 GPU Fence。
    ModelingGpuMeshAdapter m_modelingGpuMeshAdapter;   // GPU Storage → ExternalGpuGeometry Adapter。
    ModelingGpuMeshWorker m_modelingGpuMeshWorker;     // 独立线程中的 External GPU Writer。
    QOpenGLContext* m_externalGpuContext;              // Worker Thread 使用的 Shared OpenGL Context。
    QOffscreenSurface* m_externalGpuSurface;           // Worker Context 使用的 Offscreen Surface，由 GUI Thread 创建/销毁。

    BufferGeometry* m_cube;                              // MyOpenGL Owned Lit Cube。
    Curve* m_curve;                                      // MyOpenGL Owned LineStrip 测试曲线。
    BufferGeometry* m_multiPartGeometry101;              // MultiPartItem Part 101 基准 Geometry。
    BufferGeometry* m_multiPartGeometry102;              // MultiPartItem Part 102 基准 Geometry。
    BufferGeometry* m_multiPartGeometry103;              // MultiPartItem Part 103 基准 Geometry。
    BufferGeometry* m_multiPartGeometry103Replacement;   // Part 103 Replace 测试 Geometry，预注册以隔离 Part 结构测试。
    BufferGeometry* m_multiPartGeometry104;              // Part 104 Add 测试 Geometry，预注册以隔离 Part 结构测试。
    ExternalGeometry* m_externalGeometry;              // External CPU ModelingMesh 对应的 MyOpenGL Geometry GPU Cache。
    ExternalGpuGeometry* m_externalGpuGeometry;        // External GPU Storage 对应的 MyOpenGL Geometry VAO。
    Texture* m_cubeTexture;                    // 测试 Diffuse Texture。
    Material* m_cubeMaterial;                          // 三种 Triangle Geometry 共用的 Lit Material。
    Material* m_curveMaterial;                         // Curve 使用的 VertexColor Material。
    Light* m_sun;                                      // 主方向光。
    Light* m_fillLight;                                // 弱方向补光，用于保留背光面形体。
    Light* m_pointLight;                               // Point Diffuse 测试光。
    Light* m_spotLight;                                // Spot Diffuse / Cone Attenuation 测试光。
    ResourceId m_cubeTextureId;                        // Diffuse Texture ResourceId。

    RenderItem* m_cubeItem;                            // MyOpenGL Owned Cube Scene Item。
    RenderItem* m_curveItem;                           // MyOpenGL Curve Scene Item。
    RenderItem* m_multiPartItem;                       // 显式 Multi-Part Scene Item；所有 Part 仍属于同一个用户对象。
    RenderItem* m_externalGeometryItem;                    // External CPU Geometry Scene Item。
    RenderItem* m_externalGpuGeometryItem;                 // External GPU Geometry Scene Item。

    BufferGeometryPickSource* m_cubePrimitivePicker;  // Owned Cube CPU Geometry Picker，SandBox 拥有 Adapter。
    BufferGeometryPickSource* m_curvePrimitivePicker; // Curve CPU Geometry Picker，SandBox 拥有 Adapter。
    BufferGeometryPickSource* m_multiPartPicker101;   // Part 101 Geometry Picker，SandBox 拥有 Adapter。
    BufferGeometryPickSource* m_multiPartPicker102;   // Part 102 Geometry Picker，SandBox 拥有 Adapter。
    BufferGeometryPickSource* m_multiPartPicker103;   // Part 103 基准 Geometry Picker，SandBox 拥有 Adapter。
    BufferGeometryPickSource* m_multiPartPicker103Replacement; // Part 103 Replace Geometry Picker。
    BufferGeometryPickSource* m_multiPartPicker104;   // Part 104 Add Geometry Picker。
    PrimitivePickSource* m_externalCpuPrimitivePicker;      // External CPU ModelingMesh Primitive Picker，SandBox 拥有 Adapter。
    PrimitivePickSource* m_externalGpuPrimitivePicker;      // External GPU Path 使用同源 ModelingMesh CPU Picker，不做 GPU Readback。

    bool m_externalWide;                               // 两条 External Path 当前是否使用 Wide Geometry。
    bool m_externalSplit;                              // 两条 External Path 当前是否使用 Split Quad Topology。
    int m_lightPreset;                                 // L 键灯光预设：0 Main，1 Main+Fill，2 +Point，3 All，4 Off。
    int m_multiPartState;                               // N 键状态：0 Base，1 Remove102，2 Replace103，3 Add104。
};

#endif // OPENGLSANDBOXWIDGET_H