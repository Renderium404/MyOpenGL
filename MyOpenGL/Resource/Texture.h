#ifndef TEXTURE_H
#define TEXTURE_H

#include "MyOpenGL/Core/Resource.h"

#include <QImage>
#include <QOpenGLFunctions_3_3_Core>
#include <QRect>

#include <vector>

/// 二维纹理资源。
/// MyOpenGL 拥有 CPU Image 和 OpenGL Texture Object，支持完整更新和矩形区域增量更新。
class Texture : public Resource
{
public:
    explicit Texture(const QString& name = "Texture", ResourceUpdatePolicy updatePolicy = ResourceUpdateDynamic);
    ~Texture() override;

    /// 纹理基本信息
    int width() const;
    int height() const;
    bool isEmpty() const;
    const QImage& image() const;

    /// CPU 数据
    bool setImage(const QImage& image);                         // 替换完整纹理数据，并标记全量更新。
    bool updateRegion(int x, int y, const QImage& regionImage); // 修改已有纹理中的矩形区域，并记录局部更新区域。

    /// 增量更新状态
    int dirtyRegionCount() const;
    const std::vector<QRect>& dirtyRegions() const;

    /// GPU 对象
    GLuint textureId() const;

protected:
    /// Resource GPU 实现
    bool onInitializeGL(QOpenGLFunctions_3_3_Core* gl) override;
    bool onUpdateFullGL(QOpenGLFunctions_3_3_Core* gl) override;
    bool onUpdatePartialGL(QOpenGLFunctions_3_3_Core* gl) override;
    void onReleaseGL(QOpenGLFunctions_3_3_Core* gl) override;

private:
    /// 内部辅助
    bool validateData() const;

private:
    GLuint m_textureId;                // 当前纹理对应的 OpenGL Texture Object。
    QImage m_image;                    // CPU 端 RGBA8888 完整纹理数据。
    std::vector<QRect> m_dirtyRegions; // 等待 glTexSubImage2D() 同步的矩形区域。
};

#endif // TEXTURE_H
