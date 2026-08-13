#include <QApplication>
#include <QSurfaceFormat>

#include "OpenGLSandboxWidget.h"

int main(int argc, char* argv[])
{
    // SandBox 统一请求 OpenGL 3.3 Core，并保留 24-bit Depth Buffer。
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);

    OpenGLSandboxWidget widget;
    widget.setFormat(format);
    widget.resize(1280, 800);
    widget.show();

    return app.exec();
}