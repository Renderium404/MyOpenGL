#ifndef SHADERPROGRAM_H
#define SHADERPROGRAM_H

#include <QOpenGLFunctions_3_3_Core>

/// OpenGL Shader Program 封装。
/// 负责 Shader 编译、Program 链接和 GPU Program 生命周期，不拥有 OpenGL Context。
class ShaderProgram
{
public:
    ShaderProgram();
    ~ShaderProgram();

    /// GPU 生命周期
    bool initialize(QOpenGLFunctions_3_3_Core* gl, const char* vertexSource, const char* fragmentSource); // 编译并链接完整 Shader Program。
    void release(QOpenGLFunctions_3_3_Core* gl);                                                          // 删除当前 GPU Program。

    /// Program 状态
    bool isInitialized() const;
    GLuint programId() const;
    void bind(QOpenGLFunctions_3_3_Core* gl) const;
    static void unbind(QOpenGLFunctions_3_3_Core* gl);

    /// Uniform
    GLint uniformLocation(QOpenGLFunctions_3_3_Core* gl, const char* name) const;

private:
    GLuint compileShader(QOpenGLFunctions_3_3_Core* gl, GLenum type, const char* source);

private:
    GLuint m_programId; // 当前 OpenGL Shader Program Object。
};

#endif // SHADERPROGRAM_H