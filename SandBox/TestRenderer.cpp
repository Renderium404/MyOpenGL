#include <QApplication>
#include <QSurfaceFormat>

#include "RendererSandboxWidget.h"

int main(int argc, char* argv[])
{
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);

    QSurfaceFormat::setDefaultFormat(format);

    QApplication application(argc, argv);

    RendererSandboxWidget widget;
    widget.setWindowTitle("MyOpenGL Renderer Sandbox");
    widget.resize(800, 600);
    widget.show();

    return application.exec();
}