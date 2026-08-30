#include "Renderer.h"

#include "MyOpenGL/Light/Light.h"
#include "MyOpenGL/Material/Material.h"
#include "MyOpenGL/Render/MyOpenGLContext.h"
#include "MyOpenGL/Resource/Geometry.h"
#include "MyOpenGL/Resource/Texture.h"

#include <QDebug>
#include <QMatrix3x3>
#include <QVector3D>
#include <QtMath>

Renderer::Renderer()
    : m_openGLContext(0)
    , m_colorModelLocation(-1)
    , m_colorViewLocation(-1)
    , m_colorProjectionLocation(-1)
    , m_solidModelLocation(-1)
    , m_solidViewLocation(-1)
    , m_solidProjectionLocation(-1)
    , m_solidColorLocation(-1)
    , m_textureModelLocation(-1)
    , m_textureViewLocation(-1)
    , m_textureProjectionLocation(-1)
    , m_textureSamplerLocation(-1)
    , m_textureColorLocation(-1)
    , m_litModelLocation(-1)
    , m_litViewLocation(-1)
    , m_litProjectionLocation(-1)
    , m_litNormalLocation(-1)
    , m_litBaseColorLocation(-1)
    , m_litUseVertexColorLocation(-1)
    , m_litAmbientLightLocation(-1)
    , m_litLightCountLocation(-1)
    , m_litLightTypeLocation(-1)
    , m_litLightPositionLocation(-1)
    , m_litLightDirectionLocation(-1)
    , m_litLightColorLocation(-1)
    , m_litLightIntensityLocation(-1)
    , m_litLightRangeLocation(-1)
    , m_litLightInnerConeCosLocation(-1)
    , m_litLightOuterConeCosLocation(-1)
    , m_clearColor(0.1f, 0.1f, 0.1f, 1.0f)
    , m_initialized(false)
    , m_frameActive(false)
    , m_lightLimitWarningIssued(false)
{
}

Renderer::~Renderer()
{
    if (m_initialized)
        qWarning() << "Renderer destroyed while GPU state is still initialized.";
}

/// GPU 生命周期

