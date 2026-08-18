#ifndef RENDERER_H
#define RENDERER_H

#include "ShaderProgram.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>

class Camera;
class LightManager;
class Material;
class Geometry;
class RenderContext;
class RenderItem;
class ResourceManager;
class Scene;

/// 基础 Renderer。
/// 提供 VertexColor、SolidColor、Lit 和 LitVertexColor 基础管线，并在其上支持扁平 Scene / RenderItem 绘制。
/// 当前基础 Renderer 不启用 Texture 绘制路径。
class Renderer
{
public:
    static const int MaxLights = 8; // OpenGL 3.3 基础多光源上限；当前轻量 Viewer 不引入 UBO / SSBO。

    Renderer();
    ~Renderer();

    /// GPU 生命周期
    bool initialize(RenderContext* context); // 创建 Renderer 当前使用的 Shader Programs。
    void release();                          // 释放 Renderer 持有的 GPU Programs。

    /// Render State
    void setClearColor(const QVector4D& color);
    const QVector4D& clearColor() const;

    /// Frame
    bool beginFrame(const Camera* camera, int viewportWidth, int viewportHeight); // 设置主 Viewport、清屏并保存当前 Camera 状态。
    bool drawVertexColorGeometry(const Geometry* geometry, const QMatrix4x4& model, bool depthTest = true); // 使用 position + color 管线绘制 Geometry。
    bool drawLitGeometry(const Geometry* geometry, const Material* material, const ResourceManager* resourceManager, const LightManager* lightManager, const QMatrix4x4& model, bool depthTest = true); // 使用 Lit 或 LitVertexColor 光照管线绘制 Geometry。
    bool drawItem(const RenderItem* item, const ResourceManager* resourceManager, const LightManager* lightManager); // 根据 Item Material 和 DisplayMode 绘制用户对象。
    bool drawScene(const Scene* scene, const ResourceManager* resourceManager, const LightManager* lightManager); // 按 Scene Item 创建顺序绘制全部可见 Item。
    bool drawViewNavigation(const Geometry* geometry, const Camera* camera); // 在右上角绘制只反映 Camera 朝向的 RGB 导航器。
    void endFrame();

private:
    /// 内部辅助
    bool drawMaterialGeometry(const Geometry* geometry, const Material* material, const ResourceManager* resourceManager, const LightManager* lightManager, const QMatrix4x4& model, bool depthTest);
    bool drawWireGeometry(const Geometry* geometry, const QMatrix4x4& model, const QVector4D& color, bool depthTest, bool overlay);
    GLenum primitiveMode(const Geometry* geometry) const;

private:
    RenderContext* m_context;             // 当前 Renderer 使用的 OpenGL 函数上下文，不拥有该对象。
    ShaderProgram m_vertexColorProgram;   // position + color 无光照 Shader。
    ShaderProgram m_solidColorProgram;    // position + uniform color Shader，用于 Triangle Wireframe / Edge Overlay。
    ShaderProgram m_litProgram;           // position + normal + optional color 共用光照 Shader。

    GLint m_colorModelLocation;           // VertexColor Shader Model Matrix。
    GLint m_colorViewLocation;            // VertexColor Shader View Matrix。
    GLint m_colorProjectionLocation;      // VertexColor Shader Projection Matrix。

    GLint m_solidModelLocation;           // SolidColor Shader Model Matrix。
    GLint m_solidViewLocation;            // SolidColor Shader View Matrix。
    GLint m_solidProjectionLocation;      // SolidColor Shader Projection Matrix。
    GLint m_solidColorLocation;           // SolidColor Shader 统一 RGBA 颜色。

    GLint m_litModelLocation;             // Lit Shader Model Matrix。
    GLint m_litViewLocation;              // Lit Shader View Matrix。
    GLint m_litProjectionLocation;        // Lit Shader Projection Matrix。
    GLint m_litNormalLocation;            // Lit Shader Normal Matrix。
    GLint m_litCameraPositionLocation;    // Camera 世界坐标位置。
    GLint m_litBaseColorLocation;         // Material Base Color。
    GLint m_litSpecularColorLocation;     // Material Specular Color。
    GLint m_litShininessLocation;         // Material 高光指数。
    GLint m_litUseVertexColorLocation;    // 当前 Material 是否使用 Geometry Vertex Color。
    GLint m_litAmbientColorLocation;      // 场景环境光颜色。
    GLint m_litAmbientIntensityLocation;  // 场景环境光强度。
    GLint m_litLightCountLocation;        // 当前参与 Lit Shader 的启用灯光数量。
    GLint m_litLightTypeLocation;         // LightType Uniform Array 首元素。
    GLint m_litLightPositionLocation;     // Point / Spot World Position Uniform Array 首元素。
    GLint m_litLightDirectionLocation;    // Directional / Spot 从光源指向场景的方向 Uniform Array 首元素。
    GLint m_litLightColorLocation;        // Light RGB Uniform Array 首元素。
    GLint m_litLightIntensityLocation;    // Light 强度 Uniform Array 首元素。
    GLint m_litLightRangeLocation;        // Point / Spot Range Uniform Array 首元素。
    GLint m_litLightInnerConeCosLocation; // Spot Inner Cone Cosine Uniform Array 首元素。
    GLint m_litLightOuterConeCosLocation; // Spot Outer Cone Cosine Uniform Array 首元素。

    QMatrix4x4 m_viewMatrix;              // 当前 Frame 的 View Matrix。
    QMatrix4x4 m_projectionMatrix;        // 当前 Frame 的 Projection Matrix。
    QVector3D m_cameraPosition;           // 当前 Frame 的 Camera 世界坐标位置。
    QVector4D m_clearColor;               // Frame Buffer 清屏颜色。
    int m_viewportWidth;                  // 当前 Frame 主 Viewport 宽度。
    int m_viewportHeight;                 // 当前 Frame 主 Viewport 高度。
    bool m_initialized;                   // Renderer GPU 状态是否初始化完成。
    bool m_frameActive;                   // beginFrame() 与 endFrame() 之间是否存在活动帧。
    bool m_lightLimitWarningIssued;       // 启用灯光超过 MaxLights 时只输出一次截断警告，避免逐帧刷屏。
};

#endif // RENDERER_H
