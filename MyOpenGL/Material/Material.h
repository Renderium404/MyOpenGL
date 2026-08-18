#ifndef MATERIAL_H
#define MATERIAL_H

#include "MyOpenGL/Core/Resource.h"

#include <QString>
#include <QVector3D>
#include <QVector4D>

class MaterialManager;

/// 材质唯一标识类型，由 MaterialManager 统一分配。
typedef unsigned int MaterialId;

/// 无效材质 ID。
const MaterialId InvalidMaterialId = 0;

/// 材质类型。
/// 描述 Renderer 应使用哪一种基础渲染管线处理当前材质。
enum MaterialType
{
    MaterialTypeVertexColor,    // 无光照顶点颜色材质，颜色直接来自 Geometry Vertex。
    MaterialTypeLit,            // 受光照材质，使用 BaseColor、Specular、Texture 等表面参数。
    MaterialTypeLitVertexColor  // 受光照顶点颜色材质，使用 Position + Normal + Color，适合建模/仿真生成的面颜色。
};

/// 获取材质类型的调试名称。
const char* materialTypeName(MaterialType type);

/// 表面材质状态。
/// 不拥有 GPU 对象，通过 ResourceId 引用 Texture，并保存光照所需的表面参数。
class Material
{
public:
    explicit Material(const QString& name = "Material");

    /// 材质基本信息
    MaterialId id() const;
    const QString& name() const;
    MaterialType type() const;

    /// 材质类型
    void setVertexColor();    // 切换到无光照 Vertex Color 管线。
    void setLit();            // 切换到 Position + Normal + UV 的受光照材质管线。
    void setLitVertexColor(); // 切换到 Position + Normal + Color 的受光照材质管线。

    /// 基础颜色
    const QVector4D& baseColor() const;
    bool setBaseColor(const QVector4D& color); // 设置 RGBA 基础颜色；RGB 可大于 1，Alpha 必须位于 0~1。

    /// 镜面反射
    const QVector3D& specularColor() const;
    float shininess() const;
    bool setSpecular(const QVector3D& color, float shininess); // 设置镜面反射颜色和高光指数。

    /// Diffuse Texture
    bool hasDiffuseTexture() const;
    ResourceId diffuseTextureId() const;
    bool setDiffuseTexture(ResourceId textureId); // 设置 Diffuse Texture 引用，实际资源有效性由 Renderer 检查。
    void clearDiffuseTexture();

private:
    friend class MaterialManager;

    void setId(MaterialId id); // 仅允许 MaterialManager 设置材质 ID。

private:
    MaterialId m_id;                 // 当前材质唯一标识，未注册时为 InvalidMaterialId。
    QString m_name;                  // 当前材质调试名称。
    MaterialType m_type;             // 当前材质使用的基础渲染类型。
    QVector4D m_baseColor;           // Lit Material 的 RGBA 基础颜色。
    QVector3D m_specularColor;       // Lit Material 的镜面反射 RGB 系数。
    float m_shininess;               // 镜面高光指数，值越大则高光区域越集中。
    ResourceId m_diffuseTextureId;   // 当前 Diffuse Texture 的 ResourceId，0 表示不使用纹理。
};

#endif // MATERIAL_H
