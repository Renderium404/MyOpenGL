#ifndef OPENGLVIEWERWIDGET_H
#define OPENGLVIEWERWIDGET_H

#include <QOpenGLWidget>
#include <QPoint>

#include "Camera/CameraManager.h"
#include "Core/ResourceManager.h"
#include "Light/LightManager.h"
#include "Material/MaterialManager.h"
#include "Render/RenderContext.h"
#include "Render/Renderer.h"
#include "Scene/Scene.h"

class CoordinateSystemResource;
class GridPlaneResource;
class Material;
class MeshResource;
class QKeyEvent;
class QMouseEvent;
class QWheelEvent;
class RenderItem;
class ViewNavigationResource;

/// 可复用 OpenGL 可视化窗口。
/// 负责通用 Viewer 生命周期、Scene/Manager、Camera 导航、辅助显示以及 Object / Primitive Selection。
/// 业务模型、外部建模库 Adapter、Worker/Fence 和功能测试由派生窗口提供。
class OpenGLViewerWidget : public QOpenGLWidget
{
public:
    explicit OpenGLViewerWidget(QWidget* parent = 0);
    ~OpenGLViewerWidget() override;

    /// Framework 状态
    ResourceManager& resourceManager();
    const ResourceManager& resourceManager() const;
    CameraManager& cameraManager();
    const CameraManager& cameraManager() const;
    LightManager& lightManager();
    const LightManager& lightManager() const;
    MaterialManager& materialManager();
    const MaterialManager& materialManager() const;
    Scene& scene();
    const Scene& scene() const;
    bool viewerGLReady() const;

    /// Viewer 显示状态
    bool gridVisible() const;
    bool axesVisible() const;
    bool viewNavigationVisible() const;
    bool cameraTargetVisible() const;
    void setGridVisible(bool visible);
    void setAxesVisible(bool visible);
    void setViewNavigationVisible(bool visible);
    void setCameraTargetVisible(bool visible);

protected:
    /// 派生内容扩展
    virtual bool initializeViewerContentGL(QOpenGLFunctions_3_3_Core* gl); // Renderer Context ready 后初始化派生 GL/Shared-Context 内容；默认无需额外处理。
    virtual void buildViewerContentItems();                                // 在 Grid/Axis 之后、Overlay 之前创建业务 RenderItem；默认不创建。
    virtual bool handleViewerKeyPress(QKeyEvent* event);                   // 处理 Viewer 未消费的业务快捷键；处理后返回 true。
    virtual void viewerStateChanged();                                     // Camera、Selection、辅助显示状态变化后的通知；默认不处理。
    virtual void afterViewerGLReleased();                                  // Viewer Resource 已释放后清理派生 Shared GPU 状态；默认不处理。

    /// OpenGL 生命周期
    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;
    void releaseViewerGL(); // 派生类若拥有 Shared Context/外部 GPU 状态，应在派生析构开始时调用，保证虚函数清理仍可分派。

    /// 输入
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

    /// Selection
    bool selectObjectAt(const QPoint& position);       // Left Click 优先精确 Primitive Picking，再回退到没有 Primitive Picker 的 Bounds Picking。
    void clearObjectSelection();
    void refreshSelectionBounds();                     // 当前选中对象 Bounds 变化后刷新黄色 Object Overlay。
    void clearPrimitiveSelectionVisual();              // Geometry/Topology 变化后清除旧 Primitive Overlay，但保留 Object Selection。

    /// Camera
    void resetCamera();
    bool fitSceneToView();                              // H：保持当前朝向，将全部可见 Scene Bounds 完整收入视野。
    bool fitSelectionToView();                          // F：保持当前朝向，将当前选中 Item 的 World Bounds 完整收入视野。
    void logCameraView(const char* viewName) const;

    /// 低层访问
    RenderContext& renderContext();
    const RenderContext& renderContext() const;
    Renderer& renderer();
    const Renderer& renderer() const;

private:
    /// Viewer 创建
    void buildViewerResources();
    void rebuildViewerSceneItems();
    MeshResource* createCameraTargetMarker();
    MeshResource* createSelectionBoundsMesh();
    MeshResource* createSelectionPrimitiveMesh();

    /// Selection Overlay
    void updateSelectionBoundsMesh();
    void updateSelectionPrimitiveMesh(const ScenePrimitiveHit& hit);

private:
    RenderContext m_renderContext;                     // 当前 Widget OpenGL 3.3 Core 函数访问器。
    Renderer m_renderer;                               // 当前 Viewer Renderer。
    ResourceManager m_resourceManager;                 // 当前 Viewer 与业务 Scene 共用的 GPU ResourceManager。
    CameraManager m_cameraManager;                     // 当前 Viewer Cameras。
    LightManager m_lightManager;                       // 当前 Viewer Lights。
    MaterialManager m_materialManager;                 // 当前 Viewer Materials。
    Scene m_scene;                                     // 当前扁平 Scene，拥有通用与业务 RenderItem。

    GridPlaneResource* m_grid;                         // XZ 世界参考网格。
    CoordinateSystemResource* m_axes;                  // RGB 世界坐标轴。
    ViewNavigationResource* m_viewNavigation;          // 右上角方向导航几何。
    MeshResource* m_cameraTargetMarker;                // Camera Orbit Target 黄色十字。
    MeshResource* m_selectionBounds;                   // 当前 Selection 的黄色 World AABB Overlay。
    MeshResource* m_selectionPrimitive;                // 当前精确 Triangle Selection 的橙色 World Triangle Overlay。
    Material* m_vertexColorMaterial;                   // Grid / Axis / Target / Selection Overlay 共用 Vertex Color Material。

    RenderItem* m_axesItem;                            // 世界坐标轴 Scene Item。
    RenderItem* m_gridItem;                            // 世界 Grid Scene Item。
    RenderItem* m_cameraTargetItem;                    // Camera Target Marker Scene Item。
    RenderItem* m_selectionBoundsItem;                 // Selection AABB Overlay Scene Item，不参与自身 Picking / Fit。
    RenderItem* m_selectionPrimitiveItem;              // Primitive Triangle Overlay Scene Item，不参与自身 Picking / Fit。

    QPoint m_lastMousePosition;                        // 上一次鼠标位置。
    QPoint m_leftPressPosition;                        // 当前 Left Button Press 起始位置，用于区分 Click Selection 与 Drag Orbit。
    bool m_leftDragOccurred;                           // Left Button 按下后是否已经发生超过阈值的 Orbit Drag。
    bool m_glReady;                                    // 当前 Viewer OpenGL Scene 是否完整初始化。
    bool m_releasePerformed;                           // 当前 Context 生命周期是否已经执行 Viewer GPU Cleanup。
    bool m_showGrid;                                   // 是否显示 Grid。
    bool m_showAxes;                                   // 是否显示世界坐标轴。
    bool m_showViewNavigation;                         // 是否显示 View Navigation。
    bool m_showCameraTarget;                           // 是否显示 Camera Target。
};

#endif // OPENGLVIEWERWIDGET_H
