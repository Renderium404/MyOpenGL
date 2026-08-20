#ifndef RENDERER_H
#define RENDERER_H

#include "RenderContext.h"
#include "RenderState.h"
#include "ShaderProgram.h"

#include <QVector4D>

class Geometry;
class LightManager;
class Material;
class MyOpenGLContext;

/// 基础 Geometry Renderer。
/// Renderer 不理解 Scene / RenderItem / RenderPart 等模型组织结构，
/// 只负责根据 Geometry、Material、RenderState 和当前 RenderContext 提交 OpenGL Draw Command。
class Renderer
{
public:
    static const int MaxLights = 8; // OpenGL 3.3 基础多光源上限。

    Renderer();
    ~Renderer();

    /// GPU 生命周期
    bool initialize(MyOpenGLContext* openGLContext); // 创建当前 Renderer 使用的 Shader Programs。
    void release();                                  // 释放 Renderer 持有的 GPU Programs。

    /// Render State
    void setClearColor(const QVector4D& color);
    const QVector4D& clearColor() const;

    /// Frame
    bool beginFrame(const RenderContext& context); // 保存当前 Frame Context，设置主 Viewport 并清屏。
    void endFrame();

    /// Geometry Draw
    bool clearDepth(const RenderViewport& viewport);
    /// 根据 Material 类型选择对应渲染管线绘制 Geometry。
    /// VertexColor 不使用 LightManager；Lit / LitVertexColor 要求提供有效 LightManager。
    bool drawGeometry(const Geometry* geometry, const Material* material, const RenderState& state, const LightManager* lightManager = 0);

    /// 使用统一颜色绘制 Triangle Geometry Wireframe。
    /// overlay=true 用于在已经绘制的实体表面叠加边线。
    bool drawWireGeometry(const Geometry* geometry, const QVector4D& color, const RenderState& state, bool overlay = false);

private:
    /// Geometry Pipeline
    bool drawVertexColorGeometry(const Geometry* geometry, const RenderState& state);
    bool drawLitGeometry(const Geometry* geometry, const Material* material, const RenderState& state, const LightManager* lightManager);

    /// Render State
    bool applyRenderState(const RenderState& state);

    /// Geometry
    GLenum primitiveMode(const Geometry* geometry) const;

private:
    /// OpenGL 执行环境
    MyOpenGLContext* m_openGLContext; // 当前 OpenGL API 执行环境，不拥有该对象。

    /// Shader 程序
    ShaderProgram m_vertexColorProgram; // 顶点颜色无光照 Shader。
    ShaderProgram m_solidColorProgram;  // 统一颜色 Shader，用于纯色绘制和三角形线框。
    ShaderProgram m_litProgram;         // 基于法线的光照 Shader，可选使用顶点颜色。

    /// 顶点颜色 Shader 的 Uniform 位置
    GLint m_colorModelLocation;      // 模型变换矩阵。
    GLint m_colorViewLocation;       // 观察矩阵。
    GLint m_colorProjectionLocation; // 投影矩阵。

    /// 统一颜色 Shader 的 Uniform 位置
    GLint m_solidModelLocation;      // 模型变换矩阵。
    GLint m_solidViewLocation;       // 观察矩阵。
    GLint m_solidProjectionLocation; // 投影矩阵。
    GLint m_solidColorLocation;      // 统一 RGBA 颜色。

    /// 光照 Shader 的空间变换 Uniform 位置
    GLint m_litModelLocation;          // 模型变换矩阵。
    GLint m_litViewLocation;           // 观察矩阵。
    GLint m_litProjectionLocation;     // 投影矩阵。
    GLint m_litNormalLocation;         // 法线变换矩阵。
    GLint m_litCameraPositionLocation; // 相机世界坐标位置。

    /// 光照 Shader 的材质 Uniform 位置
    GLint m_litBaseColorLocation;      // 材质基础颜色。
    GLint m_litSpecularColorLocation;  // 材质高光颜色。
    GLint m_litShininessLocation;      // 材质高光指数。
    GLint m_litUseVertexColorLocation; // 是否叠加 Geometry 顶点颜色。

    /// 光照 Shader 的环境光 Uniform 位置
    GLint m_litAmbientColorLocation;     // 环境光颜色。
    GLint m_litAmbientIntensityLocation; // 环境光强度。

    /// 光照 Shader 的灯光数组 Uniform 位置
    GLint m_litLightCountLocation;        // 当前实际参与渲染的灯光数量。
    GLint m_litLightTypeLocation;         // 灯光类型数组首元素。
    GLint m_litLightPositionLocation;     // 点光源 / 聚光灯世界坐标数组首元素。
    GLint m_litLightDirectionLocation;    // 平行光 / 聚光灯方向数组首元素。
    GLint m_litLightColorLocation;        // 灯光颜色数组首元素。
    GLint m_litLightIntensityLocation;    // 灯光强度数组首元素。
    GLint m_litLightRangeLocation;        // 点光源 / 聚光灯作用范围数组首元素。
    GLint m_litLightInnerConeCosLocation; // 聚光灯内锥角余弦数组首元素。
    GLint m_litLightOuterConeCosLocation; // 聚光灯外锥角余弦数组首元素。

    /// 当前帧渲染环境
    RenderContext m_renderContext; // 当前 beginFrame() 到 endFrame() 之间共享的渲染环境。

    /// Renderer 自身状态
    QVector4D m_clearColor;         // 帧缓冲清屏颜色。
    bool m_initialized;             // Renderer 使用的 GPU Shader 是否已经初始化完成。
    bool m_frameActive;             // 当前是否处于一个有效的渲染帧中。
    bool m_lightLimitWarningIssued; // 灯光数量超过 MaxLights 时是否已经输出过警告。
};

#endif // RENDERER_H
