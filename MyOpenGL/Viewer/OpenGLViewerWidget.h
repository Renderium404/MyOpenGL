#ifndef OPENGLVIEWERWIDGET_H
#define OPENGLVIEWERWIDGET_H

#include <QOpenGLWidget>
#include <QPoint>

#include <vector>

#include "MyOpenGL/Camera/CameraManager.h"
#include "MyOpenGL/Core/ResourceManager.h"
#include "MyOpenGL/Light/LightManager.h"
#include "MyOpenGL/Material/MaterialManager.h"
#include "MyOpenGL/Render/RenderContext.h"
#include "MyOpenGL/Render/Renderer.h"
#include "MyOpenGL/Scene/Scene.h"

class CoordinateSystemGeometry;
class GridPlaneGeometry;
class BufferGeometry;
class QLabel;
class QKeyEvent;
class QMouseEvent;
class QWheelEvent;
class RenderItem;
class ViewNavigationGeometry;

/// Viewer 右键 Picking 模式。
/// 只决定 Viewer 当前把一次右键点击解释为 Point、Triangle 还是 Item 查询；Scene API 仍然接受明确查询类型。
enum ViewerPickMode
{
    ViewerPickModePoint,    // 只捕捉 Geometry Vertex / Endpoint；命中后推进 A-B Measurement。
    ViewerPickModeTriangle, // 只选择 Triangle Primitive；不推进 Measurement。
    ViewerPickModeItem      // 只选择 RenderItem Bounds；不推进 Measurement。
};

