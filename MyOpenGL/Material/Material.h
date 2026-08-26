#ifndef MATERIAL_H
#define MATERIAL_H

#include <QString>
#include <QVector4D>

class MaterialManager;
class Texture;

/// 材质唯一标识类型，由 MaterialManager 统一分配。
typedef unsigned int MaterialId;

/// 无效材质 ID。
const MaterialId InvalidMaterialId = 0;

/// 材质表面颜色来源。
enum class SurfaceMode
{
    Color,       // 使用 Material 统一颜色。
    VertexColor, // 使用 Geometry 顶点颜色插值。
    Texture      // 使用二维纹理采样颜色。
};

/// 基础表面材质。
/// 只描述表面颜色来源以及是否受到场景光照影响，不拥有 GPU Resource。
class Material
{
public:
    /// 基本信息
    MaterialId id() const { return m_id; }
    const QString& name() const { return m_name; }

    QString type() const;
    SurfaceMode surfaceMode() const { return m_type; }

    /// 光照
    bool lightingEnabled() const { return m_lightingEnabled; }
    void setLightingEnabled(bool enabled) { m_lightingEnabled = enabled; }

    /// 表面渲染
    bool setSurfaceMode(SurfaceMode mode);

    /// 统一颜色
    /// Texture 模式下作为纹理颜色乘数使用。
    const QVector4D& color() const { return m_color; }
    bool setColor(const QVector4D& color);

    /// 纹理
    Texture* texture() { return m_texture; }
    const Texture* texture() const { return m_texture; }
    void setTexture(Texture* texture) { m_texture = texture; }

private:
    friend class MaterialManager;

    /// MaterialManager 内部接口
    explicit Material(const QString& name);
    ~Material();

    void setId(MaterialId id) { m_id = id; }

private:
    MaterialId m_id;           // 材质唯一 ID。
    QString m_name;            // 材质名称。
    SurfaceMode m_type;        // 表面颜色来源。
    bool m_lightingEnabled;    // 是否受到场景光照影响。
    QVector4D m_color;         // Color 模式颜色；Texture 模式作为纹理颜色乘数。
    Texture* m_texture;        // Texture 模式使用的纹理，不拥有对象。
};

#endif // MATERIAL_H
