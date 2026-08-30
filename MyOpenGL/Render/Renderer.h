#ifndef RENDERER_H
#define RENDERER_H

#include "RenderContext.h"
#include "RenderState.h"
#include "ShaderProgram.h"

#include <QVector4D>

#include <cstddef>
#include <vector>

class Geometry;
class Light;
class Material;
class MyOpenGLContext;

/// 基础 Geometry Renderer。
/// Renderer 不理解 ItemManager / RenderItem / RenderPart 等模型组织结构，
/// 只负责根据 Geometry、Material、RenderState、Light 和当前 RenderContext 提交 OpenGL Draw Command。
class Renderer
{
public:
    static const int MaxLights = 8; // 单次 Draw 最多使用的非 Ambient 灯光数量。

    Renderer();
    ~Renderer();

    /// GPU 生命周期

    /// 初始化 Renderer 使用的 OpenGL Shader 和运行状态。
    /// openGLContext 仅作为引用使用，Renderer 不拥有该对象。
    bool initialize(MyOpenGLContext* openGLContext);

    /// 释放 Renderer 创建的 OpenGL Shader 等 GPU 状态。
    void release();

    /// Render State

    /// 设置后续 Frame 使用的背景清除颜色。
    void setClearColor(const QVector4D& color);

    /// 返回当前背景清除颜色。
    const QVector4D& clearColor() const;

    /// Frame

    /// 开始一个 Frame，并保存当前共享 RenderContext。
    bool beginFrame(const RenderContext& context);

    /// 结束当前 Frame，并恢复 Renderer 管理的基础 OpenGL 状态。
    void endFrame();

    /// Geometry Draw

    /// 清除指定 Viewport 范围内的 Depth Buffer。
    bool clearDepth(const RenderViewport& viewport);

    /// 使用一个 RenderState 绘制一次 Geometry。
    /// geometry、material 和 lights 中的对象均仅作为引用使用，Renderer 不拥有这些对象。
    bool drawGeometry(const Geometry* geometry, const Material* material, const RenderState& state, const std::vector<const Light*>& lights);

    /// 使用同一个 Geometry、Material 和 Lights 按多个 RenderState 连续绘制。
    /// 与单 RenderState 接口相比，该接口会复用 Geometry、Shader、Material 和 Light 等公共 GPU 状态。
    /// geometry、material 和 lights 中的对象均仅作为引用使用，Renderer 不拥有这些对象。
    bool drawGeometry(const Geometry* geometry, const Material* material, const std::vector<RenderState>& states, const std::vector<const Light*>& lights);

    /// 使用统一颜色绘制一次 Triangle Geometry Wireframe。
    /// overlay=true 用于在已经绘制的实体表面叠加边线。
    /// geometry 仅作为引用使用，Renderer 不拥有该对象。
    bool drawWireGeometry(const Geometry* geometry, const QVector4D& color, const RenderState& state, bool overlay = false);

    /// 使用统一颜色按多个 RenderState 连续绘制 Triangle Geometry Wireframe。
    /// 与单 RenderState 接口相比，该接口会复用 Geometry、Shader 和统一颜色等公共 GPU 状态。
    /// geometry 仅作为引用使用，Renderer 不拥有该对象。
    bool drawWireGeometry(const Geometry* geometry, const QVector4D& color, const std::vector<RenderState>& states, bool overlay = false);

private:
    /// Geometry Dispatch

    /// 根据 Material 类型选择实际 Geometry Pipeline。
    /// states 指向调用期间有效的 RenderState 数组，不保存该引用。
    bool drawGeometryStates(const Geometry* geometry,
                            const Material* material,
                            const RenderState* states,
                            std::size_t stateCount,
                            const std::vector<const Light*>& lights);

    /// Geometry Pipeline

    /// 使用统一颜色 Pipeline 按多个 RenderState 连续绘制同一个 Geometry。
    bool drawColorGeometry(const Geometry* geometry,
                           const QVector4D& color,
                           const RenderState* states,
                           std::size_t stateCount);

    /// 使用顶点颜色 Pipeline 按多个 RenderState 连续绘制同一个 Geometry。
    bool drawVertexColorGeometry(const Geometry* geometry,
                                 const RenderState* states,
                                 std::size_t stateCount);

    /// 使用纹理 Pipeline 按多个 RenderState 连续绘制同一个 Geometry。
    /// material 仅作为引用使用，Renderer 不拥有该对象。
    bool drawTextureGeometry(const Geometry* geometry,
                             const Material* material,
                             const RenderState* states,
                             std::size_t stateCount);

