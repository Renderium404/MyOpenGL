#include "Material.h"

#include <QDebug>

const char* materialTypeName(MaterialType type)
{
    switch (type)
    {
    case MaterialTypeVertexColor:
        return "VertexColor";
    case MaterialTypeLit:
        return "Lit";
    }

    return "Unknown";
}

Material::Material(const QString& name)
    : m_id(InvalidMaterialId)
    , m_name(name)
    , m_type(MaterialTypeVertexColor)
    , m_baseColor(1.0f, 1.0f, 1.0f, 1.0f)
    , m_specularColor(1.0f, 1.0f, 1.0f)
    , m_shininess(32.0f)                    // 32 为常见 Blinn/Phong 教学高光指数，具有较明显但不过窄的高光。
    , m_diffuseTextureId(InvalidResourceId)
{
}

/// 材质基本信息

MaterialId Material::id() const
{
    return m_id;
}

const QString& Material::name() const
{
    return m_name;
}

MaterialType Material::type() const
{
    return m_type;
}

/// 材质类型

void Material::setVertexColor()
{
    m_type = MaterialTypeVertexColor;
}

void Material::setLit()
{
    m_type = MaterialTypeLit;
}

/// 基础颜色

const QVector4D& Material::baseColor() const
{
    return m_baseColor;
}

bool Material::setBaseColor(const QVector4D& color)
{
    if (color.x() < 0.0f || color.y() < 0.0f || color.z() < 0.0f)
    {
        qWarning() << "Material setBaseColor failed: RGB components cannot be negative:" << m_name;
        return false;
    }

    if (color.w() < 0.0f || color.w() > 1.0f)
    {
        qWarning() << "Material setBaseColor failed: alpha must be between 0 and 1:" << m_name;
        return false;
    }

    m_baseColor = color;
    return true;
}

/// 镜面反射

const QVector3D& Material::specularColor() const
{
    return m_specularColor;
}

float Material::shininess() const
{
    return m_shininess;
}

bool Material::setSpecular(const QVector3D& color, float shininess)
{
    if (color.x() < 0.0f || color.y() < 0.0f || color.z() < 0.0f)
    {
        qWarning() << "Material setSpecular failed: color components cannot be negative:" << m_name;
        return false;
    }

    if (shininess <= 0.0f)
    {
        qWarning() << "Material setSpecular failed: shininess must be greater than zero:" << m_name;
        return false;
    }

    m_specularColor = color;
    m_shininess = shininess;
    return true;
}

/// Diffuse Texture

bool Material::hasDiffuseTexture() const
{
    return m_diffuseTextureId != InvalidResourceId;
}

ResourceId Material::diffuseTextureId() const
{
    return m_diffuseTextureId;
}

bool Material::setDiffuseTexture(ResourceId textureId)
{
    if (textureId == InvalidResourceId)
    {
        qWarning() << "Material setDiffuseTexture failed: texture ResourceId is invalid:" << m_name;
        return false;
    }

    m_diffuseTextureId = textureId;
    return true;
}

void Material::clearDiffuseTexture()
{
    m_diffuseTextureId = InvalidResourceId;
}

/// MaterialManager 内部接口

void Material::setId(MaterialId id)
{
    m_id = id;
}