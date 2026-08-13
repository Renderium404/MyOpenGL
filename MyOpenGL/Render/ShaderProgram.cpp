#include "ShaderProgram.h"

#include <QDebug>

ShaderProgram::ShaderProgram()
    : m_programId(0)
{
}

ShaderProgram::~ShaderProgram()
{
    if (m_programId != 0)
        qWarning() << "ShaderProgram destroyed while GPU program is still initialized.";
}

/// GPU 生命周期

bool ShaderProgram::initialize(QOpenGLFunctions_3_3_Core* gl, const char* vertexSource, const char* fragmentSource)
{
    if (gl == 0 || vertexSource == 0 || fragmentSource == 0)
    {
        qWarning() << "ShaderProgram initialize failed: invalid argument.";
        return false;
    }

    if (m_programId != 0)
    {
        qWarning() << "ShaderProgram initialize failed: program is already initialized.";
        return false;
    }

    const GLuint vertexShader = compileShader(gl, GL_VERTEX_SHADER, vertexSource);

    if (vertexShader == 0)
        return false;

    const GLuint fragmentShader = compileShader(gl, GL_FRAGMENT_SHADER, fragmentSource);

    if (fragmentShader == 0)
    {
        gl->glDeleteShader(vertexShader);
        return false;
    }

    const GLuint program = gl->glCreateProgram();
    gl->glAttachShader(program, vertexShader);
    gl->glAttachShader(program, fragmentShader);
    gl->glLinkProgram(program);

    GLint success = GL_FALSE;
    gl->glGetProgramiv(program, GL_LINK_STATUS, &success);

    if (success != GL_TRUE)
    {
        GLchar log[1024] = { 0 };
        gl->glGetProgramInfoLog(program, sizeof(log), 0, log);

        qWarning() << "ShaderProgram link failed:" << log;

        gl->glDeleteProgram(program);
        gl->glDeleteShader(vertexShader);
        gl->glDeleteShader(fragmentShader);
        return false;
    }

    gl->glDeleteShader(vertexShader);
    gl->glDeleteShader(fragmentShader);

    m_programId = program;
    return true;
}

void ShaderProgram::release(QOpenGLFunctions_3_3_Core* gl)
{
    if (m_programId == 0)
        return;

    if (gl == 0)
    {
        qWarning() << "ShaderProgram release failed: OpenGL functions is null.";
        return;
    }

    gl->glDeleteProgram(m_programId);
    m_programId = 0;
}

/// Program 状态

bool ShaderProgram::isInitialized() const
{
    return m_programId != 0;
}

GLuint ShaderProgram::programId() const
{
    return m_programId;
}

void ShaderProgram::bind(QOpenGLFunctions_3_3_Core* gl) const
{
    if (gl == 0 || m_programId == 0)
    {
        qWarning() << "ShaderProgram bind failed: program is not initialized.";
        return;
    }

    gl->glUseProgram(m_programId);
}

void ShaderProgram::unbind(QOpenGLFunctions_3_3_Core* gl)
{
    if (gl != 0)
        gl->glUseProgram(0);
}

/// Uniform

GLint ShaderProgram::uniformLocation(QOpenGLFunctions_3_3_Core* gl, const char* name) const
{
    if (gl == 0 || m_programId == 0 || name == 0)
    {
        qWarning() << "ShaderProgram uniformLocation failed: invalid argument.";
        return -1;
    }

    return gl->glGetUniformLocation(m_programId, name);
}

/// 内部辅助

GLuint ShaderProgram::compileShader(QOpenGLFunctions_3_3_Core* gl, GLenum type, const char* source)
{
    const GLuint shader = gl->glCreateShader(type);

    gl->glShaderSource(shader, 1, &source, 0);
    gl->glCompileShader(shader);

    GLint success = GL_FALSE;
    gl->glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (success != GL_TRUE)
    {
        GLchar log[1024] = { 0 };
        gl->glGetShaderInfoLog(shader, sizeof(log), 0, log);

        qWarning() << "ShaderProgram shader compilation failed:" << log;

        gl->glDeleteShader(shader);
        return 0;
    }

    return shader;
}