    /// 使用光照 Pipeline 按多个 RenderState 连续绘制同一个 Geometry。
    /// material 和 lights 中的对象均仅作为引用使用，Renderer 不拥有这些对象。
    bool drawLitGeometry(const Geometry* geometry,
                         const Material* material,
                         const RenderState* states,
                         std::size_t stateCount,
                         const std::vector<const Light*>& lights);

    /// 使用统一颜色按多个 RenderState 连续绘制 Triangle Geometry Wireframe。
    bool drawWireGeometryStates(const Geometry* geometry,
                                const QVector4D& color,
                                const RenderState* states,
                                std::size_t stateCount,
                                bool overlay);

    /// Render State

    /// 将一个 RenderState 中的 Viewport、Depth 和 Blend 状态应用到 OpenGL。
    bool applyRenderState(const RenderState& state);

    /// Geometry

    /// 返回 Geometry RenderType 对应的 OpenGL Primitive Mode。
    GLenum primitiveMode(const Geometry* geometry) const;

private:
    MyOpenGLContext* m_openGLContext; // 当前 OpenGL 执行环境，仅引用，不拥有。

    ShaderProgram m_vertexColorProgram; // 基于顶点颜色的渲染 Program。
    ShaderProgram m_solidColorProgram;  // 基于统一颜色的渲染 Program。
    ShaderProgram m_textureProgram;     // 基于纹理的渲染 Program。
    ShaderProgram m_litProgram;         // 基于光照的渲染 Program。

    GLint m_colorModelLocation;      // 顶点颜色 Program 的 Model Uniform Location。
    GLint m_colorViewLocation;       // 顶点颜色 Program 的 View Uniform Location。
    GLint m_colorProjectionLocation; // 顶点颜色 Program 的 Projection Uniform Location。

    GLint m_solidModelLocation;      // 统一颜色 Program 的 Model Uniform Location。
    GLint m_solidViewLocation;       // 统一颜色 Program 的 View Uniform Location。
    GLint m_solidProjectionLocation; // 统一颜色 Program 的 Projection Uniform Location。
    GLint m_solidColorLocation;      // 统一颜色 Program 的 Color Uniform Location。

    GLint m_textureModelLocation;      // 纹理 Program 的 Model Uniform Location。
    GLint m_textureViewLocation;       // 纹理 Program 的 View Uniform Location。
    GLint m_textureProjectionLocation; // 纹理 Program 的 Projection Uniform Location。
    GLint m_textureSamplerLocation;    // 纹理 Program 的 Texture Sampler Uniform Location。
    GLint m_textureColorLocation;      // 纹理 Program 的颜色乘数 Uniform Location。

    GLint m_litModelLocation;      // 光照 Program 的 Model Uniform Location。
    GLint m_litViewLocation;       // 光照 Program 的 View Uniform Location。
    GLint m_litProjectionLocation; // 光照 Program 的 Projection Uniform Location。
    GLint m_litNormalLocation;     // 光照 Program 的 Normal Matrix Uniform Location。

    GLint m_litBaseColorLocation;      // 光照 Program 的基础颜色 Uniform Location。
    GLint m_litUseVertexColorLocation; // 光照 Program 的顶点颜色开关 Uniform Location。

    GLint m_litAmbientLightLocation; // 光照 Program 的环境光 Uniform Location。

    GLint m_litLightCountLocation;        // 光照 Program 的有效非环境光数量 Uniform Location。
    GLint m_litLightTypeLocation;         // 光照 Program 的灯光类型数组 Uniform Location。
    GLint m_litLightPositionLocation;     // 光照 Program 的灯光位置数组 Uniform Location。
    GLint m_litLightDirectionLocation;    // 光照 Program 的灯光方向数组 Uniform Location。
    GLint m_litLightColorLocation;        // 光照 Program 的灯光颜色数组 Uniform Location。
    GLint m_litLightIntensityLocation;    // 光照 Program 的灯光强度数组 Uniform Location。
    GLint m_litLightRangeLocation;        // 光照 Program 的灯光范围数组 Uniform Location。
    GLint m_litLightInnerConeCosLocation; // 光照 Program 的 Spot 内锥角余弦数组 Uniform Location。
    GLint m_litLightOuterConeCosLocation; // 光照 Program 的 Spot 外锥角余弦数组 Uniform Location。

    RenderContext m_renderContext; // 当前 Frame 的共享渲染环境。

    QVector4D m_clearColor;       // 当前 Frame 背景清除颜色。
    bool m_initialized;           // Renderer 是否已经完成 GPU 初始化。
    bool m_frameActive;           // 当前是否位于 beginFrame()/endFrame() 之间。
    bool m_lightLimitWarningIssued; // 当前 Renderer 生命周期内是否已经输出过灯光数量超限警告。
};

#endif // RENDERER_H