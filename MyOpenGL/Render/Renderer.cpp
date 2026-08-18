#include "Renderer.h"

#include "MyOpenGL/Camera/Camera.h"
#include "MyOpenGL/Core/ResourceManager.h"
#include "MyOpenGL/Light/Light.h"
#include "MyOpenGL/Light/LightManager.h"
#include "MyOpenGL/Material/Material.h"
#include "MyOpenGL/Render/RenderContext.h"
#include "MyOpenGL/Resource/Geometry.h"
#include "MyOpenGL/Scene/RenderItem.h"
#include "MyOpenGL/Scene/Scene.h"

#include <QDebug>
#include <QMatrix3x3>
#include <QtMath>

#include <vector>

Renderer::Renderer()
    : m_context(0)
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
    , m_cameraPosition(0.0f, 0.0f, 0.0f)
    , m_clearColor(0.1f, 0.1f, 0.1f, 1.0f)
    , m_viewportWidth(0)
    , m_viewportHeight(0)
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

bool Renderer::initialize(RenderContext* context)
{
    if (context == 0 || !context->isInitialized())
    {
        qWarning() << "Renderer initialize failed: RenderContext is invalid.";
        return false;
    }

    if (m_initialized)
        return true;

    QOpenGLFunctions_3_3_Core* gl = context->gl();

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
        "in vec3 fragmentPosition;\n"
        "in vec3 fragmentNormal;\n"
        "in vec4 vertexColor;\n"
        "uniform vec3 cameraPosition;\n"
        "uniform vec4 baseColor;\n"
        "uniform vec3 specularColor;\n"
        "uniform float shininess;\n"
        "uniform bool useVertexColor;\n"
        "uniform vec3 ambientColor;\n"
        "uniform float ambientIntensity;\n"
        "uniform int lightCount;\n"
        "uniform int lightType[MaxLights];\n"
        "uniform vec3 lightPosition[MaxLights];\n"
        "uniform vec3 lightDirection[MaxLights];\n"
        "uniform vec3 lightColor[MaxLights];\n"
        "uniform float lightIntensity[MaxLights];\n"
        "uniform float lightRange[MaxLights];\n"
        "uniform float lightInnerConeCos[MaxLights];\n"
        "uniform float lightOuterConeCos[MaxLights];\n"
        "out vec4 FragColor;\n"
        "void main()\n"
        "{\n"
        "    vec4 surfaceColor = baseColor;\n"
        "    if (useVertexColor)\n"
        "        surfaceColor *= vertexColor;\n"
        "\n"
        "    vec3 normal = normalize(fragmentNormal);\n"
        "    vec3 viewDirection = normalize(cameraPosition - fragmentPosition);\n"
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
        m_litNormalLocation < 0 || m_litCameraPositionLocation < 0 || m_litBaseColorLocation < 0 ||
        m_litSpecularColorLocation < 0 || m_litShininessLocation < 0 ||
        m_litUseVertexColorLocation < 0 ||
        m_litAmbientColorLocation < 0 || m_litAmbientIntensityLocation < 0 ||
        m_litLightCountLocation < 0 || m_litLightTypeLocation < 0 || m_litLightPositionLocation < 0 ||
        m_litLightDirectionLocation < 0 || m_litLightColorLocation < 0 || m_litLightIntensityLocation < 0 ||
        m_litLightRangeLocation < 0 || m_litLightInnerConeCosLocation < 0 || m_litLightOuterConeCosLocation < 0)
    {
        qWarning() << "Renderer initialize failed: required Shader Uniform was not found.";
        m_vertexColorProgram.release(gl);
        m_solidColorProgram.release(gl);
        m_litProgram.release(gl);
        return false;
    }

    m_context = context;
    m_initialized = true;
    return true;
}

