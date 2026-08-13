#include "Renderer.h"

#include "Camera/Camera.h"
#include "Core/ResourceManager.h"
#include "Light/Light.h"
#include "Light/LightManager.h"
#include "Material/Material.h"
#include "Render/RenderContext.h"
#include "Resource/RenderableMesh.h"
#include "Resource/TextureResource.h"

#include <QDebug>
#include <QMatrix3x3>

Renderer::Renderer()
    : m_context(0)
    , m_colorModelLocation(-1)
    , m_colorViewLocation(-1)
    , m_colorProjectionLocation(-1)
    , m_litModelLocation(-1)
    , m_litViewLocation(-1)
    , m_litProjectionLocation(-1)
    , m_litNormalLocation(-1)
    , m_litCameraPositionLocation(-1)
    , m_litBaseColorLocation(-1)
    , m_litSpecularColorLocation(-1)
    , m_litShininessLocation(-1)
    , m_litUseTextureLocation(-1)
    , m_litTextureLocation(-1)
    , m_litAmbientColorLocation(-1)
    , m_litAmbientIntensityLocation(-1)
    , m_litLightDirectionLocation(-1)
    , m_litLightColorLocation(-1)
    , m_litLightIntensityLocation(-1)
    , m_cameraPosition(0.0f, 0.0f, 0.0f)
    , m_clearColor(0.1f, 0.1f, 0.1f, 1.0f)
    , m_viewportWidth(0)
    , m_viewportHeight(0)
    , m_initialized(false)
    , m_frameActive(false)
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

    const char* litVertexShader =
        "#version 330 core\n"
        "layout(location = 0) in vec3 aPosition;\n"
        "layout(location = 1) in vec3 aNormal;\n"
        "layout(location = 2) in vec2 aTexCoord;\n"
        "uniform mat4 model;\n"
        "uniform mat4 view;\n"
        "uniform mat4 projection;\n"
        "uniform mat3 normalMatrix;\n"
        "out vec3 fragmentPosition;\n"
        "out vec3 fragmentNormal;\n"
        "out vec2 texCoord;\n"
        "void main()\n"
        "{\n"
        "    vec4 worldPosition = model * vec4(aPosition, 1.0);\n"
        "    fragmentPosition = worldPosition.xyz;\n"
        "    fragmentNormal = normalize(normalMatrix * aNormal);\n"
        "    texCoord = aTexCoord;\n"
        "    gl_Position = projection * view * worldPosition;\n"
        "}\n";

    const char* litFragmentShader =
        "#version 330 core\n"
        "in vec3 fragmentPosition;\n"
        "in vec3 fragmentNormal;\n"
        "in vec2 texCoord;\n"
        "uniform vec3 cameraPosition;\n"
        "uniform vec4 baseColor;\n"
        "uniform vec3 specularColor;\n"
        "uniform float shininess;\n"
        "uniform bool useDiffuseTexture;\n"
        "uniform sampler2D diffuseTexture;\n"
        "uniform vec3 ambientColor;\n"
        "uniform float ambientIntensity;\n"
        "uniform vec3 directionalLightDirection;\n"
        "uniform vec3 directionalLightColor;\n"
        "uniform float directionalLightIntensity;\n"
        "out vec4 FragColor;\n"
        "void main()\n"
        "{\n"
        "    vec4 surfaceColor = baseColor;\n"
        "    if (useDiffuseTexture)\n"
        "        surfaceColor *= texture(diffuseTexture, texCoord);\n"
        "    vec3 normal = normalize(fragmentNormal);\n"
        "    vec3 lightDirection = normalize(-directionalLightDirection);\n"
        "    vec3 viewDirection = normalize(cameraPosition - fragmentPosition);\n"
        "    vec3 halfDirection = normalize(lightDirection + viewDirection);\n"
        "    float diffuseFactor = max(dot(normal, lightDirection), 0.0);\n"
        "    float specularFactor = 0.0;\n"
        "    if (diffuseFactor > 0.0)\n"
        "        specularFactor = pow(max(dot(normal, halfDirection), 0.0), shininess);\n"
        "    vec3 ambient = surfaceColor.rgb * ambientColor * ambientIntensity;\n"
        "    vec3 diffuse = surfaceColor.rgb * directionalLightColor * directionalLightIntensity * diffuseFactor;\n"
        "    vec3 specular = specularColor * directionalLightColor * directionalLightIntensity * specularFactor;\n"
        "    FragColor = vec4(ambient + diffuse + specular, surfaceColor.a);\n"
        "}\n";

    if (!m_vertexColorProgram.initialize(gl, vertexColorVertexShader, vertexColorFragmentShader))
        return false;

    if (!m_litProgram.initialize(gl, litVertexShader, litFragmentShader))
    {
        m_vertexColorProgram.release(gl);
        return false;
    }

    m_colorModelLocation = m_vertexColorProgram.uniformLocation(gl, "model");
    m_colorViewLocation = m_vertexColorProgram.uniformLocation(gl, "view");
    m_colorProjectionLocation = m_vertexColorProgram.uniformLocation(gl, "projection");

    m_litModelLocation = m_litProgram.uniformLocation(gl, "model");
    m_litViewLocation = m_litProgram.uniformLocation(gl, "view");
    m_litProjectionLocation = m_litProgram.uniformLocation(gl, "projection");
    m_litNormalLocation = m_litProgram.uniformLocation(gl, "normalMatrix");
    m_litCameraPositionLocation = m_litProgram.uniformLocation(gl, "cameraPosition");
    m_litBaseColorLocation = m_litProgram.uniformLocation(gl, "baseColor");
    m_litSpecularColorLocation = m_litProgram.uniformLocation(gl, "specularColor");
    m_litShininessLocation = m_litProgram.uniformLocation(gl, "shininess");
    m_litUseTextureLocation = m_litProgram.uniformLocation(gl, "useDiffuseTexture");
    m_litTextureLocation = m_litProgram.uniformLocation(gl, "diffuseTexture");
    m_litAmbientColorLocation = m_litProgram.uniformLocation(gl, "ambientColor");
    m_litAmbientIntensityLocation = m_litProgram.uniformLocation(gl, "ambientIntensity");
    m_litLightDirectionLocation = m_litProgram.uniformLocation(gl, "directionalLightDirection");
    m_litLightColorLocation = m_litProgram.uniformLocation(gl, "directionalLightColor");
    m_litLightIntensityLocation = m_litProgram.uniformLocation(gl, "directionalLightIntensity");

    if (m_colorModelLocation < 0 || m_colorViewLocation < 0 || m_colorProjectionLocation < 0 ||
        m_litModelLocation < 0 || m_litViewLocation < 0 || m_litProjectionLocation < 0 ||
        m_litNormalLocation < 0 || m_litCameraPositionLocation < 0 || m_litBaseColorLocation < 0 ||
        m_litSpecularColorLocation < 0 || m_litShininessLocation < 0 || m_litUseTextureLocation < 0 ||
        m_litTextureLocation < 0 || m_litAmbientColorLocation < 0 || m_litAmbientIntensityLocation < 0 ||
        m_litLightDirectionLocation < 0 || m_litLightColorLocation < 0 || m_litLightIntensityLocation < 0)
    {
        qWarning() << "Renderer initialize failed: required Shader Uniform was not found.";
        m_vertexColorProgram.release(gl);
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
    m_litProgram.release(gl);

    m_context = 0;
    m_viewportWidth = 0;
    m_viewportHeight = 0;
    m_initialized = false;
    m_frameActive = false;
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

bool Renderer::drawVertexColorMesh(const RenderableMesh* mesh, const QMatrix4x4& model, bool depthTest)
{
    if (!m_frameActive)
    {
        qWarning() << "Renderer drawVertexColorMesh failed: beginFrame() has not been called.";
        return false;
    }

    if (mesh == 0)
    {
        qWarning() << "Renderer drawVertexColorMesh failed: mesh is null.";
        return false;
    }

    if (!mesh->renderMeshInitialized() || mesh->vao() == 0)
    {
        qWarning() << "Renderer drawVertexColorMesh failed: mesh GPU resource is not initialized:" << mesh->renderMeshName();
        return false;
    }

    if (mesh->indexCount() <= 0)
    {
        qWarning() << "Renderer drawVertexColorMesh failed: mesh contains no indices:" << mesh->renderMeshName();
        return false;
    }

    if (!mesh->hasAttribute(0, 3) || !mesh->hasAttribute(1, 3))
    {
        qWarning() << "Renderer drawVertexColorMesh failed: position + color layout is required:" << mesh->renderMeshName();
        return false;
    }

    QOpenGLFunctions_3_3_Core* gl = m_context->gl();

    if (gl == 0)
        return false;

    if (depthTest)
        gl->glEnable(GL_DEPTH_TEST);
    else
        gl->glDisable(GL_DEPTH_TEST);

    m_vertexColorProgram.bind(gl);
    gl->glUniformMatrix4fv(m_colorModelLocation, 1, GL_FALSE, model.constData());
    gl->glUniformMatrix4fv(m_colorViewLocation, 1, GL_FALSE, m_viewMatrix.constData());
    gl->glUniformMatrix4fv(m_colorProjectionLocation, 1, GL_FALSE, m_projectionMatrix.constData());

    gl->glBindVertexArray(mesh->vao());
    gl->glDrawElements(primitiveMode(mesh), mesh->indexCount(), mesh->indexType(), 0);
    gl->glBindVertexArray(0);

    if (!depthTest)
        gl->glEnable(GL_DEPTH_TEST);

    return true;
}

bool Renderer::drawLitMesh(const RenderableMesh* mesh, const Material* material, const ResourceManager* resourceManager, const LightManager* lightManager, const QMatrix4x4& model)
{
    if (!m_frameActive)
    {
        qWarning() << "Renderer drawLitMesh failed: beginFrame() has not been called.";
        return false;
    }

    if (mesh == 0 || material == 0 || resourceManager == 0 || lightManager == 0)
    {
        qWarning() << "Renderer drawLitMesh failed: invalid argument.";
        return false;
    }

    if (!mesh->renderMeshInitialized() || mesh->vao() == 0)
    {
        qWarning() << "Renderer drawLitMesh failed: mesh GPU resource is not initialized:" << mesh->renderMeshName();
        return false;
    }

    if (mesh->indexCount() <= 0)
    {
        qWarning() << "Renderer drawLitMesh failed: mesh contains no indices:" << mesh->renderMeshName();
        return false;
    }

    if (material->type() != MaterialTypeLit)
    {
        qWarning() << "Renderer drawLitMesh failed: material is not Lit:" << material->name();
        return false;
    }

    if (!mesh->hasAttribute(0, 3) || !mesh->hasAttribute(1, 3) || !mesh->hasAttribute(2, 2))
    {
        qWarning() << "Renderer drawLitMesh failed: position + normal + uv layout is required:" << mesh->renderMeshName();
        return false;
    }

    QOpenGLFunctions_3_3_Core* gl = m_context->gl();

    if (gl == 0)
        return false;

    const QMatrix3x3 normalMatrix = model.normalMatrix();
    const QVector4D& baseColor = material->baseColor();
    const QVector3D& specularColor = material->specularColor();
    const QVector3D& ambientColor = lightManager->ambientColor();

    QVector3D lightDirection(0.0f, -1.0f, 0.0f);
    QVector3D lightColor(1.0f, 1.0f, 1.0f);
    float lightIntensity = 0.0f;

    const Light* directionalLight = lightManager->firstEnabledDirectionalLight();

    if (directionalLight != 0)
    {
        lightDirection = directionalLight->direction();
        lightColor = directionalLight->color();
        lightIntensity = directionalLight->intensity();
    }

    bool useTexture = false;
    const TextureResource* texture = 0;

    if (material->hasDiffuseTexture())
    {
        const Resource* resource = resourceManager->get(material->diffuseTextureId());

        if (resource == 0)
        {
            qWarning() << "Renderer drawLitMesh failed: diffuse texture resource does not exist:" << material->diffuseTextureId();
            return false;
        }

        if (resource->type() != ResourceTypeTexture)
        {
            qWarning() << "Renderer drawLitMesh failed: diffuse texture ResourceId does not reference TextureResource:" << material->diffuseTextureId();
            return false;
        }

        texture = static_cast<const TextureResource*>(resource);

        if (!texture->isInitialized() || texture->textureId() == 0)
        {
            qWarning() << "Renderer drawLitMesh failed: diffuse texture GPU resource is not initialized:" << texture->name();
            return false;
        }

        useTexture = true;
    }

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
    gl->glUniform3f(m_litLightDirectionLocation, lightDirection.x(), lightDirection.y(), lightDirection.z());
    gl->glUniform3f(m_litLightColorLocation, lightColor.x(), lightColor.y(), lightColor.z());
    gl->glUniform1f(m_litLightIntensityLocation, lightIntensity);

    gl->glUniform1i(m_litUseTextureLocation, useTexture ? 1 : 0);
    gl->glUniform1i(m_litTextureLocation, 0);

    if (useTexture)
    {
        gl->glActiveTexture(GL_TEXTURE0);
        gl->glBindTexture(GL_TEXTURE_2D, texture->textureId());
    }

    gl->glEnable(GL_DEPTH_TEST);
    gl->glBindVertexArray(mesh->vao());
    gl->glDrawElements(primitiveMode(mesh), mesh->indexCount(), mesh->indexType(), 0);
    gl->glBindVertexArray(0);

    if (useTexture)
        gl->glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

bool Renderer::drawViewNavigation(const RenderableMesh* mesh, const Camera* camera)
{
    if (!m_frameActive)
    {
        qWarning() << "Renderer drawViewNavigation failed: beginFrame() has not been called.";
        return false;
    }

    if (mesh == 0 || camera == 0)
    {
        qWarning() << "Renderer drawViewNavigation failed: invalid argument.";
        return false;
    }

    if (!mesh->renderMeshInitialized() || mesh->vao() == 0)
    {
        qWarning() << "Renderer drawViewNavigation failed: mesh GPU resource is not initialized:" << mesh->renderMeshName();
        return false;
    }

    if (!mesh->hasAttribute(0, 3) || !mesh->hasAttribute(1, 3))
    {
        qWarning() << "Renderer drawViewNavigation failed: position + color layout is required:" << mesh->renderMeshName();
        return false;
    }

    QOpenGLFunctions_3_3_Core* gl = m_context->gl();

    if (gl == 0)
        return false;

    const int gizmoSize = 128;       // View Navigation 固定使用 128 × 128 Pixel。
    const int gizmoMargin = 16;      // 与窗口右上边缘保留 16 Pixel 间距。

    if (m_viewportWidth < gizmoSize + gizmoMargin * 2 || m_viewportHeight < gizmoSize + gizmoMargin * 2)
        return true;

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

    gl->glBindVertexArray(mesh->vao());
    gl->glDrawElements(primitiveMode(mesh), mesh->indexCount(), mesh->indexType(), 0);
    gl->glBindVertexArray(0);

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
    gl->glBindTexture(GL_TEXTURE_2D, 0);
    ShaderProgram::unbind(gl);

    m_frameActive = false;
}

/// 内部辅助

GLenum Renderer::primitiveMode(const RenderableMesh* mesh) const
{
    switch (mesh->primitiveType())
    {
    case MeshPrimitiveTriangles:
        return GL_TRIANGLES;
    case MeshPrimitiveLines:
        return GL_LINES;
    case MeshPrimitiveLineStrip:
        return GL_LINE_STRIP;
    }

    return GL_TRIANGLES;
}