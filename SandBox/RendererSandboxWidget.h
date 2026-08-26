#ifndef RENDERERSANDBOXWIDGET_H
#define RENDERERSANDBOXWIDGET_H

#include <QOpenGLWidget>

#include "MyOpenGL/Core/ResourceManager.h"
#include "MyOpenGL/Material/MaterialManager.h"
#include "MyOpenGL/Render/MyOpenGLContext.h"
#include "MyOpenGL/Render/Renderer.h"

class BufferGeometry;
class Material;
class Texture;

/// Renderer 独立测试沙盒。
/// 不经过 Viewer / ItemManager / RenderItem，直接测试底层 Renderer Pipeline。
class RendererSandboxWidget : public QOpenGLWidget
{
public:
    explicit RendererSandboxWidget(QWidget* parent = 0);
    ~RendererSandboxWidget() override;

protected:
    void initializeGL() override;
    void paintGL() override;

private:
    bool createTextureTest();
    QImage createTestImage() const;
    void releaseSandbox();

private:
    MyOpenGLContext m_openGLContext;
    Renderer m_renderer;

    ResourceManager m_resourceManager;
    MaterialManager m_materialManager;

    BufferGeometry* m_textureGeometry;
    Texture* m_texture;
    Material* m_textureMaterial;

    bool m_initialized;
};

#endif // RENDERERSANDBOXWIDGET_H