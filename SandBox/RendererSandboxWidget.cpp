#include "RendererSandboxWidget.h"

#include <QDebug>
#include <QImage>
#include <QPainter>

#include "MyOpenGL/Material/Material.h"
#include "MyOpenGL/Resource/BufferGeometry.h"
#include "MyOpenGL/Resource/Texture.h"

RendererSandboxWidget::RendererSandboxWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , m_textureGeometry(0)
    , m_texture(0)
    , m_textureMaterial(0)
    , m_initialized(false)
{
    setMinimumSize(600, 400);
}

RendererSandboxWidget::~RendererSandboxWidget()
{
    releaseSandbox();
}

void RendererSandboxWidget::initializeGL()
{
    if (!m_openGLContext.initialize())
    {
        qWarning() << "RendererSandboxWidget initializeGL failed: MyOpenGLContext initialization failed.";
        return;
    }

    if (!m_renderer.initialize(&m_openGLContext))
    {
        qWarning() << "RendererSandboxWidget initializeGL failed: Renderer initialization failed.";
        return;
    }

    m_renderer.setClearColor(QVector4D(0.12f, 0.12f, 0.12f, 1.0f));

    if (!createTextureTest())
    {
        qWarning() << "RendererSandboxWidget initializeGL failed: texture test creation failed.";
        return;
    }

    m_initialized = true;
}

void RendererSandboxWidget::paintGL()
{
    if (!m_initialized)
        return;

    QOpenGLFunctions_3_3_Core* gl = m_openGLContext.gl();

    if (gl == 0)
        return;

    /// 每帧同步 Resource，后面测试 Dynamic Geometry / Texture Partial Update 时也可以直接复用。
    if (!m_resourceManager.syncAll(gl))
    {
        qWarning() << "RendererSandboxWidget paintGL failed: resource synchronization failed.";
        return;
    }

    RenderContext context;

    context.view.setToIdentity();
    context.projection.setToIdentity();

    context.cameraPosition = QVector3D(0.0f, 0.0f, 1.0f);
    context.cameraForward = QVector3D(0.0f, 0.0f, -1.0f);
    context.cameraUp = QVector3D(0.0f, 1.0f, 0.0f);

    context.viewportWidth = width();
    context.viewportHeight = height();

    if (!m_renderer.beginFrame(context))
        return;

    RenderState state;

    state.model.setToIdentity();
    state.view.setToIdentity();
    state.projection.setToIdentity();

    state.viewport = RenderViewport(0, 0, width(), height());

    /// 当前测试 Quad 直接位于 Clip Space，不需要 Depth。
    state.depthTestEnabled = false;
    state.depthWriteEnabled = false;

    const std::vector<const Light*> noLights;

    if (!m_renderer.drawGeometry(m_textureGeometry, m_textureMaterial, state, noLights))
        qWarning() << "RendererSandboxWidget paintGL failed: texture Geometry drawing failed.";

    m_renderer.endFrame();
}

bool RendererSandboxWidget::createTextureTest()
{
    /// ---------------- Geometry ----------------

    BufferGeometry* geometry = new BufferGeometry("RendererSandboxTextureQuad", BufferUsage::Static, RenderType::Triangles);

    std::vector<GeometryVertexAttribute> attributes;

    GeometryVertexAttribute positionAttribute;
    positionAttribute.location = GeometryAttribute::Position;
    positionAttribute.componentCount = 3;
    positionAttribute.valueOffset = 0;
    attributes.push_back(positionAttribute);

    GeometryVertexAttribute texCoordAttribute;
    texCoordAttribute.location = GeometryAttribute::TexCoord;
    texCoordAttribute.componentCount = 2;
    texCoordAttribute.valueOffset = 3;
    attributes.push_back(texCoordAttribute);

    geometry->setVertexLayout(5, attributes);

    /// position.xyz + uv
    ///
    /// QImage 的顶部对应 v=0，因此这里：
    ///
    /// 左下 -> (0, 1)
    /// 右下 -> (1, 1)
    /// 右上 -> (1, 0)
    /// 左上 -> (0, 0)
    const std::vector<GLfloat> vertices =
    {
        -0.75f, -0.75f, 0.0f, 0.0f, 1.0f,
         0.75f, -0.75f, 0.0f, 1.0f, 1.0f,
         0.75f,  0.75f, 0.0f, 1.0f, 0.0f,
        -0.75f,  0.75f, 0.0f, 0.0f, 0.0f
    };

    const std::vector<GLuint> indices =
    {
        0, 1, 2,
        0, 2, 3
    };

    geometry->setVertexData(vertices);
    geometry->setIndexData(indices);

    if (m_resourceManager.adopt(geometry) == InvalidResourceId)
    {
        delete geometry;
        qWarning() << "RendererSandboxWidget createTextureTest failed: Geometry registration failed.";
        return false;
    }

    m_textureGeometry = geometry;

    /// ---------------- Texture ----------------

    Texture* texture = new Texture("RendererSandboxTexture");

    if (!texture->setImage(createTestImage()))
    {
        delete texture;
        qWarning() << "RendererSandboxWidget createTextureTest failed: Texture image initialization failed.";
        return false;
    }

    if (m_resourceManager.adopt(texture) == InvalidResourceId)
    {
        delete texture;
        qWarning() << "RendererSandboxWidget createTextureTest failed: Texture registration failed.";
        return false;
    }

    m_texture = texture;

    /// ---------------- Material ----------------

    Material* material = m_materialManager.createMaterial("RendererSandboxTextureMaterial");

    if (material == 0)
    {
        qWarning() << "RendererSandboxWidget createTextureTest failed: Material creation failed.";
        return false;
    }

    material->setSurfaceMode(SurfaceMode::Texture);
    material->setLightingEnabled(false);
    material->setTexture(texture);
    material->setColor(QVector4D(1.0f, 1.0f, 1.0f, 1.0f));

    m_textureMaterial = material;

    return true;
}

QImage RendererSandboxWidget::createTestImage() const
{
    const int imageSize = 256;
    const int halfSize = imageSize / 2;

    QImage image(imageSize, imageSize, QImage::Format_RGBA8888);
    image.fill(Qt::black);

    QPainter painter(&image);
    /// 左上：红
    painter.fillRect(QRect(0, 0, halfSize, halfSize), QColor(255, 0, 0));
    /// 右上：绿
    painter.fillRect(QRect(halfSize, 0, halfSize, halfSize), QColor(0, 255, 0));
    /// 左下：蓝
    painter.fillRect(QRect(0, halfSize, halfSize, halfSize), QColor(0, 0, 255));
    /// 右下：黄
    painter.fillRect(QRect(halfSize, halfSize, halfSize, halfSize), QColor(255, 255, 0));

    painter.end();

    return image;
}

void RendererSandboxWidget::releaseSandbox()
{
    if (context() == 0)
        return;

    makeCurrent();

    QOpenGLFunctions_3_3_Core* gl = m_openGLContext.gl();

    if (gl != 0)
    {
        m_renderer.release();
        m_resourceManager.clear(gl);
    }

    m_materialManager.clear();

    m_textureGeometry = 0;
    m_texture = 0;
    m_textureMaterial = 0;

    m_initialized = false;

    doneCurrent();
}