void Renderer::release()
{
    if (!m_initialized)
        return;

    QOpenGLFunctions_3_3_Core* gl = m_context->gl();

    if (gl == 0)
    {
        qWarning() << "Renderer release failed: OpenGL functions are unavailable.";
        return;
    }

    m_vertexColorProgram.release(gl);
    m_solidColorProgram.release(gl);
    m_litProgram.release(gl);

    m_context = 0;
    m_viewportWidth = 0;
    m_viewportHeight = 0;
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

bool Renderer::beginFrame(const Camera* camera, int viewportWidth, int viewportHeight)
{
    if (!m_initialized)
    {
        qWarning() << "Renderer beginFrame failed: renderer is not initialized.";
        return false;
    }

    if (camera == 0)
    {
        qWarning() << "Renderer beginFrame failed: camera is null.";
        return false;
    }

    if (viewportWidth <= 0 || viewportHeight <= 0)
    {
        qWarning() << "Renderer beginFrame failed: viewport size is invalid.";
        return false;
    }

    if (m_frameActive)
    {
        qWarning() << "Renderer beginFrame failed: a frame is already active.";
        return false;
    }

    QOpenGLFunctions_3_3_Core* gl = m_context->gl();

    if (gl == 0)
        return false;

    const float aspect = static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);

    m_viewMatrix = camera->viewMatrix();
    m_projectionMatrix = camera->projectionMatrix(aspect);
    m_cameraPosition = camera->position();
    m_viewportWidth = viewportWidth;
    m_viewportHeight = viewportHeight;

    gl->glViewport(0, 0, viewportWidth, viewportHeight);
    gl->glEnable(GL_DEPTH_TEST);
    gl->glClearColor(m_clearColor.x(), m_clearColor.y(), m_clearColor.z(), m_clearColor.w());
    gl->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_frameActive = true;
    return true;
}

bool Renderer::drawVertexColorGeometry(const Geometry* geometry, const QMatrix4x4& model, bool depthTest)
{
    if (!m_frameActive)
    {
        qWarning() << "Renderer drawVertexColorGeometry failed: beginFrame() has not been called.";
        return false;
    }

    if (geometry == 0)
    {
        qWarning() << "Renderer drawVertexColorGeometry failed: geometry is null.";
        return false;
    }

    if (!geometry->isInitialized() || geometry->vao() == 0)
    {
        qWarning() << "Renderer drawVertexColorGeometry failed: geometry GPU resource is not initialized:" << geometry->name();
        return false;
    }

    if (geometry->indexCount() <= 0)
    {
        qWarning() << "Renderer drawVertexColorGeometry failed: geometry contains no indices:" << geometry->name();
        return false;
    }

    if (!geometry->hasAttribute(0, 3) || !geometry->hasAttribute(1, 3))
    {
        qWarning() << "Renderer drawVertexColorGeometry failed: position + color layout is required:" << geometry->name();
        return false;
    }

    QOpenGLFunctions_3_3_Core* gl = m_context->gl();

    if (gl == 0)
        return false;

    // prepareDrawGL() 之后不再存在普通参数验证的 Early Return，
    // 因此成功开始的 External GPU Read Transaction 一定会和 finishDrawGL() 成对。
    if (!geometry->prepareDrawGL(gl))
        return false;

    if (depthTest)
        gl->glEnable(GL_DEPTH_TEST);
    else
        gl->glDisable(GL_DEPTH_TEST);

    m_vertexColorProgram.bind(gl);
    gl->glUniformMatrix4fv(m_colorModelLocation, 1, GL_FALSE, model.constData());
    gl->glUniformMatrix4fv(m_colorViewLocation, 1, GL_FALSE, m_viewMatrix.constData());
    gl->glUniformMatrix4fv(m_colorProjectionLocation, 1, GL_FALSE, m_projectionMatrix.constData());

    gl->glBindVertexArray(geometry->vao());
    gl->glDrawElements(primitiveMode(geometry), geometry->indexCount(), geometry->indexType(), 0);
    gl->glBindVertexArray(0);

    // External GPU Geometry 可以在 Draw 提交后发布 Renderer 已完成读取的 GPU Fence。
    geometry->finishDrawGL(gl);

    if (!depthTest)
        gl->glEnable(GL_DEPTH_TEST);

    return true;
}