bool Renderer::initialize(MyOpenGLContext* openGLContext)
{
    if (openGLContext == 0 || !openGLContext->isInitialized())
    {
        qWarning() << "Renderer initialize failed: MyOpenGLContext is invalid.";
        return false;
    }

    if (m_initialized)
        return true;

    QOpenGLFunctions_3_3_Core* gl = openGLContext->gl();

    if (gl == 0)
        return false;

    const char* vertexColorVertexShader =
        "#version 330 core\n"
        "layout(location = 0) in vec3 aPosition;\n"
        "layout(location = 3) in vec3 aColor;\n"
        "uniform mat4 model;\n"
        "uniform mat4 view;\n"
        "uniform mat4 projection;\n"
        "out vec3 vertexColor;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = projection * view * model * vec4(aPosition, 1.0);\n"
        "    vertexColor = aColor;\n"
        "}\n";

    const char* vertexColorFragmentShader =
        "#version 330 core\n"
        "in vec3 vertexColor;\n"
        "out vec4 FragColor;\n"
        "void main()\n"
        "{\n"
        "    FragColor = vec4(vertexColor, 1.0);\n"
        "}\n";

    const char* solidColorVertexShader =
        "#version 330 core\n"
        "layout(location = 0) in vec3 aPosition;\n"
        "uniform mat4 model;\n"
        "uniform mat4 view;\n"
        "uniform mat4 projection;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = projection * view * model * vec4(aPosition, 1.0);\n"
        "}\n";

    const char* solidColorFragmentShader =
        "#version 330 core\n"
        "uniform vec4 color;\n"
        "out vec4 FragColor;\n"
        "void main()\n"
        "{\n"
        "    FragColor = color;\n"
        "}\n";

    const char* litVertexShader =
        "#version 330 core\n"
        "layout(location = 0) in vec3 aPosition;\n"
        "layout(location = 1) in vec3 aNormal;\n"
        "layout(location = 3) in vec3 aVertexColor;\n"
        "uniform mat4 model;\n"
        "uniform mat4 view;\n"
        "uniform mat4 projection;\n"
        "uniform mat3 normalMatrix;\n"
        "out vec3 fragmentPosition;\n"
        "out vec3 fragmentNormal;\n"
        "out vec3 vertexColor;\n"
        "void main()\n"
        "{\n"
        "    vec4 worldPosition = model * vec4(aPosition, 1.0);\n"
        "    fragmentPosition = worldPosition.xyz;\n"
        "    fragmentNormal = normalize(normalMatrix * aNormal);\n"
        "    vertexColor = aVertexColor;\n"
        "    gl_Position = projection * view * worldPosition;\n"
        "}\n";

    const char* litFragmentShader =
        "#version 330 core\n"
        "const int MaxLights = 8;\n"
        "const int LightTypeDirectional = 0;\n"
        "const int LightTypePoint = 1;\n"
        "const int LightTypeSpot = 2;\n"
        "\n"
        "in vec3 fragmentPosition;\n"
        "in vec3 fragmentNormal;\n"
        "in vec3 vertexColor;\n"
        "\n"
        "uniform vec4 baseColor;\n"
        "uniform bool useVertexColor;\n"
        "uniform vec3 ambientLight;\n"
        "\n"
        "uniform int lightCount;\n"
        "uniform int lightType[MaxLights];\n"
        "uniform vec3 lightPosition[MaxLights];\n"
        "uniform vec3 lightDirection[MaxLights];\n"
        "uniform vec3 lightColor[MaxLights];\n"
        "uniform float lightIntensity[MaxLights];\n"
        "uniform float lightRange[MaxLights];\n"
        "uniform float lightInnerConeCos[MaxLights];\n"
        "uniform float lightOuterConeCos[MaxLights];\n"
        "\n"
        "out vec4 FragColor;\n"
        "\n"
        "void main()\n"
        "{\n"
        "    vec4 surfaceColor = useVertexColor ? vec4(vertexColor, 1.0) : baseColor;\n"
        "    vec3 normal = normalize(fragmentNormal);\n"
        "\n"
        "    vec3 result = surfaceColor.rgb * ambientLight;\n"
        "\n"
        "    for (int i = 0; i < MaxLights; ++i)\n"
        "    {\n"
        "        if (i >= lightCount)\n"
        "            break;\n"
        "\n"
        "        vec3 toLight = vec3(0.0);\n"
        "        float attenuation = 1.0;\n"
        "\n"
        "        if (lightType[i] == LightTypeDirectional)\n"
        "        {\n"
        "            toLight = normalize(-lightDirection[i]);\n"
        "        }\n"
        "        else\n"
        "        {\n"
        "            vec3 delta = lightPosition[i] - fragmentPosition;\n"
        "            float distanceToLight = length(delta);\n"
        "            float safeRange = max(lightRange[i], 0.0001);\n"
        "\n"
        "            if (distanceToLight <= 0.000001 || distanceToLight >= safeRange)\n"
        "                continue;\n"
        "\n"
        "            toLight = delta / distanceToLight;\n"
        "\n"
        "            float normalizedDistance = clamp(distanceToLight / safeRange, 0.0, 1.0);\n"
        "            attenuation = 1.0 - normalizedDistance;\n"
        "            attenuation *= attenuation;\n"
        "\n"
        "            if (lightType[i] == LightTypeSpot)\n"
        "            {\n"
        "                vec3 fromLight = -toLight;\n"
        "                float coneCos = dot(normalize(lightDirection[i]), fromLight);\n"
        "                float coneFactor = smoothstep(lightOuterConeCos[i], lightInnerConeCos[i], coneCos);\n"
        "                attenuation *= coneFactor;\n"
        "            }\n"
        "        }\n"
        "\n"
        "        float diffuseFactor = max(dot(normal, toLight), 0.0);\n"
        "\n"
        "        if (diffuseFactor <= 0.0 || attenuation <= 0.0)\n"
        "            continue;\n"
        "\n"
        "        vec3 radiance = lightColor[i] * lightIntensity[i] * attenuation;\n"
        "        result += surfaceColor.rgb * radiance * diffuseFactor;\n"
        "    }\n"
        "\n"
        "    FragColor = vec4(result, surfaceColor.a);\n"
        "}\n";

    const char* textureVertexShader =
        "#version 330 core\n"
        "layout(location = 0) in vec3 aPosition;\n"
        "layout(location = 2) in vec2 aTexCoord;\n"
        "uniform mat4 model;\n"
        "uniform mat4 view;\n"
        "uniform mat4 projection;\n"
        "out vec2 texCoord;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = projection * view * model * vec4(aPosition, 1.0);\n"
        "    texCoord = aTexCoord;\n"
        "}\n";

    const char* textureFragmentShader =
        "#version 330 core\n"
        "in vec2 texCoord;\n"
        "uniform sampler2D texture0;\n"
        "uniform vec4 color;\n"
        "out vec4 FragColor;\n"
        "void main()\n"
        "{\n"
        "    FragColor = texture(texture0, texCoord) * color;\n"
        "}\n";

    if (!m_vertexColorProgram.initialize(gl, vertexColorVertexShader, vertexColorFragmentShader))
        return false;

    if (!m_solidColorProgram.initialize(gl, solidColorVertexShader, solidColorFragmentShader))
    {
        m_vertexColorProgram.release(gl);
        return false;
    }

    if (!m_textureProgram.initialize(gl, textureVertexShader, textureFragmentShader))
    {
        m_solidColorProgram.release(gl);
        m_vertexColorProgram.release(gl);
        return false;
    }

    if (!m_litProgram.initialize(gl, litVertexShader, litFragmentShader))
    {
        m_textureProgram.release(gl);
        m_solidColorProgram.release(gl);
        m_vertexColorProgram.release(gl);
        return false;
    }

    m_colorModelLocation = m_vertexColorProgram.uniformLocation(gl, "model");
    m_colorViewLocation = m_vertexColorProgram.uniformLocation(gl, "view");
    m_colorProjectionLocation = m_vertexColorProgram.uniformLocation(gl, "projection");

    m_solidModelLocation = m_solidColorProgram.uniformLocation(gl, "model");
    m_solidViewLocation = m_solidColorProgram.uniformLocation(gl, "view");
    m_solidProjectionLocation = m_solidColorProgram.uniformLocation(gl, "projection");
    m_solidColorLocation = m_solidColorProgram.uniformLocation(gl, "color");

    m_textureModelLocation = m_textureProgram.uniformLocation(gl, "model");
    m_textureViewLocation = m_textureProgram.uniformLocation(gl, "view");
    m_textureProjectionLocation = m_textureProgram.uniformLocation(gl, "projection");
    m_textureSamplerLocation = m_textureProgram.uniformLocation(gl, "texture0");
    m_textureColorLocation = m_textureProgram.uniformLocation(gl, "color");

    m_litModelLocation = m_litProgram.uniformLocation(gl, "model");
    m_litViewLocation = m_litProgram.uniformLocation(gl, "view");
    m_litProjectionLocation = m_litProgram.uniformLocation(gl, "projection");
    m_litNormalLocation = m_litProgram.uniformLocation(gl, "normalMatrix");

    m_litBaseColorLocation = m_litProgram.uniformLocation(gl, "baseColor");
    m_litUseVertexColorLocation = m_litProgram.uniformLocation(gl, "useVertexColor");

    m_litAmbientLightLocation = m_litProgram.uniformLocation(gl, "ambientLight");

    m_litLightCountLocation = m_litProgram.uniformLocation(gl, "lightCount");
    m_litLightTypeLocation = m_litProgram.uniformLocation(gl, "lightType[0]");
    m_litLightPositionLocation = m_litProgram.uniformLocation(gl, "lightPosition[0]");
    m_litLightDirectionLocation = m_litProgram.uniformLocation(gl, "lightDirection[0]");
    m_litLightColorLocation = m_litProgram.uniformLocation(gl, "lightColor[0]");
    m_litLightIntensityLocation = m_litProgram.uniformLocation(gl, "lightIntensity[0]");
    m_litLightRangeLocation = m_litProgram.uniformLocation(gl, "lightRange[0]");
    m_litLightInnerConeCosLocation = m_litProgram.uniformLocation(gl, "lightInnerConeCos[0]");
    m_litLightOuterConeCosLocation = m_litProgram.uniformLocation(gl, "lightOuterConeCos[0]");

    if (m_colorModelLocation < 0 || m_colorViewLocation < 0 || m_colorProjectionLocation < 0 ||
        m_solidModelLocation < 0 || m_solidViewLocation < 0 || m_solidProjectionLocation < 0 || m_solidColorLocation < 0 ||
        m_textureModelLocation < 0 || m_textureViewLocation < 0 || m_textureProjectionLocation < 0 ||
        m_textureSamplerLocation < 0 || m_textureColorLocation < 0 ||
        m_litModelLocation < 0 || m_litViewLocation < 0 || m_litProjectionLocation < 0 || m_litNormalLocation < 0 ||
        m_litBaseColorLocation < 0 || m_litUseVertexColorLocation < 0 || m_litAmbientLightLocation < 0 ||
        m_litLightCountLocation < 0 || m_litLightTypeLocation < 0 ||
        m_litLightPositionLocation < 0 || m_litLightDirectionLocation < 0 ||
        m_litLightColorLocation < 0 || m_litLightIntensityLocation < 0 ||
        m_litLightRangeLocation < 0 || m_litLightInnerConeCosLocation < 0 ||
        m_litLightOuterConeCosLocation < 0)
    {
        qWarning() << "Renderer initialize failed: required Shader Uniform was not found.";

        m_litProgram.release(gl);
        m_textureProgram.release(gl);
        m_solidColorProgram.release(gl);
        m_vertexColorProgram.release(gl);

        return false;
    }

    m_openGLContext = openGLContext;
    m_initialized = true;

    return true;
}

