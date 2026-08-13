#include "TextureResource.h"

#include <QDebug>

#include <cstring>

TextureResource::TextureResource(const QString& name, ResourceUpdatePolicy updatePolicy)
    : Resource(name, ResourceTypeTexture, updatePolicy)
    , m_textureId(0)
{
}

TextureResource::~TextureResource()
{
}

/// 纹理基本信息

int TextureResource::width() const
{
    return m_image.width();
}

int TextureResource::height() const
{
    return m_image.height();
}

bool TextureResource::isEmpty() const
{
    return m_image.isNull();
}

const QImage& TextureResource::image() const
{
    return m_image;
}

/// CPU 数据

bool TextureResource::setImage(const QImage& image)
{
    if (image.isNull())
    {
        qWarning() << "TextureResource setImage failed: image is null:" << name();
        return false;
    }

    // CPU 纹理统一保存为 RGBA8888，使一个像素固定占用 4 Byte，并与 GL_RGBA / GL_UNSIGNED_BYTE 对应。
    m_image = image.convertToFormat(QImage::Format_RGBA8888);
    m_dirtyRegions.clear();
    markFullDirty();
    return true;
}

bool TextureResource::updateRegion(int x, int y, const QImage& regionImage)
{
    if (m_image.isNull())
    {
        qWarning() << "TextureResource updateRegion failed: texture image is empty:" << name();
        return false;
    }

    if (regionImage.isNull())
    {
        qWarning() << "TextureResource updateRegion failed: region image is null:" << name();
        return false;
    }

    if (x < 0 || y < 0)
    {
        qWarning() << "TextureResource updateRegion failed: region position cannot be negative:" << name();
        return false;
    }

    if (x + regionImage.width() > m_image.width() || y + regionImage.height() > m_image.height())
    {
        qWarning() << "TextureResource updateRegion failed: region exceeds texture bounds:" << name();
        return false;
    }

    const QImage source = regionImage.convertToFormat(QImage::Format_RGBA8888);
    const int bytesPerPixel = 4; // QImage::Format_RGBA8888 固定为 RGBA 四个 8-bit 通道。

    for (int row = 0; row < source.height(); ++row)
    {
        unsigned char* destination = m_image.scanLine(y + row) + x * bytesPerPixel;
        const unsigned char* sourceRow = source.constScanLine(row);
        std::memcpy(destination, sourceRow, source.width() * bytesPerPixel);
    }

    QRect dirtyRegion(x, y, source.width(), source.height());
    m_dirtyRegions.push_back(dirtyRegion);
    markPartialDirty();
    return true;
}

/// 增量更新状态

int TextureResource::dirtyRegionCount() const
{
    return static_cast<int>(m_dirtyRegions.size());
}

const std::vector<QRect>& TextureResource::dirtyRegions() const
{
    return m_dirtyRegions;
}

/// GPU 对象

GLuint TextureResource::textureId() const
{
    return m_textureId;
}

/// Resource GPU 实现

bool TextureResource::onInitializeGL(QOpenGLFunctions_3_3_Core* gl)
{
    if (!validateData())
        return false;

    gl->glGenTextures(1, &m_textureId);

    if (m_textureId == 0)
    {
        qWarning() << "TextureResource initialize failed: OpenGL texture creation failed:" << name();
        return false;
    }

    gl->glBindTexture(GL_TEXTURE_2D, m_textureId);

    // 第一版固定使用 Repeat + Linear，后续再将采样参数独立出来。
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // RGBA8888 每像素 4 Byte，每行天然满足 4 Byte 对齐。
    gl->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_image.width(), m_image.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, m_image.constBits());

    gl->glBindTexture(GL_TEXTURE_2D, 0);

    m_dirtyRegions.clear();
    return true;
}

bool TextureResource::onUpdateFullGL(QOpenGLFunctions_3_3_Core* gl)
{
    if (!validateData())
        return false;

    gl->glBindTexture(GL_TEXTURE_2D, m_textureId);
    gl->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    // glTexImage2D() 重新定义完整 Texture Storage，因此允许纹理尺寸发生变化。
    gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_image.width(), m_image.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, m_image.constBits());

    gl->glBindTexture(GL_TEXTURE_2D, 0);

    // Full Update 已覆盖所有局部修改，旧 Dirty Region 不再有效。
    m_dirtyRegions.clear();
    return true;
}

bool TextureResource::onUpdatePartialGL(QOpenGLFunctions_3_3_Core* gl)
{
    if (m_dirtyRegions.empty())
        return true;

    if (!validateData())
        return false;

    gl->glBindTexture(GL_TEXTURE_2D, m_textureId);
    gl->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    for (std::size_t i = 0; i < m_dirtyRegions.size(); ++i)
    {
        const QRect& region = m_dirtyRegions[i];

        // QImage::copy() 生成连续的局部 RGBA8888 数据，第一版优先保证更新逻辑清晰。
        const QImage regionImage = m_image.copy(region);
        gl->glTexSubImage2D(GL_TEXTURE_2D, 0, region.x(), region.y(), region.width(), region.height(), GL_RGBA, GL_UNSIGNED_BYTE, regionImage.constBits());
    }

    gl->glBindTexture(GL_TEXTURE_2D, 0);

    m_dirtyRegions.clear();
    return true;
}

void TextureResource::onReleaseGL(QOpenGLFunctions_3_3_Core* gl)
{
    if (m_textureId != 0)
    {
        gl->glDeleteTextures(1, &m_textureId);
        m_textureId = 0;
    }

    m_dirtyRegions.clear();
}

/// 内部辅助

bool TextureResource::validateData() const
{
    if (m_image.isNull())
    {
        qWarning() << "TextureResource validation failed: image is empty:" << name();
        return false;
    }

    if (m_image.format() != QImage::Format_RGBA8888)
    {
        qWarning() << "TextureResource validation failed: image format is not RGBA8888:" << name();
        return false;
    }

    return true;
}