bool Renderer::drawLitGeometry(const Geometry* geometry, const Material* material, const ResourceManager* resourceManager, const LightManager* lightManager, const QMatrix4x4& model, bool depthTest)
{
    if (!m_frameActive)
    {
        qWarning() << "Renderer drawLitGeometry failed: beginFrame() has not been called.";
        return false;
    }

    if (geometry == 0 || material == 0 || resourceManager == 0 || lightManager == 0)
    {
        qWarning() << "Renderer drawLitGeometry failed: invalid argument.";
        return false;
    }

    if (!geometry->isInitialized() || geometry->vao() == 0)
    {
        qWarning() << "Renderer drawLitGeometry failed: geometry GPU resource is not initialized:" << geometry->name();
        return false;
    }

    if (geometry->indexCount() <= 0)
    {
        qWarning() << "Renderer drawLitGeometry failed: geometry contains no indices:" << geometry->name();
        return false;
    }

    const bool useVertexColor = material->type() == MaterialTypeLitVertexColor;

    if (material->type() != MaterialTypeLit && !useVertexColor)
    {
        qWarning() << "Renderer drawLitGeometry failed: Lit or LitVertexColor material is required:" << material->name();
        return false;
    }

    if (!geometry->hasAttribute(0, 3) || !geometry->hasAttribute(1, 3))
    {
        qWarning() << "Renderer drawLitGeometry failed: position + normal layout is required:" << geometry->name();
        return false;
    }

    if (useVertexColor && !geometry->hasAttribute(3, 4))
    {
        qWarning() << "Renderer drawLitGeometry failed: LitVertexColor requires color4 at attribute location 3:" << geometry->name();
        return false;
    }

    QOpenGLFunctions_3_3_Core* gl = m_context->gl();

    if (gl == 0)
        return false;

    const QMatrix3x3 normalMatrix = model.normalMatrix();
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

    // Degree -> Radian 固定比例；Spot Angle 在 Light 中使用 Degree，Shader 只接收 Cosine。
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

    // 当前 Renderer 不启用 Texture 路径；ResourceManager 参数保留以维持既有绘制接口稳定。
    (void)resourceManager;

    // 从这里开始不再存在普通验证 Early Return。
    // External GPU Geometry 可以在这里等待另一个共享 Context 完成对 VBO / EBO 的写入。
    if (!geometry->prepareDrawGL(gl))
        return false;

    m_litProgram.bind(gl);

    gl->glUniformMatrix4fv(m_litModelLocation, 1, GL_FALSE, model.constData());
    gl->glUniformMatrix4fv(m_litViewLocation, 1, GL_FALSE, m_viewMatrix.constData());
    gl->glUniformMatrix4fv(m_litProjectionLocation, 1, GL_FALSE, m_projectionMatrix.constData());
    gl->glUniformMatrix3fv(m_litNormalLocation, 1, GL_FALSE, normalMatrix.constData());

    gl->glUniform3f(m_litCameraPositionLocation, m_cameraPosition.x(), m_cameraPosition.y(), m_cameraPosition.z());
    gl->glUniform4f(m_litBaseColorLocation, baseColor.x(), baseColor.y(), baseColor.z(), baseColor.w());
    gl->glUniform3f(m_litSpecularColorLocation, specularColor.x(), specularColor.y(), specularColor.z());
    gl->glUniform1f(m_litShininessLocation, material->shininess());

    gl->glUniform3f(m_litAmbientColorLocation, ambientColor.x(), ambientColor.y(), ambientColor.z());
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

    gl->glUniform1i(m_litUseVertexColorLocation, useVertexColor ? 1 : 0);

    if (depthTest)
        gl->glEnable(GL_DEPTH_TEST);
    else
        gl->glDisable(GL_DEPTH_TEST);

    gl->glBindVertexArray(geometry->vao());
    gl->glDrawElements(primitiveMode(geometry), geometry->indexCount(), geometry->indexType(), 0);
    gl->glBindVertexArray(0);

    // Draw 已进入当前 Renderer Command Stream，可以结束 External GPU Read Transaction。
    geometry->finishDrawGL(gl);

    if (!depthTest)
        gl->glEnable(GL_DEPTH_TEST);

    return true;
}