void Renderer::release()
{
    if (!m_initialized)
        return;

    QOpenGLFunctions_3_3_Core* gl = m_openGLContext != 0 ? m_openGLContext->gl() : 0;

    if (gl == 0)
    {
        qWarning() << "Renderer release failed: OpenGL functions are unavailable.";
        return;
    }

    m_vertexColorProgram.release(gl);
    m_solidColorProgram.release(gl);
    m_textureProgram.release(gl);
    m_litProgram.release(gl);

    m_openGLContext = 0;

    m_initialized = false;
    m_frameActive = false;
    m_lightLimitWarningIssued = false;
}

/// Render State

void Renderer::setClearColor(const QVector4D& color)
{
    m_clearColor = color;
}

const QVector4D& Renderer::clearColor() const
{
    return m_clearColor;
}

/// Frame

bool Renderer::beginFrame(const RenderContext& context)
{
    if (!m_initialized)
    {
        qWarning() << "Renderer beginFrame failed: renderer is not initialized.";
        return false;
    }

    if (m_frameActive)
    {
        qWarning() << "Renderer beginFrame failed: a frame is already active.";
        return false;
    }

    if (!context.isValid())
    {
        qWarning() << "Renderer beginFrame failed: RenderContext is invalid.";
        return false;
    }

    QOpenGLFunctions_3_3_Core* gl = m_openGLContext->gl();

    if (gl == 0)
        return false;

    m_renderContext = context;

    gl->glViewport(0, 0, context.viewportWidth, context.viewportHeight);

    gl->glEnable(GL_DEPTH_TEST);
    gl->glDepthFunc(GL_LESS);
    gl->glDepthMask(GL_TRUE);

    gl->glDisable(GL_BLEND);

    gl->glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    gl->glLineWidth(1.0f);

    gl->glClearColor(m_clearColor.x(), m_clearColor.y(), m_clearColor.z(), m_clearColor.w());
    gl->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_frameActive = true;

    return true;
}