/// 获取 Viewer Picking 模式的调试名称。
const char* viewerPickModeName(ViewerPickMode mode);

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

    /// Item 显示状态
    bool setItemVisible(RenderItem* item, bool visible); // 设置明确 Scene Item 的显示状态；隐藏当前 Picked Item 时同步清除 Viewer Pick / Highlight。
    bool setItemsVisible(const std::vector<RenderItem*>& items, bool visible); // 批量设置明确 Scene Item；全部验证通过后才执行修改。
    void setAllItemsVisible(bool visible); // 设置当前 Scene 全部用户 Item 的显示状态；不影响 Viewer 内部辅助几何。
    bool isolateItems(const std::vector<RenderItem*>& items); // 只显示明确 Item 集合，其余 Scene Item 隐藏；集合不能为空。

    /// Measurement
    void setMeasurementPointA(const QVector3D& point); // 设置明确 World Point A，并开始一组新测量；已有 Point B 会被清除。
    bool setMeasurementPointB(const QVector3D& point); // 设置明确 World Point B；必须先存在 Point A。
    bool measurementPointA(QVector3D& point) const;    // 返回当前 Point A；不存在时返回 false。
    bool measurementPointB(QVector3D& point) const;    // 返回当前 Point B；不存在时返回 false。
    float measurementLength() const;                   // 返回 A-B 世界空间长度；测量未完成时返回 0。
    void clearMeasurement();                           // 清除 A/B、测量线和文字状态。

    /// Picking Mode
    ViewerPickMode pickMode() const;
    bool setPickMode(ViewerPickMode mode); // 设置明确右键 Picking 模式；切换时清除旧 Pick / Highlight，但保留已有 Measurement。
    void cyclePickMode();                   // Point -> Triangle -> Item -> Point。

    /// Picking Candidates
    const RenderItemCandidates& pickCandidates() const;
    bool addPickCandidate(RenderItem* item);       // 将属于当前 Scene 的明确 Item 加入 Viewer Picking Candidate 集合。
    bool removePickCandidate(RenderItem* item);    // 仅移除 Candidate 引用，不删除 Scene Item。
    void clearPickCandidates();
    float primitivePickTolerance() const;
    bool setPrimitivePickTolerance(float pixels); // 设置 Line 等屏幕空间 Primitive 的拾取容差；单位 Pixel，必须大于 0。
    float pointPickTolerance() const;
    bool setPointPickTolerance(float pixels);     // 设置 Measurement Vertex / Endpoint Snap 容差；单位 Pixel，必须大于 0。

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
    bool pickAt(const QPoint& position, QVector3D* hitPosition = 0); // 通用 Primitive/Object 查询入口；保留给派生 Viewer 显式调用，不受当前 PickMode 限制。
    bool pickPointAt(const QPoint& position, ScenePointHit& hit);      // 只执行 Vertex / Endpoint Snap，并同步当前 Pick / Bounds Highlight。
    bool pickTriangleAt(const QPoint& position, ScenePrimitiveHit& hit); // 只执行 Triangle Primitive Picking，并同步 Triangle Highlight。
    bool pickItemAt(const QPoint& position, SceneRayHit& hit);          // 只执行 RenderItem Bounds Picking，并同步 Bounds Highlight。
    RenderItem* pickedItem();                          // 最近一次成功 Picking 命中的 Item；没有命中时为 0。
    const RenderItem* pickedItem() const;
    void clearPickedItem();                            // 只清除 Viewer Picking 状态，不隐式修改 Highlight。

    /// Highlight
    bool showBoundsHighlight(const AxisAlignedBoundingBox& bounds); // 显示明确 World Bounds 的黄色 AABB，不查询 Picking 状态。
    void clearBoundsHighlight();
    bool showTriangleHighlight(const QVector3D& vertex0, const QVector3D& vertex1, const QVector3D& vertex2); // 显示明确 World Triangle 的橙色边线，不查询 Picking 状态。
    bool showLineHighlight(const QVector3D& vertex0, const QVector3D& vertex1); // 显示明确 World Line Segment 的橙色线段，不查询 Picking 状态。
    void clearPrimitiveHighlight();

    /// Camera View Bounds
    bool focusPoint(const QVector3D& worldPoint);                         // 平移当前 View Bounds，使其 Center 移到指定世界点。
    bool focusBounds(const AxisAlignedBoundingBox& bounds);               // 用指定 Bounds 替换当前 View Bounds，并保持当前方向和距离。
    bool focusItem(const RenderItem* item);                               // 用指定 Item 的 World Bounds 替换当前 View Bounds。
    bool focusPrimitive(const ScenePrimitiveHit& hit);                    // 平移当前 View Bounds，使其 Center 移到指定 Render Primitive 几何中心。
    bool fitBoundsToView(const AxisAlignedBoundingBox& bounds, float margin = 1.15f); // 用指定 Bounds 替换当前 View Bounds，并按当前方向完整 Fit。
    bool fitItemToView(const RenderItem* item, float margin = 1.15f);      // 用指定 Item 的 World Bounds 替换当前 View Bounds，并完整 Fit。
    bool fitPrimitiveToView(const ScenePrimitiveHit& hit, float margin = 1.15f); // 用指定 Render Primitive Bounds 替换当前 View Bounds，并完整 Fit。

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
    BufferGeometry* createCameraTargetMarker();
    BufferGeometry* createBoundsHighlightGeometry();
    BufferGeometry* createPrimitiveHighlightGeometry();
    BufferGeometry* createMeasurementMarkerGeometry();
    BufferGeometry* createMeasurementLineGeometry();

    /// Measurement / Picking 内部辅助
    bool handleRightClickPick(const QPoint& position);                  // 按当前 ViewerPickMode 分派一次右键 Point / Triangle / Item 查询。
    bool measureAt(const QPoint& position);                             // Point Mode 下只使用精确 Vertex / Endpoint Snap 推进 A -> B -> 新 A。
    bool projectWorldPointToScreen(const QVector3D& worldPoint, QPointF& screenPoint) const; // 将明确 World Point 投影到当前 Widget Pixel。
    void updateMeasurementLabels();                                      // 更新 QLabel Overlay 的坐标、长度文字和屏幕位置，不使用 QPainter OpenGL Paint Engine。

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

    GridPlaneGeometry* m_grid;                         // XZ 世界参考网格。
    CoordinateSystemGeometry* m_axes;                  // RGB 世界坐标轴。
    ViewNavigationGeometry* m_viewNavigation;          // 右上角方向导航几何。
    BufferGeometry* m_cameraTargetMarker;              // Camera Orbit Target 黄色十字。
    BufferGeometry* m_boundsHighlight;                 // 显式 World Bounds 黄色 AABB Highlight。
    BufferGeometry* m_primitiveHighlight;              // 显式 World Triangle / Line 橙色 Primitive Highlight。
    BufferGeometry* m_measurementMarker;               // Viewer 内部单位十字 Measurement Point Marker。
    BufferGeometry* m_measurementLine;                 // Viewer 内部动态 A-B Measurement Segment。
    QLabel* m_measurementPointALabel;                  // Point A 屏幕文字 Overlay，QObject 父对象为 Viewer。
    QLabel* m_measurementPointBLabel;                  // Point B 屏幕文字 Overlay，QObject 父对象为 Viewer。
    QLabel* m_measurementLengthLabel;                  // A-B 长度屏幕文字 Overlay，QObject 父对象为 Viewer。

    QPoint m_lastMousePosition;                        // 上一次鼠标位置。
    QPoint m_rightPressPosition;                       // 当前 Right Button Press 起始位置，用于区分 Measurement Click 与误拖动。
    bool m_rightDragOccurred;                          // Right Button 按下后是否已经越过 Qt Drag Threshold。
    bool m_glReady;                                    // 当前 Viewer OpenGL Scene 是否完整初始化。
    bool m_releasePerformed;                           // 当前 Context 生命周期是否已经执行 Viewer GPU Cleanup。
    bool m_showGrid;                                   // 是否显示 Grid。
    bool m_showAxes;                                   // 是否显示世界坐标轴。
    bool m_showViewNavigation;                         // 是否显示 View Navigation。
    bool m_showCameraTarget;                           // 是否显示 Camera Target。
    bool m_showBoundsHighlight;                       // 是否显示当前明确 Bounds Highlight。
    ViewerPickMode m_pickMode;                       // 当前右键 Picking 模式。
    float m_primitivePickTolerance;                    // 屏幕空间 Line Primitive Picking 容差，单位 Pixel。
    float m_pointPickTolerance;                        // Point / Endpoint Snap 容差，单位 Pixel。
    bool m_showPrimitiveHighlight;                     // 是否显示当前明确 Primitive Highlight。
    bool m_hasMeasurementPointA;                       // 当前是否存在明确 Measurement Point A。
    bool m_hasMeasurementPointB;                       // 当前是否存在明确 Measurement Point B。
    QVector3D m_measurementPointA;                     // 当前 Measurement Point A 世界坐标。
    QVector3D m_measurementPointB;                     // 当前 Measurement Point B 世界坐标。
};

#endif // OPENGLVIEWERWIDGET_H
