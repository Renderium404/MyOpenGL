#include "Renderer.h"

#include "MyOpenGL/Light/Light.h"
#include "MyOpenGL/Light/LightManager.h"
#include "MyOpenGL/Material/Material.h"
#include "MyOpenGL/Render/MyOpenGLContext.h"
#include "MyOpenGL/Resource/Geometry.h"

#include <QDebug>
#include <QMatrix3x3>
#include <QtMath>

#include <vector>

Renderer::Renderer()
    : m_openGLContext(0)
    , m_colorModelLocation(-1)
    , m_colorViewLocation(-1)
    , m_colorProjectionLocation(-1)
    , m_solidModelLocation(-1)
    , m_solidViewLocation(-1)
    , m_solidProjectionLocation(-1)
    , m_solidColorLocation(-1)
    , m_litModelLocation(-1)
    , m_litViewLocation(-1)
    , m_litProjectionLocation(-1)
    , m_litNormalLocation(-1)
    , m_litCameraPositionLocation(-1)
    , m_litBaseColorLocation(-1)
    , m_litSpecularColorLocation(-1)
    , m_litShininessLocation(-1)
    , m_litUseVertexColorLocation(-1)
    , m_litAmbientColorLocation(-1)
    , m_litAmbientIntensityLocation(-1)
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
        "layout(location = 1) in vec3 aColor;\n"
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
        "layout(location = 3) in vec4 aVertexColor;\n"
        "uniform mat4 model;\n"
        "uniform mat4 view;\n"
        "uniform mat4 projection;\n"
        "uniform mat3 normalMatrix;\n"
        "out vec3 fragmentPosition;\n"
        "out vec3 fragmentNormal;\n"
        "out vec4 vertexColor;\n"
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
        "in vec4 vertexColor;\n"
        "\n"
        "uniform vec3 cameraPosition;\n"
        "uniform vec4 baseColor;\n"
        "uniform vec3 specularColor;\n"
        "uniform float shininess;\n"
        "uniform bool useVertexColor;\n"
        "\n"
        "uniform vec3 ambientColor;\n"
        "uniform float ambientIntensity;\n"
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
        "    vec4 surfaceColor = baseColor;\n"
        "\n"
        "    if (useVertexColor)\n"
        "        surfaceColor *= vertexColor;\n"
        "\n"
        "    vec3 normal = normalize(fragmentNormal);\n"
        "    vec3 viewDirection = normalize(cameraPosition - fragmentPosition);\n"
        "\n"
        "    vec3 diffuseAccumulation = vec3(0.0);\n"
        "    vec3 specularAccumulation = vec3(0.0);\n"
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
        "\n"
        "        diffuseAccumulation += surfaceColor.rgb * radiance * diffuseFactor;\n"
        "\n"
        "        vec3 halfDirection = normalize(toLight + viewDirection);\n"
        "        float specularFactor = pow(max(dot(normal, halfDirection), 0.0), shininess);\n"
        "        specularAccumulation += specularColor * radiance * specularFactor;\n"
        "    }\n"
        "\n"
        "    vec3 ambient = surfaceColor.rgb * ambientColor * ambientIntensity;\n"
        "    FragColor = vec4(ambient + diffuseAccumulation + specularAccumulation, surfaceColor.a);\n"
        "}\n";

    if (!m_vertexColorProgram.initialize(gl, vertexColorVertexShader, vertexColorFragmentShader))
        return false;

    if (!m_solidColorProgram.initialize(gl, solidColorVertexShader, solidColorFragmentShader))
    {
        m_vertexColorProgram.release(gl);
        return false;
    }

    if (!m_litProgram.initialize(gl, litVertexShader, litFragmentShader))
    {
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

    m_litModelLocation = m_litProgram.uniformLocation(gl, "model");
    m_litViewLocation = m_litProgram.uniformLocation(gl, "view");
    m_litProjectionLocation = m_litProgram.uniformLocation(gl, "projection");
    m_litNormalLocation = m_litProgram.uniformLocation(gl, "normalMatrix");
    m_litCameraPositionLocation = m_litProgram.uniformLocation(gl, "cameraPosition");

    m_litBaseColorLocation = m_litProgram.uniformLocation(gl, "baseColor");
    m_litSpecularColorLocation = m_litProgram.uniformLocation(gl, "specularColor");
    m_litShininessLocation = m_litProgram.uniformLocation(gl, "shininess");
    m_litUseVertexColorLocation = m_litProgram.uniformLocation(gl, "useVertexColor");

    m_litAmbientColorLocation = m_litProgram.uniformLocation(gl, "ambientColor");
    m_litAmbientIntensityLocation = m_litProgram.uniformLocation(gl, "ambientIntensity");

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
        m_litModelLocation < 0 || m_litViewLocation < 0 || m_litProjectionLocation < 0 ||
        m_litNormalLocation < 0 || m_litCameraPositionLocation < 0 ||
        m_litBaseColorLocation < 0 || m_litSpecularColorLocation < 0 ||
        m_litShininessLocation < 0 || m_litUseVertexColorLocation < 0 ||
        m_litAmbientColorLocation < 0 || m_litAmbientIntensityLocation < 0 ||
        m_litLightCountLocation < 0 || m_litLightTypeLocation < 0 ||
        m_litLightPositionLocation < 0 || m_litLightDirectionLocation < 0 ||
        m_litLightColorLocation < 0 || m_litLightIntensityLocation < 0 ||
        m_litLightRangeLocation < 0 ||
        m_litLightInnerConeCosLocation < 0 || m_litLightOuterConeCosLocation < 0)
    {
        qWarning() << "Renderer initialize failed: required Shader Uniform was not found.";

        m_litProgram.release(gl);
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

    gl->glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

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

    gl->glDepthFunc(GL_LESS);
    gl->glDepthMask(GL_TRUE);
    gl->glEnable(GL_DEPTH_TEST);

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

    // glClear() 本身不受 glViewport() 限制，因此使用 Scissor
    // 将 Depth Clear 严格限制在导航器自己的屏幕区域。
    gl->glEnable(GL_SCISSOR_TEST);
    gl->glScissor(viewport.x, viewport.y, viewport.width, viewport.height);
    gl->glClear(GL_DEPTH_BUFFER_BIT);
    gl->glDisable(GL_SCISSOR_TEST);

    return true;
}
bool Renderer::drawGeometry(const Geometry* geometry, const Material* material, const RenderState& state, const LightManager* lightManager)
{
    if (!m_frameActive)
    {
        qWarning() << "Renderer drawGeometry failed: beginFrame() has not been called.";
        return false;
    }

    if (geometry == 0 || material == 0)
    {
        qWarning() << "Renderer drawGeometry failed: invalid argument.";
        return false;
    }

    switch (material->type())
    {
    case MaterialTypeVertexColor:
        return drawVertexColorGeometry(geometry, state);

    case MaterialTypeLit:
    case MaterialTypeLitVertexColor:
        if (lightManager == 0)
        {
            qWarning() << "Renderer drawGeometry failed: Lit Material requires LightManager:"
                       << material->name();
            return false;
        }

        return drawLitGeometry(geometry, material, state, lightManager);
    }

    qWarning() << "Renderer drawGeometry failed: unsupported Material type:" << material->name();
    return false;
}