void Renderer::endFrame()
{
    if (!m_frameActive)
        return;

    QOpenGLFunctions_3_3_Core* gl = m_openGLContext->gl();

    if (gl == 0)
        return;

    gl->glBindVertexArray(0);

    gl->glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    gl->glDisable(GL_POLYGON_OFFSET_LINE);
    gl->glLineWidth(1.0f);

    gl->glDepthFunc(GL_LESS);
    gl->glDepthMask(GL_TRUE);
    gl->glEnable(GL_DEPTH_TEST);

    gl->glDisable(GL_BLEND);

    ShaderProgram::unbind(gl);

    m_frameActive = false;
}

/// Geometry Draw

bool Renderer::clearDepth(const RenderViewport& viewport)
{
    if (!m_frameActive)
    {
        qWarning() << "Renderer clearDepth failed: beginFrame() has not been called.";
        return false;
    }

    if (!viewport.isValid())
    {
        qWarning() << "Renderer clearDepth failed: RenderViewport is invalid.";
        return false;
    }

    QOpenGLFunctions_3_3_Core* gl = m_openGLContext->gl();

    if (gl == 0)
        return false;

    /// 清除 Depth Buffer 前必须打开 Depth Write，否则 glClear 不会修改 Depth Buffer。
    gl->glDepthMask(GL_TRUE);

    gl->glEnable(GL_SCISSOR_TEST);
    gl->glScissor(viewport.x, viewport.y, viewport.width, viewport.height);
    gl->glClear(GL_DEPTH_BUFFER_BIT);
    gl->glDisable(GL_SCISSOR_TEST);

    return true;
}

bool Renderer::drawGeometry(const Geometry* geometry,
                            const Material* material,
                            const RenderState& state,
                            const std::vector<const Light*>& lights)
{
    return drawGeometryStates(geometry, material, &state, 1, lights);
}

bool Renderer::drawGeometry(const Geometry* geometry,
                            const Material* material,
                            const std::vector<RenderState>& states,
                            const std::vector<const Light*>& lights)
{
    return drawGeometryStates(geometry,
                              material,
                              states.empty() ? 0 : &states[0],
                              states.size(),
                              lights);
}

bool Renderer::drawGeometryStates(const Geometry* geometry,
                                  const Material* material,
                                  const RenderState* states,
                                  std::size_t stateCount,
                                  const std::vector<const Light*>& lights)
{
    if (!m_frameActive)
    {
        qWarning() << "Renderer drawGeometry failed: beginFrame() has not been called.";
        return false;
    }

    if (geometry == 0 || material == 0)
    {
        qWarning() << "Renderer drawGeometry failed: invalid Geometry or Material.";
        return false;
    }

    /// 空 State 集合表示本次没有需要绘制的对象，不视为错误。
    if (stateCount == 0)
        return true;

    if (states == 0)
    {
        qWarning() << "Renderer drawGeometry failed: RenderState array is null.";
        return false;
    }

    /// 无光照材质。
    if (!material->lightingEnabled())
    {
        switch (material->surfaceMode())
        {
        case SurfaceMode::Color:
            return drawColorGeometry(geometry, material->color(), states, stateCount);

        case SurfaceMode::VertexColor:
            return drawVertexColorGeometry(geometry, states, stateCount);

        case SurfaceMode::Texture:
            return drawTextureGeometry(geometry, material, states, stateCount);
        }

        qWarning() << "Renderer drawGeometry failed: unsupported Material surface mode:"
                   << material->name();

        return false;
    }

    /// 当前基础 Renderer 不支持 Texture SurfaceMode 参与光照。
    if (material->surfaceMode() == SurfaceMode::Texture)
    {
        qWarning() << "Renderer drawGeometry failed: lit texture material is not supported:"
                   << material->name();

        return false;
    }

    return drawLitGeometry(geometry, material, states, stateCount, lights);
}

/// Color