bool Renderer::drawItem(const RenderItem* item, const ResourceManager* resourceManager, const LightManager* lightManager)
{
    if (!m_frameActive)
    {
        qWarning() << "Renderer drawItem failed: beginFrame() has not been called.";
        return false;
    }

    if (item == 0)
    {
        qWarning() << "Renderer drawItem failed: item is null.";
        return false;
    }

    if (!item->isVisible())
        return true;

    // 空 Item 是合法的用户对象状态，例如动态仿真中所有 RenderPart 暂时被移除。
    if (item->partCount() == 0)
        return true;

    const Material* material = item->material();

    if (material == 0)
    {
        // 只有实际存在可绘制 Geometry 时才要求 Material，允许 Adapter 分阶段建立空 Part。
        bool hasDrawablePart = false;

        for (int partIndex = 0; partIndex < item->partCount(); ++partIndex)
        {
            const RenderPart* currentPart = item->partAt(partIndex);

            if (currentPart != 0 && currentPart->geometry() != 0)
            {
                hasDrawablePart = true;
                break;
            }
        }

        if (!hasDrawablePart)
            return true;

        qWarning() << "Renderer drawItem failed: drawable Item requires Material:" << item->name();
        return false;
    }

    const QMatrix4x4 model = item->transform().matrix();

    for (int partIndex = 0; partIndex < item->partCount(); ++partIndex)
    {
        const RenderPart* currentPart = item->partAt(partIndex);

        if (currentPart == 0 || currentPart->geometry() == 0)
            continue;

        const Geometry* geometry = currentPart->geometry();
        bool drawSucceeded = false;

        // Polygon Display Mode 只对 Triangle Geometry 有额外意义。
        // Lines / LineStrip 本身已经是线图元，因此继续按当前 Item Material 正常绘制。
        if (geometry->renderType() != Triangles)
        {
            drawSucceeded = drawMaterialGeometry(geometry, material, resourceManager, lightManager, model, item->depthTestEnabled());
        }
        else
        {
            switch (item->displayMode())
            {
            case RenderItemDisplayShaded:
                drawSucceeded = drawMaterialGeometry(geometry, material, resourceManager, lightManager, model, item->depthTestEnabled());
                break;

            case RenderItemDisplayWireframe:
                drawSucceeded = drawWireGeometry(geometry, model, item->edgeColor(), item->depthTestEnabled(), false);
                break;

            case RenderItemDisplayShadedWithEdges:
                drawSucceeded = drawMaterialGeometry(geometry, material, resourceManager, lightManager, model, item->depthTestEnabled());

                if (drawSucceeded)
                    drawSucceeded = drawWireGeometry(geometry, model, item->edgeColor(), item->depthTestEnabled(), true);

                break;

            default:
                qWarning() << "Renderer drawItem failed: unsupported DisplayMode:" << item->name();
                return false;
            }
        }

        if (!drawSucceeded)
        {
            qWarning() << "Renderer drawItem failed while drawing RenderPart:"
                       << "Item=" << item->name()
                       << "PartId=" << static_cast<qulonglong>(currentPart->id())
                       << "Geometry=" << geometry->name();
            return false;
        }
    }

    return true;
}

bool Renderer::drawScene(const Scene* scene, const ResourceManager* resourceManager, const LightManager* lightManager)
{
    if (!m_frameActive)
    {
        qWarning() << "Renderer drawScene failed: beginFrame() has not been called.";
        return false;
    }

    if (scene == 0 || resourceManager == 0 || lightManager == 0)
    {
        qWarning() << "Renderer drawScene failed: invalid argument.";
        return false;
    }

    for (int i = 0; i < scene->itemCount(); ++i)
    {
        const RenderItem* item = scene->item(i);

        if (item != 0 && !drawItem(item, resourceManager, lightManager))
        {
            qWarning() << "Renderer drawScene failed while drawing item:" << item->name();
            return false;
        }
    }

    return true;
}