/// Vertex Color

bool Renderer::drawVertexColorGeometry(const Geometry* geometry, const RenderState& state)
{
    if (geometry == 0)
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

    if (!geometry->hasAttribute(0, 3) || !geometry->hasAttribute(1, 3))
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

    if (!applyRenderState(state))
    {
        geometry->finishDrawGL(gl);
        return false;
    }

    m_vertexColorProgram.bind(gl);

    gl->glUniformMatrix4fv(m_colorModelLocation, 1, GL_FALSE, state.model.constData());
    gl->glUniformMatrix4fv(m_colorViewLocation, 1, GL_FALSE, state.view.constData());
    gl->glUniformMatrix4fv(m_colorProjectionLocation, 1, GL_FALSE, state.projection.constData());

    gl->glBindVertexArray(geometry->vao());
    gl->glDrawElements(primitiveMode(geometry), geometry->indexCount(), geometry->indexType(), 0);
    gl->glBindVertexArray(0);

    geometry->finishDrawGL(gl);

    return true;
}

/// Lit

bool Renderer::drawLitGeometry(const Geometry* geometry, const Material* material, const RenderState& state, const LightManager* lightManager)
{
    if (geometry == 0 || material == 0 || lightManager == 0)
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

    const bool useVertexColor = material->type() == MaterialTypeLitVertexColor;

    if (material->type() != MaterialTypeLit && !useVertexColor)
    {
        qWarning() << "Renderer drawLitGeometry failed: Lit or LitVertexColor Material is required:"
                   << material->name();
        return false;
    }

    if (!geometry->hasAttribute(0, 3) || !geometry->hasAttribute(1, 3))
    {
        qWarning() << "Renderer drawLitGeometry failed: position + normal layout is required:"
                   << geometry->name();
        return false;
    }

    if (useVertexColor && !geometry->hasAttribute(3, 4))
    {
        qWarning() << "Renderer drawLitGeometry failed: LitVertexColor requires color4 at attribute location 3:"
                   << geometry->name();
        return false;
    }

    QOpenGLFunctions_3_3_Core* gl = m_openGLContext->gl();

    if (gl == 0)
        return false;

    const QMatrix3x3 normalMatrix = state.model.normalMatrix();

    const QVector4D& baseColor = material->baseColor();
    const QVector3D& specularColor = material->specularColor();
    const QVector3D& ambientColor = lightManager->ambientColor();

    std::vector<const Light*> enabledLights;
    lightManager->enabledLights(enabledLights);

    if (enabledLights.size() > static_cast<std::size_t>(MaxLights) && !m_lightLimitWarningIssued)
    {
        qWarning() << "Renderer drawLitGeometry: enabled light count exceeds MaxLights; extra lights are ignored:"
                   << "Enabled=" << static_cast<int>(enabledLights.size())
                   << "MaxLights=" << MaxLights;

        m_lightLimitWarningIssued = true;
    }

    const int lightCount = qMin(static_cast<int>(enabledLights.size()), MaxLights);

    GLint lightTypes[MaxLights] = { 0 };

    GLfloat lightPositions[MaxLights * 3] = { 0.0f };
    GLfloat lightDirections[MaxLights * 3] = { 0.0f };
    GLfloat lightColors[MaxLights * 3] = { 0.0f };

    GLfloat lightIntensities[MaxLights] = { 0.0f };
    GLfloat lightRanges[MaxLights] = { 1.0f };

    GLfloat lightInnerConeCos[MaxLights] = { 1.0f };
    GLfloat lightOuterConeCos[MaxLights] = { 1.0f };

    const float degreesToRadians = 0.017453292519943295f;

    for (int lightIndex = 0; lightIndex < lightCount; ++lightIndex)
    {
        const Light* light = enabledLights[static_cast<std::size_t>(lightIndex)];

        if (light == 0)
            continue;

        const QVector3D& position = light->position();
        const QVector3D& direction = light->direction();
        const QVector3D& color = light->color();

        const int vectorOffset = lightIndex * 3;

        lightTypes[lightIndex] = static_cast<GLint>(light->type());

        lightPositions[vectorOffset + 0] = position.x();
        lightPositions[vectorOffset + 1] = position.y();
        lightPositions[vectorOffset + 2] = position.z();

        lightDirections[vectorOffset + 0] = direction.x();
        lightDirections[vectorOffset + 1] = direction.y();
        lightDirections[vectorOffset + 2] = direction.z();

        lightColors[vectorOffset + 0] = color.x();
        lightColors[vectorOffset + 1] = color.y();
        lightColors[vectorOffset + 2] = color.z();

        lightIntensities[lightIndex] = light->intensity();
        lightRanges[lightIndex] = light->range();

        lightInnerConeCos[lightIndex] = qCos(light->innerConeAngle() * degreesToRadians);
        lightOuterConeCos[lightIndex] = qCos(light->outerConeAngle() * degreesToRadians);
    }

    if (!geometry->prepareDrawGL(gl))
        return false;

    if (!applyRenderState(state))
    {
        geometry->finishDrawGL(gl);
        return false;
    }

    m_litProgram.bind(gl);

    gl->glUniformMatrix4fv(m_litModelLocation, 1, GL_FALSE, state.model.constData());
    gl->glUniformMatrix4fv(m_litViewLocation, 1, GL_FALSE, state.view.constData());
    gl->glUniformMatrix4fv(m_litProjectionLocation, 1, GL_FALSE, state.projection.constData());

    gl->glUniformMatrix3fv(m_litNormalLocation, 1, GL_FALSE, normalMatrix.constData());

    gl->glUniform3f(
        m_litCameraPositionLocation,
        m_renderContext.cameraPosition.x(),
        m_renderContext.cameraPosition.y(),
        m_renderContext.cameraPosition.z());

    gl->glUniform4f(
        m_litBaseColorLocation,
        baseColor.x(),
        baseColor.y(),
        baseColor.z(),
        baseColor.w());

    gl->glUniform3f(
        m_litSpecularColorLocation,
        specularColor.x(),
        specularColor.y(),
        specularColor.z());

    gl->glUniform1f(m_litShininessLocation, material->shininess());

    gl->glUniform1i(m_litUseVertexColorLocation, useVertexColor ? 1 : 0);

    gl->glUniform3f(
        m_litAmbientColorLocation,
        ambientColor.x(),
        ambientColor.y(),
        ambientColor.z());

    gl->glUniform1f(m_litAmbientIntensityLocation, lightManager->ambientIntensity());

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

    gl->glDrawElements(
        primitiveMode(geometry),
        geometry->indexCount(),
        geometry->indexType(),
        0);

    gl->glBindVertexArray(0);

    geometry->finishDrawGL(gl);

    return true;
}