bool Renderer::drawColorGeometry(const Geometry* geometry,
                                 const QVector4D& color,
                                 const RenderState* states,
                                 std::size_t stateCount)
{
    if (geometry == 0 || states == 0 || stateCount == 0)
        return false;

    if (!geometry->isInitialized() || geometry->vao() == 0)
    {
        qWarning() << "Renderer drawColorGeometry failed: Geometry GPU resource is not initialized:"
                   << geometry->name();

        return false;
    }

    if (geometry->indexCount() <= 0)
    {
        qWarning() << "Renderer drawColorGeometry failed: Geometry contains no indices:"
                   << geometry->name();

        return false;
    }

    if (!geometry->hasAttribute(GeometryAttribute::Position, 3))
    {
        qWarning() << "Renderer drawColorGeometry failed: position layout is required:"
                   << geometry->name();

        return false;
    }

    QOpenGLFunctions_3_3_Core* gl = m_openGLContext->gl();

    if (gl == 0)
        return false;

    if (!geometry->prepareDrawGL(gl))
        return false;

    m_solidColorProgram.bind(gl);

    /// 所有 RenderState 共用同一个 Material Color，因此只需要上传一次。
    gl->glUniform4f(m_solidColorLocation, color.x(), color.y(), color.z(), color.w());

    gl->glBindVertexArray(geometry->vao());

    const bool lineGeometry =geometry->renderType() == RenderType::Lines ||geometry->renderType() == RenderType::LineStrip;

    if (lineGeometry)gl->glLineWidth(geometry->lineWidth());

    bool result = true;

    for (std::size_t i = 0; i < stateCount; ++i)
    {
        const RenderState& state = states[i];

        if (!applyRenderState(state))
        {
            result = false;
            break;
        }

        gl->glUniformMatrix4fv(m_solidModelLocation, 1, GL_FALSE, state.model.constData());
        gl->glUniformMatrix4fv(m_solidViewLocation, 1, GL_FALSE, state.view.constData());
        gl->glUniformMatrix4fv(m_solidProjectionLocation, 1, GL_FALSE, state.projection.constData());

        gl->glDrawElements(primitiveMode(geometry),geometry->indexCount(),geometry->indexType(),0);
    }

    if (lineGeometry)gl->glLineWidth(1.0f);

    gl->glBindVertexArray(0);

    geometry->finishDrawGL(gl);

    return result;
}

/// Vertex Color

bool Renderer::drawVertexColorGeometry(const Geometry* geometry,
                                       const RenderState* states,
                                       std::size_t stateCount)
{
    if (geometry == 0 || states == 0 || stateCount == 0)
        return false;

    if (!geometry->isInitialized() || geometry->vao() == 0)
    {
        qWarning() << "Renderer drawVertexColorGeometry failed: Geometry GPU resource is not initialized:"
                   << geometry->name();

        return false;
    }

    if (geometry->indexCount() <= 0)
    {
        qWarning() << "Renderer drawVertexColorGeometry failed: Geometry contains no indices:"
                   << geometry->name();

        return false;
    }

    if (!geometry->hasAttribute(GeometryAttribute::Position, 3) ||
        !geometry->hasAttribute(GeometryAttribute::Color, 3))
    {
        qWarning() << "Renderer drawVertexColorGeometry failed: position + color layout is required:"
                   << geometry->name();

        return false;
    }

    QOpenGLFunctions_3_3_Core* gl = m_openGLContext->gl();

    if (gl == 0)
        return false;

    if (!geometry->prepareDrawGL(gl))
        return false;

    m_vertexColorProgram.bind(gl);

    gl->glBindVertexArray(geometry->vao());

    const bool lineGeometry =
        geometry->renderType() == RenderType::Lines ||
        geometry->renderType() == RenderType::LineStrip;

    if (lineGeometry)
        gl->glLineWidth(geometry->lineWidth());

    bool result = true;

    for (std::size_t i = 0; i < stateCount; ++i)
    {
        const RenderState& state = states[i];

        if (!applyRenderState(state))
        {
            result = false;
            break;
        }

        gl->glUniformMatrix4fv(m_colorModelLocation, 1, GL_FALSE, state.model.constData());
        gl->glUniformMatrix4fv(m_colorViewLocation, 1, GL_FALSE, state.view.constData());
        gl->glUniformMatrix4fv(m_colorProjectionLocation, 1, GL_FALSE, state.projection.constData());

        gl->glDrawElements(primitiveMode(geometry),
                           geometry->indexCount(),
                           geometry->indexType(),
                           0);
    }

    if (lineGeometry)
        gl->glLineWidth(1.0f);

    gl->glBindVertexArray(0);

    geometry->finishDrawGL(gl);

    return result;
}

/// Texture

