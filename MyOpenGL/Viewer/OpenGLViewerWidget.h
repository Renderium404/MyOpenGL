#ifndef OPENGLVIEWERWIDGET_H
#define OPENGLVIEWERWIDGET_H

#include <QOpenGLWidget>
#include <QPoint>
#include <QVector3D>

#include "MyOpenGL/Camera/CameraManager.h"
#include "MyOpenGL/Core/ResourceManager.h"
#include "MyOpenGL/Light/LightManager.h"
#include "MyOpenGL/Material/MaterialManager.h"
#include "MyOpenGL/Render/MyOpenGLContext.h"
#include "MyOpenGL/Render/RenderContext.h"
#include "MyOpenGL/Render/Renderer.h"
#include "MyOpenGL/Scene/Scene.h"
#include "MyOpenGL/Viewer/System/CoordinateSystem.h"
#include "MyOpenGL/Viewer/System/ViewNavigation.h"

class QKeyEvent;
class QMouseEvent;
class QWheelEvent;
class Material;
class RenderItem;

/// MyOpenGL 基础 Viewer。
/// 负责 OpenGL 生命周期、Scene 绘制、Camera 操作和 Viewer 系统显示。
class OpenGLViewerWidget : public QOpenGLWidget
{
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

    Scene& scene();
    const Scene& scene() const;

    bool viewerGLReady() const;

    /// Camera
    bool fitSceneToView(float margin = 1.15f);
    void toggleProjection();

protected:
    /// OpenGL
    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;

    /// Input
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    /// OpenGL 生命周期
    void releaseViewerGL();

    /// Viewer 系统资源
    void buildViewerResources();
    void unregisterViewerResources();

    /// 渲染编排
    bool buildRenderContext(RenderContext& context) const;
    bool drawScene(const RenderContext& context);
    bool drawItem(const RenderItem* item, const RenderContext& context);

    /// Camera Navigation
    bool navigationAnchor(QVector3D& anchor) const;
    bool scenePointAt(const QPoint& position, QVector3D& point);
private:
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
    Scene m_scene;                                    // 用户 RenderItem 场景。
    /// Input 状态
    QPoint m_lastMousePosition;                       // 上一次鼠标位置。
    QVector3D m_navigationAnchor;                     // 当前拖动操作锚点。
    bool m_hasNavigationAnchor;                       // 当前是否存在拖动锚点。
    /// Viewer 系统渲染
    Material* m_systemVertexColorMaterial;            // 坐标系和导航共用顶点颜色材质。


    /// Viewer 状态
    bool m_glReady;                                   // OpenGL 是否初始化完成。
    bool m_releasePerformed;                          // 是否已经执行 GPU 释放。
};

#endif // OPENGLVIEWERWIDGET_H