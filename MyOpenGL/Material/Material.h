#ifndef MATERIAL_H
#define MATERIAL_H

#include <QString>
#include <QVector4D>

class MaterialManager;

/// 材质唯一标识类型，由 MaterialManager 统一分配。
typedef unsigned int MaterialId;

/// 无效材质 ID。
const MaterialId InvalidMaterialId = 0;

/// 材质表面颜色来源。
/// 后续可拓展为纹理材质
enum class SurfaceMode
{
    Color,      // 使用 Material 统一颜色。
    VertexColor // 使用 Geometry 顶点颜色插值。
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
    const QVector4D& color() const { return m_color; }
    bool setColor(const QVector4D& color);

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
    QVector4D m_color;         // Color 模式使用的统一颜色。
};

#endif // MATERIAL_H
