#include "Material.h"

#include <QDebug>

Material::Material(const QString& name)
    : m_id(InvalidMaterialId)
    , m_name(name)
    , m_type(SurfaceMode::Color)
    , m_lightingEnabled(true)
    , m_color(1.0f, 1.0f, 1.0f, 1.0f)
    , m_texture(0)
{
}

Material::~Material()
{
}

/// 基本信息

QString Material::type() const
{
    switch (m_type)
    {
    case SurfaceMode::Color:
        return "Color";

    case SurfaceMode::VertexColor:
        return "VertexColor";

    case SurfaceMode::Texture:
        return "Texture";
    }

    return "Unknown";
}

/// 表面渲染

bool Material::setSurfaceMode(SurfaceMode mode)
{
    switch (mode)
    {
    case SurfaceMode::Color:
    case SurfaceMode::VertexColor:
    case SurfaceMode::Texture:
        m_type = mode;
        return true;
    }

    qWarning() << "Material setSurfaceMode failed: unsupported surface mode:" << static_cast<int>(mode);
    return false;
}

/// 统一颜色

bool Material::setColor(const QVector4D& color)
{
    if (color.x() < 0.0f || color.y() < 0.0f || color.z() < 0.0f)
    {
        qWarning() << "Material setColor failed: RGB components cannot be negative:" << m_name;
        return false;
    }

    if (color.w() < 0.0f || color.w() > 1.0f)
    {
        qWarning() << "Material setColor failed: alpha must be between 0 and 1:" << m_name;
        return false;
    }

    m_color = color;
    return true;
}