bool Renderer::drawTextureGeometry(const Geometry* geometry,
                                   const Material* material,
                                   const RenderState* states,
                                   std::size_t stateCount)
{
    if (geometry == 0 || material == 0 || states == 0 || stateCount == 0)
        return false;

    if (!geometry->isInitialized() || geometry->vao() == 0)
    {
        qWarning() << "Renderer drawTextureGeometry failed: Geometry GPU resource is not initialized:"
                   << geometry->name();

        return false;
    }

    if (geometry->indexCount() <= 0)
    {
        qWarning() << "Renderer drawTextureGeometry failed: Geometry contains no indices:"
                   << geometry->name();

        return false;
    }

    if (!geometry->hasAttribute(GeometryAttribute::Position, 3) ||
        !geometry->hasAttribute(GeometryAttribute::TexCoord, 2))
    {
        qWarning() << "Renderer drawTextureGeometry failed: position + texcoord layout is required:"
                   << geometry->name();

        return false;
    }

    const Texture* texture = material->texture();

    if (texture == 0)
    {
        qWarning() << "Renderer drawTextureGeometry failed: Material texture is null:"
                   << material->name();

        return false;
    }

    if (!texture->isInitialized() || texture->textureId() == 0)
    {
        qWarning() << "Renderer drawTextureGeometry failed: Texture GPU resource is not initialized:"
                   << texture->name();

        return false;
    }

    QOpenGLFunctions_3_3_Core* gl = m_openGLContext->gl();

    if (gl == 0)
        return false;

    if (!geometry->prepareDrawGL(gl))
        return false;

    const QVector4D& color = material->color();

    m_textureProgram.bind(gl);

    /// Material Color 和 Texture 对所有 RenderState 相同，因此只配置一次。
    gl->glUniform4f(m_textureColorLocation, color.x(), color.y(), color.z(), color.w());

    gl->glActiveTexture(GL_TEXTURE0);
    gl->glBindTexture(GL_TEXTURE_2D, texture->textureId());
    gl->glUniform1i(m_textureSamplerLocation, 0);

    gl->glBindVertexArray(geometry->vao());

    const bool lineGeometry =
        geometry->renderType() == RenderType::Lines ||
        geometry->renderType() == RenderType::LineStrip;

    if (lineGeometry)
        gl->glLineWidth(geometry->lineWidth());

    bool result = true;

    for (std::size_t i = 0; i < stateCount; ++i)
    {
        const RenderState& state = states[i];

        if (!applyRenderState(state))
        {
            result = false;
            break;
        }

        gl->glUniformMatrix4fv(m_textureModelLocation, 1, GL_FALSE, state.model.constData());
        gl->glUniformMatrix4fv(m_textureViewLocation, 1, GL_FALSE, state.view.constData());
        gl->glUniformMatrix4fv(m_textureProjectionLocation, 1, GL_FALSE, state.projection.constData());

        gl->glDrawElements(primitiveMode(geometry),
                           geometry->indexCount(),
                           geometry->indexType(),
                           0);
    }

    if (lineGeometry)
        gl->glLineWidth(1.0f);

    gl->glBindVertexArray(0);

    gl->glBindTexture(GL_TEXTURE_2D, 0);

    geometry->finishDrawGL(gl);

    return result;
}

/// Lit