bool Renderer::drawViewNavigation(const Geometry* geometry, const Camera* camera)
{
    if (!m_frameActive)
    {
        qWarning() << "Renderer drawViewNavigation failed: beginFrame() has not been called.";
        return false;
    }

    if (geometry == 0 || camera == 0)
    {
        qWarning() << "Renderer drawViewNavigation failed: invalid argument.";
        return false;
    }

    if (!geometry->isInitialized() || geometry->vao() == 0)
    {
        qWarning() << "Renderer drawViewNavigation failed: geometry GPU resource is not initialized:" << geometry->name();
        return false;
    }

    if (!geometry->hasAttribute(0, 3) || !geometry->hasAttribute(1, 3))
    {
        qWarning() << "Renderer drawViewNavigation failed: position + color layout is required:" << geometry->name();
        return false;
    }

    const int gizmoSize = 128;  // View Navigation 固定使用 128 × 128 Pixel。
    const int gizmoMargin = 16; // 与窗口右上边缘保留 16 Pixel 间距。

    // 当前 Viewport 太小时根本不会发生 Draw，因此没有必要开始 GPU Read Transaction。
    if (m_viewportWidth < gizmoSize + gizmoMargin * 2 || m_viewportHeight < gizmoSize + gizmoMargin * 2)
        return true;

    QOpenGLFunctions_3_3_Core* gl = m_context->gl();

    if (gl == 0)
        return false;

    if (!geometry->prepareDrawGL(gl))
        return false;

    const float gizmoCameraDistance = 3.0f; // Gizmo Camera 只使用主 Camera 朝向，固定距离避免受 Zoom 影响。
    const QVector3D gizmoEye = -camera->forward() * gizmoCameraDistance;

    QMatrix4x4 gizmoView;
    gizmoView.lookAt(gizmoEye, QVector3D(0.0f, 0.0f, 0.0f), camera->viewUp());

    const float gizmoHalfExtent = 1.35f; // 单位方向轴使用固定正交范围，保证屏幕尺寸稳定。
    QMatrix4x4 gizmoProjection;
    gizmoProjection.ortho(-gizmoHalfExtent, gizmoHalfExtent, -gizmoHalfExtent, gizmoHalfExtent, 0.1f, 10.0f);

    QMatrix4x4 gizmoModel;

    const int gizmoX = m_viewportWidth - gizmoSize - gizmoMargin;
    const int gizmoY = m_viewportHeight - gizmoSize - gizmoMargin;

    gl->glDisable(GL_DEPTH_TEST);
    gl->glViewport(gizmoX, gizmoY, gizmoSize, gizmoSize);

    m_vertexColorProgram.bind(gl);
    gl->glUniformMatrix4fv(m_colorModelLocation, 1, GL_FALSE, gizmoModel.constData());
    gl->glUniformMatrix4fv(m_colorViewLocation, 1, GL_FALSE, gizmoView.constData());
    gl->glUniformMatrix4fv(m_colorProjectionLocation, 1, GL_FALSE, gizmoProjection.constData());

    gl->glBindVertexArray(geometry->vao());
    gl->glDrawElements(primitiveMode(geometry), geometry->indexCount(), geometry->indexType(), 0);
    gl->glBindVertexArray(0);

    geometry->finishDrawGL(gl);

    gl->glViewport(0, 0, m_viewportWidth, m_viewportHeight);
    gl->glEnable(GL_DEPTH_TEST);

    return true;
}

void Renderer::endFrame()
{
    if (!m_frameActive)
        return;

    QOpenGLFunctions_3_3_Core* gl = m_context->gl();

    if (gl == 0)
        return;

    gl->glBindVertexArray(0);
    ShaderProgram::unbind(gl);

    m_frameActive = false;
}

/// 内部辅助

