#include <QApplication>
#include <QSurfaceFormat>

#include "OpenGLWidget.h"

int main(int argc, char* argv[])
{
    // 请求 OpenGL 3.3 Core Profile
    QSurfaceFormat format;

    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);

    // 请求 24-bit Depth Buffer
    format.setDepthBufferSize(24);

    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);

    OpenGLWidget window;

    window.resize(800, 600);
    window.setWindowTitle("MyOpenGL - Cube");
    window.show();

    return app.exec();
}