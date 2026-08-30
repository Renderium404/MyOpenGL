#ifndef OPENGLVIEWERWIDGET_H
#define OPENGLVIEWERWIDGET_H

#include <QOpenGLWidget>
#include <QPointF>
#include <QTimer>
#include <QVector3D>
#include <QMatrix4x4>
#include <vector>

#include "MyOpenGL/Camera/CameraManager.h"
#include "MyOpenGL/Core/ResourceManager.h"
#include "MyOpenGL/Item/ItemManager.h"
#include "MyOpenGL/Light/LightManager.h"
#include "MyOpenGL/Material/MaterialManager.h"
#include "MyOpenGL/Render/MyOpenGLContext.h"
#include "MyOpenGL/Render/RenderContext.h"
#include "MyOpenGL/Render/Renderer.h"
#include "MyOpenGL/Resource/BufferGeometry.h"
#include "MyOpenGL/Viewer/System/CoordinateSystem.h"
#include "MyOpenGL/Viewer/System/ViewNavigation.h"
#include "MyOpenGL/Viewer/Measurement/MeasurementTool.h"
class QContextMenuEvent;
class QKeyEvent;
class QMouseEvent;
class QWheelEvent;
class Light;
class Material;
class RenderItem;
class QMenu;
class QPainter;
class QPointF;
class ViewportOverlayWidget;
/// MyOpenGL 基础 Viewer。
/// 负责 OpenGL 生命周期、Item 绘制、Camera 操作和 Viewer 系统显示。
class OpenGLViewerWidget : public QOpenGLWidget
{
     Q_OBJECT
public:
    explicit OpenGLViewerWidget(QWidget* parent = 0);
    ~OpenGLViewerWidget() override;

    /// Viewer 数据
    ResourceManager& resourceManager();
    const ResourceManager& resourceManager() const;

    MaterialManager& materialManager();
    const MaterialManager& materialManager() const;

    LightManager& lightManager();
    const LightManager& lightManager() const;

    CameraManager& cameraManager();
    const CameraManager& cameraManager() const;

    ItemManager& itemManager();
    const ItemManager& itemManager() const;
    ItemManager& measurementItemManager();
    const ItemManager& measurementItemManager() const;
    /// Viewer 系统显示
    CoordinateSystem& coordinateSystem();
    const CoordinateSystem& coordinateSystem() const;

    ViewNavigation& viewNavigation();
    const ViewNavigation& viewNavigation() const;

    /// Viewer 状态
    bool viewerGLReady() const;

    /// Camera
    bool fitItemsToView(float margin = 1.15f);
    void toggleProjection();//改变投影模式

    /// 测量工具
    void setMeasurementTool(MeasurementTool* tool);
    MeasurementTool* measurementTool();
    const MeasurementTool* measurementTool() const;
    /// 三维测量结果。
    void clearMeasurementItems();
    bool removeLastMeasurementItem();

    bool scenePointAtWorld(const QPointF& scene, QVector3D& world) const;
    bool worldPointAtScene(const QVector3D& world, QPointF& scene) const;
signals:
    void measurementFinished(MeasurementType type);
protected:
    /// OpenGL事件处理
    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;

    /// QT事件处理
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

    /// 子类扩展
    virtual void populateContextMenu(QMenu& menu);
    virtual bool handleKeyPress(QKeyEvent* event);
    // 背景绘制。
    virtual void drawSceneBackground(Renderer& renderer, const RenderContext& context);
    // 场景绘制。
    virtual void drawOpenGLFrame(Renderer& renderer, const RenderContext& context);
    // 前景绘制。
    virtual void drawSceneFront(Renderer& renderer, const RenderContext& context);
    // 悬浮层绘制
    virtual void drawViewportOverlay(QPainter& painter);

    /// 刷新 Viewport 悬浮层。
    void updateViewportOverlay();
    bool setStandardView(ViewNavigationFace face);
private:
    friend class ViewportOverlayWidget;
    /// OpenGL 生命周期
    void releaseViewerGL();
    