bool Renderer::drawMaterialGeometry(const Geometry* geometry, const Material* material, const ResourceManager* resourceManager, const LightManager* lightManager, const QMatrix4x4& model, bool depthTest)
{
    switch (material->type())
    {
    case MaterialTypeVertexColor:
        return drawVertexColorGeometry(geometry, model, depthTest);

    case MaterialTypeLit:
    case MaterialTypeLitVertexColor:
        return drawLitGeometry(geometry, material, resourceManager, lightManager, model, depthTest);
    }

    qWarning() << "Renderer drawMaterialGeometry failed: unsupported Material type:" << material->name();
    return false;
}

bool Renderer::drawWireGeometry(const Geometry* geometry, const QMatrix4x4& model, const QVector4D& color, bool depthTest, bool overlay)
{
    if (!m_frameActive)
    {
        qWarning() << "Renderer drawWireGeometry failed: beginFrame() has not been called.";
        return false;
    }

    if (geometry == 0)
    {
        qWarning() << "Renderer drawWireGeometry failed: geometry is null.";
        return false;
    }

    if (geometry->renderType() != Triangles)
    {
        qWarning() << "Renderer drawWireGeometry failed: Triangle Geometry is required:" << geometry->name();
        return false;
    }

    if (!geometry->isInitialized() || geometry->vao() == 0)
    {
        qWarning() << "Renderer drawWireGeometry failed: geometry GPU resource is not initialized:" << geometry->name();
        return false;
    }

    if (geometry->indexCount() <= 0)
    {
        qWarning() << "Renderer drawWireGeometry failed: geometry contains no indices:" << geometry->name();
        return false;
    }

    if (!geometry->hasAttribute(0, 3))
    {
        qWarning() << "Renderer drawWireGeometry failed: position layout is required:" << geometry->name();
        return false;
    }

    QOpenGLFunctions_3_3_Core* gl = m_context->gl();

    if (gl == 0)
        return false;

    // 所有验证都发生在 prepareDrawGL() 之前，保证 External GPU Read Transaction 必然成对结束。
    if (!geometry->prepareDrawGL(gl))
        return false;

    if (depthTest)
        gl->glEnable(GL_DEPTH_TEST);
    else
        gl->glDisable(GL_DEPTH_TEST);

    // Overlay 线框需要通过已经写入的 Fill Depth。
    // GL_LEQUAL 允许同一 Triangle Edge 在相同深度覆盖表面；轻微负 Polygon Offset 用于减少实现差异导致的 Z-Fighting。
    if (overlay && depthTest)
    {
        gl->glDepthFunc(GL_LEQUAL);
        gl->glEnable(GL_POLYGON_OFFSET_LINE);
        gl->glPolygonOffset(-1.0f, -1.0f);
    }

    gl->glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    m_solidColorProgram.bind(gl);
    gl->glUniformMatrix4fv(m_solidModelLocation, 1, GL_FALSE, model.constData());
    gl->glUniformMatrix4fv(m_solidViewLocation, 1, GL_FALSE, m_viewMatrix.constData());
    gl->glUniformMatrix4fv(m_solidProjectionLocation, 1, GL_FALSE, m_projectionMatrix.constData());
    gl->glUniform4f(m_solidColorLocation, color.x(), color.y(), color.z(), color.w());

    gl->glBindVertexArray(geometry->vao());
    gl->glDrawElements(GL_TRIANGLES, geometry->indexCount(), geometry->indexType(), 0);
    gl->glBindVertexArray(0);

    // Draw 已进入当前 Renderer Command Stream，可以结束 External GPU Read Transaction。
    geometry->finishDrawGL(gl);

    gl->glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    if (overlay && depthTest)
    {
        gl->glDisable(GL_POLYGON_OFFSET_LINE);
        gl->glDepthFunc(GL_LESS);
    }

    if (!depthTest)
        gl->glEnable(GL_DEPTH_TEST);

    return true;
}

GLenum Renderer::primitiveMode(const Geometry* geometry) const
{
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
