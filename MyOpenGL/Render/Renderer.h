#ifndef RENDERER_H
#define RENDERER_H

#include "Render/ShaderProgram.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>

class Camera;
class LightManager;
class Material;
class RenderableObject;
class RenderContext;
class RenderItem;
class ResourceManager;
class Scene;

/// 基础 Renderer。
/// 提供 VertexColor 和 Lit 两条基础管线，并在其上支持扁平 Scene / RenderItem 绘制。
class Renderer
{
public:
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
    bool drawVertexColorMesh(const RenderableObject* mesh, const QMatrix4x4& model, bool depthTest = true); // 使用 position + color 管线绘制网格。
    bool drawLitMesh(const RenderableObject* mesh, const Material* material, const ResourceManager* resourceManager, const LightManager* lightManager, const QMatrix4x4& model, bool depthTest = true); // 使用 position + normal + uv 光照管线绘制网格。
    bool drawItem(const RenderItem* item, const ResourceManager* resourceManager, const LightManager* lightManager); // 根据 Item Material 类型选择基础绘制管线。
    bool drawScene(const Scene* scene, const ResourceManager* resourceManager, const LightManager* lightManager); // 按 Scene Item 创建顺序绘制全部可见 Item。
    bool drawViewNavigation(const RenderableObject* mesh, const Camera* camera); // 在右上角绘制只反映 Camera 朝向的 RGB 导航器。
    void endFrame();

private:
    /// 内部辅助
    GLenum primitiveMode(const RenderableObject* mesh) const;

private:
    RenderContext* m_context;             // 当前 Renderer 使用的 OpenGL 函数上下文，不拥有该对象。
    ShaderProgram m_vertexColorProgram;   // position + color 无光照 Shader。
    ShaderProgram m_litProgram;           // position + normal + uv 光照 Shader。

    GLint m_colorModelLocation;           // VertexColor Shader Model Matrix。
    GLint m_colorViewLocation;            // VertexColor Shader View Matrix。
    GLint m_colorProjectionLocation;      // VertexColor Shader Projection Matrix。

    GLint m_litModelLocation;             // Lit Shader Model Matrix。
    GLint m_litViewLocation;              // Lit Shader View Matrix。
    GLint m_litProjectionLocation;        // Lit Shader Projection Matrix。
    GLint m_litNormalLocation;            // Lit Shader Normal Matrix。
    GLint m_litCameraPositionLocation;    // Camera 世界坐标位置。
    GLint m_litBaseColorLocation;         // Material Base Color。
    GLint m_litSpecularColorLocation;     // Material Specular Color。
    GLint m_litShininessLocation;         // Material 高光指数。
    GLint m_litUseTextureLocation;        // 当前 Material 是否使用 Diffuse Texture。
    GLint m_litTextureLocation;           // Diffuse Texture Sampler。
    GLint m_litAmbientColorLocation;      // 场景环境光颜色。
    GLint m_litAmbientIntensityLocation;  // 场景环境光强度。
    GLint m_litLightDirectionLocation;    // Directional Light 从光源指向场景的方向。
    GLint m_litLightColorLocation;        // Directional Light RGB。
    GLint m_litLightIntensityLocation;    // Directional Light 强度。

    QMatrix4x4 m_viewMatrix;              // 当前 Frame 的 View Matrix。
    QMatrix4x4 m_projectionMatrix;        // 当前 Frame 的 Projection Matrix。
    QVector3D m_cameraPosition;           // 当前 Frame 的 Camera 世界坐标位置。
    QVector4D m_clearColor;               // Frame Buffer 清屏颜色。
    int m_viewportWidth;                  // 当前 Frame 主 Viewport 宽度。
    int m_viewportHeight;                 // 当前 Frame 主 Viewport 高度。
    bool m_initialized;                   // Renderer GPU 状态是否初始化完成。
    bool m_frameActive;                   // beginFrame() 与 endFrame() 之间是否存在活动帧。
};

#endif // RENDERER_H