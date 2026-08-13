#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H
#include <QPoint>
#include <QVector3D>
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
class QMouseEvent;
class QWheelEvent;
class OpenGLWidget : public QOpenGLWidget,
                     protected QOpenGLFunctions_3_3_Core
{
public:
    explicit OpenGLWidget(QWidget* parent = 0);
    ~OpenGLWidget() override;

protected:
    void initializeGL() override;                   //openGL的准备工作
    void resizeGL(int w, int h) override;           //窗口缩放
    void paintGL() override;                        //openGL的绘制
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
private:

    GLuint compileShader(GLenum type, const char* source);      //shader编译
    GLuint createShaderProgram();                               //shader创建
    void updateCornerData();
private:
    GLuint m_vao;                   //描述如何解释顶点数据
    GLuint m_vbo;                   //顶点数据，存储所有的的顶点
    GLuint m_ebo;                   //索引数据，连接那些顶点形成三角面
    GLuint m_texture;               //纹理资源
    GLuint m_shaderProgram;         //保存我们使用的 Shader Program
    GLint m_modelLocation;          //模型位置
    GLint m_viewLocation;           //视口位置
    GLint m_projectionLocation;     //3D → 2D 的投影规则

    std::vector<GLfloat> m_vertices;

    QPoint m_lastMousePosition;

    bool m_leftButtonPressed;
    bool m_rightButtonPressed;

    float m_cameraYaw;
    float m_cameraPitch;
    float m_cameraDistance;
    QVector3D m_cameraTarget;

    float m_cornerY;
    bool m_cornerDataDirty;

    bool m_ready;
};

#endif // OPENGLWIDGET_H