bool Renderer::drawLitGeometry(const Geometry* geometry,
                               const Material* material,
                               const RenderState* states,
                               std::size_t stateCount,
                               const std::vector<const Light*>& lights)
{
    if (geometry == 0 || material == 0 || states == 0 || stateCount == 0)
        return false;

    if (!geometry->isInitialized() || geometry->vao() == 0)
    {
        qWarning() << "Renderer drawLitGeometry failed: Geometry GPU resource is not initialized:"
                   << geometry->name();

        return false;
    }

    if (geometry->indexCount() <= 0)
    {
        qWarning() << "Renderer drawLitGeometry failed: Geometry contains no indices:"
                   << geometry->name();

        return false;
    }

    if (!geometry->hasAttribute(GeometryAttribute::Position, 3) ||
        !geometry->hasAttribute(GeometryAttribute::Normal, 3))
    {
        qWarning() << "Renderer drawLitGeometry failed: position + normal layout is required:"
                   << geometry->name();

        return false;
    }

    const bool useVertexColor = material->surfaceMode() == SurfaceMode::VertexColor;

    if (material->surfaceMode() != SurfaceMode::Color && !useVertexColor)
    {
        qWarning() << "Renderer drawLitGeometry failed: unsupported Material surface mode:"
                   << material->name();

        return false;
    }

    if (useVertexColor && !geometry->hasAttribute(GeometryAttribute::Color, 3))
    {
        qWarning() << "Renderer drawLitGeometry failed: VertexColor requires color layout:"
                   << geometry->name();

        return false;
    }

    QOpenGLFunctions_3_3_Core* gl = m_openGLContext->gl();

    if (gl == 0)
        return false;

    QVector3D ambientLight(0.0f, 0.0f, 0.0f);

    GLint lightTypes[MaxLights] = { 0 };

    GLfloat lightPositions[MaxLights * 3] = { 0.0f };
    GLfloat lightDirections[MaxLights * 3] = { 0.0f };
    GLfloat lightColors[MaxLights * 3] = { 0.0f };

    GLfloat lightIntensities[MaxLights] = { 0.0f };
    GLfloat lightRanges[MaxLights] = { 1.0f };

    GLfloat lightInnerConeCos[MaxLights] = { 1.0f };
    GLfloat lightOuterConeCos[MaxLights] = { 1.0f };

    /// π / 180，用于将 Light 中以 Degree 保存的 Spot Cone Angle 转换为 Radian。
    const float degreesToRadians = 0.017453292519943295f;

    int lightCount = 0;
    int nonAmbientLightCount = 0;

    for (std::size_t i = 0; i < lights.size(); ++i)
    {
        const Light* light = lights[i];

        if (light == 0)
            continue;

        /// Renderer 不检查 Light::isEnabled()。
        /// 调用者放入 lights 的 Light 均视为本次 Draw 的有效输入。
        if (light->lightType() == LightType::Ambient)
        {
            ambientLight += light->color() * light->intensity();
            continue;
        }

        ++nonAmbientLightCount;

        if (lightCount >= MaxLights)
            continue;

        GLint shaderLightType = 0;

        switch (light->lightType())
        {
        case LightType::Directional:
            shaderLightType = 0;
            break;

        case LightType::Point:
            shaderLightType = 1;
            break;

        case LightType::Spot:
            shaderLightType = 2;
            break;

        case LightType::Ambient:
            continue;
        }

        const QVector3D& position = light->position();
        const QVector3D& direction = light->direction();
        const QVector3D& color = light->color();

        /// 每个 vec3 在连续 GLfloat 数组中占用三个元素。
        const int vectorOffset = lightCount * 3;

        lightTypes[lightCount] = shaderLightType;

        lightPositions[vectorOffset + 0] = position.x();
        lightPositions[vectorOffset + 1] = position.y();
        lightPositions[vectorOffset + 2] = position.z();

        lightDirections[vectorOffset + 0] = direction.x();
        lightDirections[vectorOffset + 1] = direction.y();
        lightDirections[vectorOffset + 2] = direction.z();

        lightColors[vectorOffset + 0] = color.x();
        lightColors[vectorOffset + 1] = color.y();
        lightColors[vectorOffset + 2] = color.z();

        lightIntensities[lightCount] = light->intensity();
        lightRanges[lightCount] = light->range();

        lightInnerConeCos[lightCount] = qCos(light->innerConeAngle() * degreesToRadians);
        lightOuterConeCos[lightCount] = qCos(light->outerConeAngle() * degreesToRadians);

        ++lightCount;
    }

    if (nonAmbientLightCount > MaxLights && !m_lightLimitWarningIssued)
    {
        qWarning() << "Renderer drawLitGeometry: non-ambient light count exceeds MaxLights; extra lights are ignored:"
                   << "Lights=" << nonAmbientLightCount
                   << "MaxLights=" << MaxLights;

        m_lightLimitWarningIssued = true;
    }

    if (!geometry->prepareDrawGL(gl))
        return false;

    const QVector4D& baseColor = material->color();

    m_litProgram.bind(gl);

    /// Material 与 Light 对全部 RenderState 相同，因此只上传一次。
    gl->glUniform4f(m_litBaseColorLocation,
                    baseColor.x(),
                    baseColor.y(),
                    baseColor.z(),
                    baseColor.w());

    gl->glUniform1i(m_litUseVertexColorLocation, useVertexColor ? 1 : 0);

    gl->glUniform3f(m_litAmbientLightLocation,
                    ambientLight.x(),
                    ambientLight.y(),
                    ambientLight.z());

    gl->glUniform1i(m_litLightCountLocation, lightCount);

    if (lightCount > 0)
    {
        gl->glUniform1iv(m_litLightTypeLocation, lightCount, lightTypes);

        gl->glUniform3fv(m_litLightPositionLocation, lightCount, lightPositions);
        gl->glUniform3fv(m_litLightDirectionLocation, lightCount, lightDirections);
        gl->glUniform3fv(m_litLightColorLocation, lightCount, lightColors);

        gl->glUniform1fv(m_litLightIntensityLocation, lightCount, lightIntensities);
        gl->glUniform1fv(m_litLightRangeLocation, lightCount, lightRanges);

        gl->glUniform1fv(m_litLightInnerConeCosLocation, lightCount, lightInnerConeCos);
        gl->glUniform1fv(m_litLightOuterConeCosLocation, lightCount, lightOuterConeCos);
    }

    gl->glBindVertexArray(geometry->vao());

    const bool lineGeometry =
        geometry->renderType() == RenderType::Lines ||
        geometry->renderType() == RenderType::LineStrip;

    if (lineGeometry)
        gl->glLineWidth(geometry->lineWidth());

    bool result = true;

    for (std::size_t i = 0; i < stateCount; ++i)
    {
        const RenderState& state = states[i];

        if (!applyRenderState(state))
        {
            result = false;
            break;
        }

        /// Normal Matrix 依赖当前 Model，因此必须针对每个 RenderState 单独计算。
        const QMatrix3x3 normalMatrix = state.model.normalMatrix();

        gl->glUniformMatrix4fv(m_litModelLocation, 1, GL_FALSE, state.model.constData());
        gl->glUniformMatrix4fv(m_litViewLocation, 1, GL_FALSE, state.view.constData());
        gl->glUniformMatrix4fv(m_litProjectionLocation, 1, GL_FALSE, state.projection.constData());
        gl->glUniformMatrix3fv(m_litNormalLocation, 1, GL_FALSE, normalMatrix.constData());

        gl->glDrawElements(primitiveMode(geometry),
                           geometry->indexCount(),
                           geometry->indexType(),
                           0);
    }

    if (lineGeometry)
        gl->glLineWidth(1.0f);

    gl->glBindVertexArray(0);

    geometry->finishDrawGL(gl);

    return result;
}

/// Wireframe

bool Renderer::drawWireGeometry(const Geometry* geometry,
                                const QVector4D& color,
                                const RenderState& state,
                                bool overlay)
{
    return drawWireGeometryStates(geometry, color, &state, 1, overlay);
}

bool Renderer::drawWireGeometry(const Geometry* geometry,
                                const QVector4D& color,
                                const std::vector<RenderState>& states,
                                bool overlay)
{
    return drawWireGeometryStates(geometry,
                                  color,
                                  states.empty() ? 0 : &states[0],
                                  states.size(),
                                  overlay);
}

