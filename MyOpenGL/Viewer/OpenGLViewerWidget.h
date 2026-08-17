#ifndef OPENGLVIEWERWIDGET_H
#define OPENGLVIEWERWIDGET_H

#include <QOpenGLWidget>
#include <QPoint>

#include <vector>

#include "Camera/CameraManager.h"
#include "Core/ResourceManager.h"
#include "Light/LightManager.h"
#include "Material/MaterialManager.h"
#include "Render/RenderContext.h"
#include "Render/Renderer.h"
#include "Scene/Scene.h"

class CoordinateSystemResource;
class GridPlaneResource;
class MeshResource;
class QKeyEvent;
class QMouseEvent;
class QWheelEvent;
class RenderItem;
class ViewNavigationResource;

/// 可复用 OpenGL 可视化窗口。
/// 负责通用 Viewer 生命周期、Scene/Manager、Camera 导航、Viewer 内部辅助显示以及 Object / Primitive Picking 与 Highlight。
/// Scene / RenderItem 只承载用户可操作对象；Grid、Axis、Camera Target、Highlight、ViewNavigation 不进入 Item 体系。
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

    /// Picking Candidates
    const RenderItemCandidates& pickCandidates() const;
    bool addPickCandidate(RenderItem* item);       // 将属于当前 Scene 的明确 Item 加入 Viewer Picking Candidate 集合。
    bool removePickCandidate(RenderItem* item);    // 仅移除 Candidate 引用，不删除 Scene Item。
    void clearPickCandidates();

protected:
    /// 派生内容扩展
    virtual bool initializeViewerContentGL(QOpenGLFunctions_3_3_Core* gl); // Renderer Context ready 后初始化派生 GL/Shared-Context 内容；默认无需额外处理。
    virtual void buildViewerContentItems();                                // 创建用户可操作的业务 RenderItem；这些 Item 才进入 Scene，默认不创建。
    virtual bool handleViewerKeyPress(QKeyEvent* event);                   // 处理 Viewer 未消费的业务快捷键；处理后返回 true。
    virtual void viewerStateChanged();                                     // Camera、Picking 交互状态、辅助显示状态变化后的通知；默认不处理。
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

    /// Picking / 当前交互状态
    bool pickAt(const QPoint& position);               // Left Click 优先精确 Primitive Picking，再回退到没有 Primitive Picker 的 Bounds Picking。
    RenderItem* pickedItem();                          // 最近一次成功 Picking 命中的 Item；没有命中时为 0。
    const RenderItem* pickedItem() const;
    void clearPickedItem();                            // 只清除 Viewer Picking 状态，不隐式修改 Highlight。

    /// Highlight
    bool showBoundsHighlight(const AxisAlignedBoundingBox& bounds); // 显示明确 World Bounds 的黄色 AABB，不查询 Picking 状态。
    void clearBoundsHighlight();
    bool showPrimitiveHighlight(const QVector3D& vertex0, const QVector3D& vertex1, const QVector3D& vertex2); // 显示明确 World Triangle 的橙色边线，不查询 Picking 状态。
    void clearPrimitiveHighlight();

    /// Camera View Bounds
    bool focusPoint(const QVector3D& worldPoint);                         // 平移当前 View Bounds，使其 Center 移到指定世界点。
    bool focusBounds(const AxisAlignedBoundingBox& bounds);               // 用指定 Bounds 替换当前 View Bounds，并保持当前方向和距离。
    bool focusItem(const RenderItem* item);                               // 用指定 Item 的 World Bounds 替换当前 View Bounds。
    bool focusPrimitive(const ScenePrimitiveHit& hit);                    // 平移当前 View Bounds，使其 Center 移到指定 Render Triangle 几何中心。
    bool fitBoundsToView(const AxisAlignedBoundingBox& bounds, float margin = 1.15f); // 用指定 Bounds 替换当前 View Bounds，并按当前方向完整 Fit。
    bool fitItemToView(const RenderItem* item, float margin = 1.15f);      // 用指定 Item 的 World Bounds 替换当前 View Bounds，并完整 Fit。
    bool fitPrimitiveToView(const ScenePrimitiveHit& hit, float margin = 1.15f); // 用指定 Render Triangle Bounds 替换当前 View Bounds，并完整 Fit。

    /// Camera 交互
    void resetCamera();                                                   // 恢复默认 View Bounds、默认观察方向和透视参数。
    bool fitSceneToView();                                                // H：用当前可见 Scene Bounds 替换 View Bounds，并完整 Fit。
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
    bool setPickedItem(RenderItem* item);              // 设置明确 Viewer Picking 状态；Item 必须属于当前 Scene。
    MeshResource* createCameraTargetMarker();
    MeshResource* createBoundsHighlightMesh();
    MeshResource* createPrimitiveHighlightMesh();

private:
    RenderContext m_renderContext;                     // 当前 Widget OpenGL 3.3 Core 函数访问器。
    Renderer m_renderer;                               // 当前 Viewer Renderer。
    ResourceManager m_resourceManager;                 // 当前 Viewer 与业务 Scene 共用的 GPU ResourceManager。
    CameraManager m_cameraManager;                     // 当前 Viewer Cameras。
    LightManager m_lightManager;                       // 当前 Viewer Lights。
    MaterialManager m_materialManager;                 // 当前 Viewer Materials。
    Scene m_scene;                                     // 用户操作 Scene，只拥有业务 RenderItem；Viewer 内部辅助模型不进入 Scene。
    RenderItemCandidates m_pickCandidates;             // 当前 Viewer 明确允许参与鼠标 Picking 的 RenderItem 集合。
    RenderItem* m_pickedItem;                          // 最近一次成功 Picking 命中的 Scene Item；Viewer 只借用，不拥有。

    GridPlaneResource* m_grid;                         // XZ 世界参考网格。
    CoordinateSystemResource* m_axes;                  // RGB 世界坐标轴。
    ViewNavigationResource* m_viewNavigation;          // 右上角方向导航几何。
    MeshResource* m_cameraTargetMarker;                // Camera Orbit Target 黄色十字。
    MeshResource* m_boundsHighlight;                   // 显式 World Bounds 黄色 AABB Highlight。
    MeshResource* m_primitiveHighlight;                // 显式 World Triangle 橙色边线 Highlight。

    QPoint m_lastMousePosition;                        // 上一次鼠标位置。
    QPoint m_leftPressPosition;                        // 当前 Left Button Press 起始位置，用于区分 Click Picking 与 Drag Orbit。
    bool m_leftDragOccurred;                           // Left Button 按下后是否已经发生超过阈值的 Orbit Drag。
    bool m_glReady;                                    // 当前 Viewer OpenGL Scene 是否完整初始化。
    bool m_releasePerformed;                           // 当前 Context 生命周期是否已经执行 Viewer GPU Cleanup。
    bool m_showGrid;                                   // 是否显示 Grid。
    bool m_showAxes;                                   // 是否显示世界坐标轴。
    bool m_showViewNavigation;                         // 是否显示 View Navigation。
    bool m_showCameraTarget;                           // 是否显示 Camera Target。
    bool m_showBoundsHighlight;                       // 是否显示当前明确 Bounds Highlight。
    bool m_showPrimitiveHighlight;                    // 是否显示当前明确 Primitive Highlight。
};

#endif // OPENGLVIEWERWIDGET_H