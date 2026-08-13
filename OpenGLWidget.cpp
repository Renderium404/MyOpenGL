#include "OpenGLWidget.h"

#include <QtMath>
#include <QDebug>
#include <QImage>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <QWheelEvent>

OpenGLWidget::OpenGLWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , m_vao(0)
    , m_vbo(0)
    , m_ebo(0)
    , m_texture(0)
    , m_shaderProgram(0)
    , m_modelLocation(-1)
    , m_viewLocation(-1)
    , m_projectionLocation(-1)
    , m_leftButtonPressed(false)
    , m_rightButtonPressed(false)
    , m_cameraYaw(45.0f)
    , m_cameraPitch(25.0f)
    , m_cameraDistance(3.0f)
    , m_cameraTarget(0.0f, 0.0f, 0.0f)
    , m_cornerY(0.5f)
    , m_cornerDataDirty(false)
    , m_ready(false)
{
}

OpenGLWidget::~OpenGLWidget()
{
    makeCurrent();

    if (m_shaderProgram != 0)
        glDeleteProgram(m_shaderProgram);

    if (m_texture != 0)
        glDeleteTextures(1, &m_texture);

    if (m_ebo != 0)
        glDeleteBuffers(1, &m_ebo);

    if (m_vbo != 0)
        glDeleteBuffers(1, &m_vbo);

    if (m_vao != 0)
        glDeleteVertexArrays(1, &m_vao);

    doneCurrent();
}

void OpenGLWidget::initializeGL()
{
    if (!initializeOpenGLFunctions())
    {
        qWarning() << "Failed to initialize OpenGL 3.3 Core functions.";
        return;
    }

    const QSurfaceFormat format = context()->format();
    qDebug() << "OpenGL version:" << format.majorVersion() << "." << format.minorVersion();
    qDebug() << "Depth buffer size:" << format.depthBufferSize();

    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glEnable(GL_DEPTH_TEST);

    // 每个顶点：
    //
    // position | uv
    // x y z    | u v
    //
    // stride = 5 GLfloat
    const GLfloat initialVertices[] =
    {
        // Front +Z
        -0.5f, -0.5f,  0.5f,   0.0f, 0.0f, // 0
         0.5f, -0.5f,  0.5f,   1.0f, 0.0f, // 1
         0.5f,  0.5f,  0.5f,   1.0f, 1.0f, // 2 右上前
        -0.5f,  0.5f,  0.5f,   0.0f, 1.0f, // 3

        // Back -Z
         0.5f, -0.5f, -0.5f,   0.0f, 0.0f, // 4
        -0.5f, -0.5f, -0.5f,   1.0f, 0.0f, // 5
        -0.5f,  0.5f, -0.5f,   1.0f, 1.0f, // 6
         0.5f,  0.5f, -0.5f,   0.0f, 1.0f, // 7

        // Left -X
        -0.5f, -0.5f, -0.5f,   0.0f, 0.0f, // 8
        -0.5f, -0.5f,  0.5f,   1.0f, 0.0f, // 9
        -0.5f,  0.5f,  0.5f,   1.0f, 1.0f, // 10
        -0.5f,  0.5f, -0.5f,   0.0f, 1.0f, // 11

        // Right +X
         0.5f, -0.5f,  0.5f,   0.0f, 0.0f, // 12
         0.5f, -0.5f, -0.5f,   1.0f, 0.0f, // 13
         0.5f,  0.5f, -0.5f,   1.0f, 1.0f, // 14
         0.5f,  0.5f,  0.5f,   0.0f, 1.0f, // 15 右上前

        // Top +Y
        -0.5f,  0.5f,  0.5f,   0.0f, 0.0f, // 16
         0.5f,  0.5f,  0.5f,   1.0f, 0.0f, // 17 右上前
         0.5f,  0.5f, -0.5f,   1.0f, 1.0f, // 18
        -0.5f,  0.5f, -0.5f,   0.0f, 1.0f, // 19

        // Bottom -Y
        -0.5f, -0.5f, -0.5f,   0.0f, 0.0f, // 20
         0.5f, -0.5f, -0.5f,   1.0f, 0.0f, // 21
         0.5f, -0.5f,  0.5f,   1.0f, 1.0f, // 22
        -0.5f, -0.5f,  0.5f,   0.0f, 1.0f  // 23
    };

    const GLuint indices[] =
    {
         0,  1,  2,   0,  2,  3,
         4,  5,  6,   4,  6,  7,
         8,  9, 10,   8, 10, 11,
        12, 13, 14,  12, 14, 15,
        16, 17, 18,  16, 18, 19,
        20, 21, 22,  20, 22, 23
    };

    const int vertexValueCount = sizeof(initialVertices) / sizeof(initialVertices[0]);
    m_vertices.assign(initialVertices, initialVertices + vertexValueCount);

    m_shaderProgram = createShaderProgram();

    if (m_shaderProgram == 0)
    {
        qWarning() << "Failed to create shader program.";
        return;
    }

    m_modelLocation = glGetUniformLocation(m_shaderProgram, "model");
    m_viewLocation = glGetUniformLocation(m_shaderProgram, "view");
    m_projectionLocation = glGetUniformLocation(m_shaderProgram, "projection");

    const GLint textureLocation = glGetUniformLocation(m_shaderProgram, "texture0");

    if (m_modelLocation < 0)
    {
        qWarning() << "Failed to find model uniform.";
        return;
    }

    if (m_viewLocation < 0)
    {
        qWarning() << "Failed to find view uniform.";
        return;
    }

    if (m_projectionLocation < 0)
    {
        qWarning() << "Failed to find projection uniform.";
        return;
    }

    if (textureLocation < 0)
    {
        qWarning() << "Failed to find texture0 uniform.";
        return;
    }

    // VAO
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    // VBO
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    glBufferData(GL_ARRAY_BUFFER,
                 m_vertices.size() * sizeof(GLfloat),
                 &m_vertices[0],
                 GL_DYNAMIC_DRAW);

    // EBO
    glGenBuffers(1, &m_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), 0);
    glEnableVertexAttribArray(0);

    // uv
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat),
                          reinterpret_cast<void*>(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Texture
    QImage image(":/textures/texture.png");

    if (image.isNull())
    {
        qWarning() << "Failed to load texture:" << ":/textures/texture.png";
        return;
    }

    image = image.convertToFormat(QImage::Format_RGBA8888);
    image = image.mirrored();

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 image.width(), image.height(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 image.constBits());

    glUseProgram(m_shaderProgram);
    glUniform1i(textureLocation, 0);

    m_ready = true;
}

void OpenGLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void OpenGLWidget::paintGL()
{
    if (!m_ready)
        return;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // ------------------------------------------------
    // 局部增量更新
    //
    // 一个几何角点在 VBO 中对应 3 个 Vertex：
    //
    // Front Vertex 2
    // Right Vertex 15
    // Top   Vertex 17
    //
    // 每次只更新三个 Y，也就是 12 Byte。
    // ------------------------------------------------
    if (m_cornerDataDirty)
    {
        const int stride = 5;

        const int frontVertex = 2;
        const int rightVertex = 15;
        const int topVertex = 17;

        const int frontYIndex = frontVertex * stride + 1;
        const int rightYIndex = rightVertex * stride + 1;
        const int topYIndex = topVertex * stride + 1;

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

        glBufferSubData(GL_ARRAY_BUFFER,
                        frontYIndex * sizeof(GLfloat),
                        sizeof(GLfloat),
                        &m_vertices[frontYIndex]);

        glBufferSubData(GL_ARRAY_BUFFER,
                        rightYIndex * sizeof(GLfloat),
                        sizeof(GLfloat),
                        &m_vertices[rightYIndex]);

        glBufferSubData(GL_ARRAY_BUFFER,
                        topYIndex * sizeof(GLfloat),
                        sizeof(GLfloat),
                        &m_vertices[topYIndex]);

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        m_cornerDataDirty = false;
    }

    glUseProgram(m_shaderProgram);

    QMatrix4x4 model;

    const float yaw = qDegreesToRadians(m_cameraYaw);
    const float pitch = qDegreesToRadians(m_cameraPitch);

    QVector3D cameraPosition;
    cameraPosition.setX(m_cameraTarget.x() + m_cameraDistance * qCos(pitch) * qSin(yaw));
    cameraPosition.setY(m_cameraTarget.y() + m_cameraDistance * qSin(pitch));
    cameraPosition.setZ(m_cameraTarget.z() + m_cameraDistance * qCos(pitch) * qCos(yaw));

    QMatrix4x4 view;
    view.lookAt(cameraPosition, m_cameraTarget, QVector3D(0.0f, 1.0f, 0.0f));

    if (height() <= 0)
    {
        qWarning() << "Invalid widget height:" << height();
        return;
    }

    const float aspect = static_cast<float>(width()) / static_cast<float>(height());

    QMatrix4x4 projection;
    projection.perspective(45.0f, aspect, 0.1f, 100.0f);

    glUniformMatrix4fv(m_modelLocation, 1, GL_FALSE, model.constData());
    glUniformMatrix4fv(m_viewLocation, 1, GL_FALSE, view.constData());
    glUniformMatrix4fv(m_projectionLocation, 1, GL_FALSE, projection.constData());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texture);

    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void OpenGLWidget::updateCornerData()
{
    const int stride = 5;

    const int frontVertex = 2;
    const int rightVertex = 15;
    const int topVertex = 17;

    m_vertices[frontVertex * stride + 1] = m_cornerY;
    m_vertices[rightVertex * stride + 1] = m_cornerY;
    m_vertices[topVertex * stride + 1] = m_cornerY;

    m_cornerDataDirty = true;
}

