#ifndef TEXTURERESOURCE_H
#define TEXTURERESOURCE_H

#include "Core/Resource.h"

#include <QImage>
#include <QOpenGLFunctions_3_3_Core>
#include <QRect>

#include <vector>

/// 二维纹理资源。
/// CPU 端统一保存 RGBA8888 QImage，支持完整图片替换和矩形区域增量更新。
class TextureResource : public Resource
{
public:
    explicit TextureResource(const QString& name = "Texture", ResourceUpdatePolicy updatePolicy = ResourceUpdateDynamic);
    ~TextureResource() override;

    /// 纹理基本信息
    int width() const;
    int height() const;
    bool isEmpty() const;
    const QImage& image() const;

    /// CPU 数据
    bool setImage(const QImage& image);                          // 替换完整纹理数据，并标记全量更新。
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
    GLuint m_textureId;                 // 当前纹理对应的 OpenGL Texture Object。
    QImage m_image;                     // CPU 端 RGBA8888 完整纹理数据。
    std::vector<QRect> m_dirtyRegions;  // 等待 glTexSubImage2D() 同步的矩形区域。
};

#endif // TEXTURERESOURCE_H