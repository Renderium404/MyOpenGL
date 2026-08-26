#ifndef RENDERER_H
#define RENDERER_H

#include "RenderContext.h"
#include "RenderState.h"
#include "ShaderProgram.h"

#include <QVector4D>

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
    bool initialize(MyOpenGLContext* openGLContext);
    void release();

    /// Render State
    void setClearColor(const QVector4D& color);
    const QVector4D& clearColor() const;

    /// Frame
    bool beginFrame(const RenderContext& context);
    void endFrame();

    /// Geometry Draw
    bool clearDepth(const RenderViewport& viewport);

    /// 根据 Material 配置绘制 Geometry。
    /// lights 表示本次 Draw 使用的全部灯光；Renderer 不检查 Light::isEnabled()。
    /// Ambient 会单独累加，不占 MaxLights；其余灯光按传入顺序最多使用 MaxLights 个。
    bool drawGeometry(const Geometry* geometry, const Material* material, const RenderState& state, const std::vector<const Light*>& lights);

    /// 使用统一颜色绘制 Triangle Geometry Wireframe。
    /// overlay=true 用于在已经绘制的实体表面叠加边线。
    bool drawWireGeometry(const Geometry* geometry, const QVector4D& color, const RenderState& state, bool overlay = false);

private:
    /// Geometry Pipeline
    bool drawColorGeometry(const Geometry* geometry, const QVector4D& color, const RenderState& state);
    bool drawVertexColorGeometry(const Geometry* geometry, const RenderState& state);
    bool drawTextureGeometry(const Geometry* geometry, const Material* material, const RenderState& state);
    bool drawLitGeometry(const Geometry* geometry, const Material* material, const RenderState& state, const std::vector<const Light*>& lights);

    /// Render State
    bool applyRenderState(const RenderState& state);

    /// Geometry
    GLenum primitiveMode(const Geometry* geometry) const;

private:
    /// OpenGL 执行环境
    MyOpenGLContext* m_openGLContext;

    /// Shader 程序
    ShaderProgram m_vertexColorProgram; //基于顶点的颜色渲染
    ShaderProgram m_solidColorProgram;  //基于一种统一颜色的渲染
    ShaderProgram m_textureProgram;     //基于纹理的渲染
    ShaderProgram m_litProgram;         //光照渲染

    /// 顶点颜色 Shader
    GLint m_colorModelLocation;
    GLint m_colorViewLocation;
    GLint m_colorProjectionLocation;

    /// 统一颜色 Shader
    GLint m_solidModelLocation;
    GLint m_solidViewLocation;
    GLint m_solidProjectionLocation;
    GLint m_solidColorLocation;

    /// 无光照纹理 Shader
    GLint m_textureModelLocation;
    GLint m_textureViewLocation;
    GLint m_textureProjectionLocation;
    GLint m_textureSamplerLocation;
    GLint m_textureColorLocation;

    /// 光照 Shader
    GLint m_litModelLocation;
    GLint m_litViewLocation;
    GLint m_litProjectionLocation;
    GLint m_litNormalLocation;

    GLint m_litBaseColorLocation;
    GLint m_litUseVertexColorLocation;

    GLint m_litAmbientLightLocation;

    GLint m_litLightCountLocation;
    GLint m_litLightTypeLocation;
    GLint m_litLightPositionLocation;
    GLint m_litLightDirectionLocation;
    GLint m_litLightColorLocation;
    GLint m_litLightIntensityLocation;
    GLint m_litLightRangeLocation;
    GLint m_litLightInnerConeCosLocation;
    GLint m_litLightOuterConeCosLocation;

    /// 当前帧渲染环境
    RenderContext m_renderContext;

    /// Renderer 自身状态
    QVector4D m_clearColor;
    bool m_initialized;
    bool m_frameActive;
    bool m_lightLimitWarningIssued;
};

#endif // RENDERER_H