GLuint OpenGLWidget::compileShader(GLenum type, const char* source)
{
    const GLuint shader = glCreateShader(type);

    glShaderSource(shader, 1, &source, 0);
    glCompileShader(shader);

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (success != GL_TRUE)
    {
        GLchar log[1024] = { 0 };
        glGetShaderInfoLog(shader, sizeof(log), 0, log);

        qWarning() << "Shader compilation failed:" << log;

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint OpenGLWidget::createShaderProgram()
{
    const char* vertexShaderSource =
        "#version 330 core\n"
        "\n"
        "layout(location = 0) in vec3 aPos;\n"
        "layout(location = 1) in vec2 aTexCoord;\n"
        "\n"
        "uniform mat4 model;\n"
        "uniform mat4 view;\n"
        "uniform mat4 projection;\n"
        "\n"
        "out vec2 texCoord;\n"
        "\n"
        "void main()\n"
        "{\n"
        "    gl_Position = projection * view * model * vec4(aPos, 1.0);\n"
        "    texCoord = aTexCoord;\n"
        "}\n";

    const char* fragmentShaderSource =
        "#version 330 core\n"
        "\n"
        "in vec2 texCoord;\n"
        "\n"
        "uniform sampler2D texture0;\n"
        "\n"
        "out vec4 FragColor;\n"
        "\n"
        "void main()\n"
        "{\n"
        "    FragColor = texture(texture0, texCoord);\n"
        "}\n";

    const GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);

    if (vertexShader == 0)
        return 0;

    const GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    if (fragmentShader == 0)
    {
        glDeleteShader(vertexShader);
        return 0;
    }

    const GLuint program = glCreateProgram();

    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (success != GL_TRUE)
    {
        GLchar log[1024] = { 0 };
        glGetProgramInfoLog(program, sizeof(log), 0, log);

        qWarning() << "Shader program link failed:" << log;

        glDeleteProgram(program);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return 0;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

void OpenGLWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        m_leftButtonPressed = true;

    if (event->button() == Qt::RightButton)
        m_rightButtonPressed = true;

    m_lastMousePosition = event->pos();
}

void OpenGLWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_leftButtonPressed && !m_rightButtonPressed)
        return;

    const QPoint currentPosition = event->pos();
    const QPoint delta = currentPosition - m_lastMousePosition;

    m_lastMousePosition = currentPosition;

    // 左键：Camera Orbit
    if (m_leftButtonPressed)
    {
        const float sensitivity = 0.5f;

        m_cameraYaw += delta.x() * sensitivity;
        m_cameraPitch += delta.y() * sensitivity;

        if (m_cameraPitch > 89.0f)
            m_cameraPitch = 89.0f;

        if (m_cameraPitch < -89.0f)
            m_cameraPitch = -89.0f;
    }

    // 右键：修改右上前角的 Y
    if (m_rightButtonPressed)
    {
        m_cornerY -= delta.y() * 0.005f;

        if (m_cornerY < -1.5f)
            m_cornerY = -1.5f;

        if (m_cornerY > 1.5f)
            m_cornerY = 1.5f;

        updateCornerData();
    }

    update();
}

void OpenGLWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        m_leftButtonPressed = false;

    if (event->button() == Qt::RightButton)
        m_rightButtonPressed = false;
}

void OpenGLWidget::wheelEvent(QWheelEvent* event)
{
    const int delta = event->angleDelta().y();

    if (delta == 0)
        return;

    m_cameraDistance -= static_cast<float>(delta) / 120.0f * 0.2f;

    if (m_cameraDistance < 1.5f)
        m_cameraDistance = 1.5f;

    if (m_cameraDistance > 20.0f)
        m_cameraDistance = 20.0f;

    update();
}