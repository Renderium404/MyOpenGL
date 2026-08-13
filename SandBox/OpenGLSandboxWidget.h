#ifndef OPENGLSANDBOXWIDGET_H
#define OPENGLSANDBOXWIDGET_H

#include <QOpenGLWidget>
#include <QPoint>

#include "ModelingMesh.h"
#include "ModelingMeshAdapter.h"

#include "Camera/CameraManager.h"
#include "Core/ResourceManager.h"
#include "Light/LightManager.h"
#include "Material/MaterialManager.h"
#include "Render/RenderContext.h"
#include "Render/Renderer.h"

class CoordinateSystemResource;
class ExternalMeshResource;
class GridPlaneResource;
class Material;
class MeshResource;
class TextureResource;
class ViewNavigationResource;

class QKeyEvent;
class QMouseEvent;
class QWheelEvent;

/// OpenGL 可视化调试窗口。
/// 同时验证 Owned Resource 和外部建模 Mesh 的自动 Revision GPU Synchronization。
class OpenGLSandboxWidget : public QOpenGLWidget
{
public:
    explicit OpenGLSandboxWidget(QWidget* parent = 0);
    ~OpenGLSandboxWidget() override;

protected:
    /// OpenGL 生命周期
    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;

    /// 输入
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    /// Scene 创建
    void buildScene();
    void buildExternalMesh();
    MeshResource* createCube();
    MeshResource* createCameraTargetMarker();
    TextureResource* createCheckerTexture();

    /// Scene 状态
    void resetCamera();
    void updateWindowTitle();

    /// GPU 生命周期
    void releaseGL();

private:
    // 外部 Modeling 数据和 Adapter 必须比 ResourceManager 活得更久，因为 ExternalMeshResource 只保存借用指针。
    ModelingMesh m_modelingMesh;                     // 模拟完全独立的建模库 Mesh。
    ModelingMeshAdapter m_modelingMeshAdapter;       // Application 层 ModelingMesh → MyOpenGL Adapter。

    RenderContext m_renderContext;                   // 当前 Widget OpenGL 3.3 Core 函数访问器。
    Renderer m_renderer;                             // 当前 SandBox Renderer。
    ResourceManager m_resourceManager;               // 当前 Scene GPU Resources。
    CameraManager m_cameraManager;                   // 当前 Scene Cameras。
    LightManager m_lightManager;                     // 当前 Scene Lights。
    MaterialManager m_materialManager;               // 当前 Scene Materials。

    GridPlaneResource* m_grid;                       // XZ 世界参考网格。
    CoordinateSystemResource* m_axes;                // RGB 世界坐标轴。
    ViewNavigationResource* m_viewNavigation;        // 右上角方向导航几何。
    MeshResource* m_cameraTargetMarker;              // Camera Orbit Target 黄色十字。
    MeshResource* m_cube;                            // MyOpenGL Owned Lit Cube。
    ExternalMeshResource* m_externalMesh;            // 外部 ModelingMesh 对应的 GPU Cache。
    TextureResource* m_cubeTexture;                  // 测试 Diffuse Texture。
    Material* m_cubeMaterial;                        // Cube 和 External Mesh 共用的 Lit Material。
    Light* m_sun;                                    // 主方向光。
    ResourceId m_cubeTextureId;                      // Diffuse Texture ResourceId。

    QPoint m_lastMousePosition;                      // 上一次鼠标位置。
    bool m_glReady;                                  // 当前 OpenGL Scene 是否完整初始化。
    bool m_showGrid;                                 // 是否显示 Grid。
    bool m_showAxes;                                 // 是否显示世界坐标轴。
    bool m_showViewNavigation;                       // 是否显示 View Navigation。
    bool m_showCameraTarget;                         // 是否显示 Camera Target。
    bool m_externalWide;                             // 外部 Mesh Structure 测试用宽度状态。
};

#endif // OPENGLSANDBOXWIDGET_H