    bool removeMeasurementItemAt(int index);
    /// Viewer 系统资源
    void buildViewerResources();
    void unregisterViewerResources();

    /// 渲染编排
    bool buildRenderContext(RenderContext& context) const;
    bool buildNavigationAnchorGeometry();
    bool buildNavigationAnchorRenderState(const RenderContext& context, RenderState& state) const;

    /// Item 渲染
    bool drawItems(Renderer& renderer, const ItemManager& itemManager, const RenderContext& context, const std::vector<const Light*>& lights);
    /// Camera Navigation
    bool navigationAnchor(QVector3D& anchor) const;
    /// 缩放走的锚点获取路径
    QVector3D screenPointToZoomAnchor(const QPointF& position) const;
    /// 其他操作走的锚点获取路径
    QVector3D screenPointToAnchor(const QPointF& position) const;


    bool scenePointAtWorldFromDepth(const QPointF& scene, QVector3D& world) const;      //基于屏幕缓存的屏幕点转世界坐标
    bool scenePointAtWorldFromRay(const QPointF& scene, QVector3D& world) const;        //基于光线映射的屏幕点转世界坐标
    bool projectWorldPointToScene(const QVector3D& world, QPointF& scene) const;        //基于投影的世界坐标转屏幕坐标
    /// 用于可交互对象Item的深度缓存
    bool cacheSceneDepth(const RenderContext& context);         //缓存深度
    void clearSceneDepthCache();    //清理深度

private:
    /// Viewport Overlay
    ViewportOverlayWidget* m_viewportOverlay;             // 透明 2D 悬浮绘制层。
    /// Viewer 系统显示
    CoordinateSystem m_coordinateSystem;              // 世界坐标系。
    ViewNavigation m_viewNavigation;                  // 右上角视图导航器。

    /// OpenGL / Renderer
    MyOpenGLContext m_openGLContext;                  // OpenGL API 执行环境。
    Renderer m_renderer;                              // Geometry 绘制执行器。

    /// Viewer 数据
    ResourceManager m_resourceManager;                // Resource 登记、所有权和 GPU 同步。
    MaterialManager m_materialManager;                // Material 管理。
    LightManager m_lightManager;                      // Light 管理。
    CameraManager m_cameraManager;                    // Camera 管理和导航操作。
    ItemManager m_itemManager;                        // 用户 RenderItem 管理。
    ItemManager m_measurementItemManager;                 // 测量辅助 Item 管理。
    MeasurementTool* m_measurementTool;               // 当前测量工具，不负责生命周期。
    /// Viewer 系统渲染
    Material* m_systemVertexColorMaterial;            // 坐标系和导航器共用的无光照顶点颜色 Material。
    BufferGeometry m_navigationAnchorGeometry;        // Camera Navigation 锚点显示 Geometry。
    bool m_navigationAnchorVisible;                   // 是否显示当前导航锚点。
    int m_navigationAnchorPixelSize;                  // 锚点固定屏幕 Pixel 尺寸。
    QTimer m_navigationAnchorHideTimer;               // Wheel Zoom 后延迟隐藏锚点。

    /// Input 状态
    QPointF m_lastMousePosition;                       // 上一次鼠标位置。
    QVector3D m_navigationAnchor;                     // 当前拖动操作锚点。
    bool m_hasNavigationAnchor;                       // 当前是否存在拖动锚点。

    /// Viewer 状态
    bool m_glReady;                                   // OpenGL 是否初始化完成。
    bool m_releasePerformed;                          // 当前 OpenGL Context 是否已经执行 GPU 释放。
    ///深度缓存
    std::vector<float> m_sceneDepthBuffer;
    QMatrix4x4 m_sceneDepthInverseViewProjection;
    int m_sceneDepthWidth;
    int m_sceneDepthHeight;
    bool m_sceneDepthValid;
};

#endif // OPENGLVIEWERWIDGET_H