bool Renderer::drawWireGeometryStates(const Geometry* geometry,
                                      const QVector4D& color,
                                      const RenderState* states,
                                      std::size_t stateCount,
                                      bool overlay)
{
    if (!m_frameActive)
    {
        qWarning() << "Renderer drawWireGeometry failed: beginFrame() has not been called.";
        return false;
    }

    if (geometry == 0)
    {
        qWarning() << "Renderer drawWireGeometry failed: Geometry is null.";
        return false;
    }

    /// 空 State 集合表示本次没有需要绘制的对象，不视为错误。
    if (stateCount == 0)
        return true;

    if (states == 0)
    {
        qWarning() << "Renderer drawWireGeometry failed: RenderState array is null.";
        return false;
    }

    if (geometry->renderType() != RenderType::Triangles)
    {
        qWarning() << "Renderer drawWireGeometry failed: Triangle Geometry is required:"
                   << geometry->name();

        return false;
    }

    if (!geometry->isInitialized() || geometry->vao() == 0)
    {
        qWarning() << "Renderer drawWireGeometry failed: Geometry GPU resource is not initialized:"
                   << geometry->name();

        return false;
    }

    if (geometry->indexCount() <= 0)
    {
        qWarning() << "Renderer drawWireGeometry failed: Geometry contains no indices:"
                   << geometry->name();

        return false;
    }

    if (!geometry->hasAttribute(GeometryAttribute::Position, 3))
    {
        qWarning() << "Renderer drawWireGeometry failed: position layout is required:"
                   << geometry->name();

        return false;
    }

    QOpenGLFunctions_3_3_Core* gl = m_openGLContext->gl();

    if (gl == 0)
        return false;

    if (!geometry->prepareDrawGL(gl))
        return false;

    m_solidColorProgram.bind(gl);

    /// Wireframe Color 对全部 RenderState 相同，因此只上传一次。
    gl->glUniform4f(m_solidColorLocation,
                    color.x(),
                    color.y(),
                    color.z(),
                    color.w());

    gl->glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    gl->glBindVertexArray(geometry->vao());

    bool result = true;

    for (std::size_t i = 0; i < stateCount; ++i)
    {
        const RenderState& state = states[i];

        if (!applyRenderState(state))
        {
            result = false;
            break;
        }

        const bool useOverlayDepth = overlay && state.depthTestEnabled;

        if (useOverlayDepth)
        {
            /// LEQUAL 配合负 Polygon Offset，使边线稳定显示在已经绘制的实体表面。
            gl->glDepthFunc(GL_LEQUAL);
            gl->glEnable(GL_POLYGON_OFFSET_LINE);
            gl->glPolygonOffset(-1.0f, -1.0f);
        }

        gl->glUniformMatrix4fv(m_solidModelLocation, 1, GL_FALSE, state.model.constData());
        gl->glUniformMatrix4fv(m_solidViewLocation, 1, GL_FALSE, state.view.constData());
        gl->glUniformMatrix4fv(m_solidProjectionLocation, 1, GL_FALSE, state.projection.constData());

        gl->glDrawElements(GL_TRIANGLES,
                           geometry->indexCount(),
                           geometry->indexType(),
                           0);

        if (useOverlayDepth)
        {
            gl->glDisable(GL_POLYGON_OFFSET_LINE);
            gl->glDepthFunc(GL_LESS);
        }
    }

    gl->glBindVertexArray(0);

    gl->glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    gl->glDisable(GL_POLYGON_OFFSET_LINE);
    gl->glDepthFunc(GL_LESS);

    geometry->finishDrawGL(gl);

    return result;
}

/// Render State

bool Renderer::applyRenderState(const RenderState& state)
{
    if (!state.viewport.isValid())
    {
        qWarning() << "Renderer applyRenderState failed: RenderViewport is invalid.";
        return false;
    }

    QOpenGLFunctions_3_3_Core* gl = m_openGLContext->gl();

    if (gl == 0)
        return false;

    gl->glViewport(state.viewport.x,
                   state.viewport.y,
                   state.viewport.width,
                   state.viewport.height);

    if (state.depthTestEnabled)
        gl->glEnable(GL_DEPTH_TEST);
    else
        gl->glDisable(GL_DEPTH_TEST);

    gl->glDepthMask(state.depthWriteEnabled ? GL_TRUE : GL_FALSE);

    /// 普通 Geometry Draw 固定使用 GL_LESS。
    /// Wire Overlay 会在自己的 Draw Scope 内临时切换为 GL_LEQUAL。
    gl->glDepthFunc(GL_LESS);

    if (state.blendEnabled)
    {
        gl->glEnable(GL_BLEND);
        gl->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    else
    {
        gl->glDisable(GL_BLEND);
    }

    return true;
}

/// Geometry

GLenum Renderer::primitiveMode(const Geometry* geometry) const
{
    if (geometry == 0)
        return GL_TRIANGLES;

    switch (geometry->renderType())
    {
    case RenderType::Triangles:
        return GL_TRIANGLES;

    case RenderType::Lines:
        return GL_LINES;

    case RenderType::LineStrip:
        return GL_LINE_STRIP;

    case RenderType::Points:
        return GL_POINTS;
    }

    return GL_TRIANGLES;
}