/// Wireframe

bool Renderer::drawWireGeometry(const Geometry* geometry, const QVector4D& color, const RenderState& state, bool overlay)
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

    if (geometry->renderType() != Triangles)
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

    if (!geometry->hasAttribute(0, 3))
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

    if (!applyRenderState(state))
    {
        geometry->finishDrawGL(gl);
        return false;
    }

    if (overlay && state.depthTestEnabled)
    {
        gl->glDepthFunc(GL_LEQUAL);
        gl->glEnable(GL_POLYGON_OFFSET_LINE);
        gl->glPolygonOffset(-1.0f, -1.0f);
    }

    gl->glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    m_solidColorProgram.bind(gl);

    gl->glUniformMatrix4fv(m_solidModelLocation, 1, GL_FALSE, state.model.constData());
    gl->glUniformMatrix4fv(m_solidViewLocation, 1, GL_FALSE, state.view.constData());
    gl->glUniformMatrix4fv(m_solidProjectionLocation, 1, GL_FALSE, state.projection.constData());

    gl->glUniform4f(
        m_solidColorLocation,
        color.x(),
        color.y(),
        color.z(),
        color.w());

    gl->glBindVertexArray(geometry->vao());

    gl->glDrawElements(
        GL_TRIANGLES,
        geometry->indexCount(),
        geometry->indexType(),
        0);

    gl->glBindVertexArray(0);

    geometry->finishDrawGL(gl);

    gl->glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    if (overlay && state.depthTestEnabled)
    {
        gl->glDisable(GL_POLYGON_OFFSET_LINE);
        gl->glDepthFunc(GL_LESS);
    }

    return true;
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

    gl->glViewport(
        state.viewport.x,
        state.viewport.y,
        state.viewport.width,
        state.viewport.height);

    if (state.depthTestEnabled)
        gl->glEnable(GL_DEPTH_TEST);
    else
        gl->glDisable(GL_DEPTH_TEST);

    gl->glDepthMask(state.depthWriteEnabled ? GL_TRUE : GL_FALSE);

    // 当前基础 Renderer 的普通 Geometry Draw 固定使用 GL_LESS。
    // Wire Overlay 会在自身 Draw Scope 内临时切换为 GL_LEQUAL。
    gl->glDepthFunc(GL_LESS);

    return true;
}

/// Geometry

GLenum Renderer::primitiveMode(const Geometry* geometry) const
{
    if (geometry == 0)
        return GL_TRIANGLES;

    switch (geometry->renderType())
    {
    case Triangles:
        return GL_TRIANGLES;

    case Lines:
        return GL_LINES;

    case LineStrip:
        return GL_LINE_STRIP;
    }

    return GL_TRIANGLES;
}