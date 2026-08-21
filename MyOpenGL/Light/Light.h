#ifndef LIGHT_H
#define LIGHT_H

#include <QString>
#include <QVector3D>

class LightManager;

/// 灯光唯一标识类型，由 LightManager 统一分配。
typedef unsigned int LightId;

const LightId InvalidLightId = 0;

/// 灯光类型。
enum class LightType
{
    Ambient,     // 环境光。
    Directional, // 平行光。
    Point,       // 点光源。
    Spot         // 聚光灯。
};

/// 场景灯光。
/// 所有灯光都具有颜色和强度，不同类型使用不同的空间参数。
class Light
{
public:
    /// 基本信息
    LightId id() const { return m_id; }
    const QString& name() const { return m_name; }

    QString type() const;
    LightType lightType() const { return m_type; }

    /// 状态
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

    /// 基础光照
    const QVector3D& color() const { return m_color; }
    float intensity() const { return m_intensity; }

    bool setColor(const QVector3D& color);
    bool setIntensity(float intensity);

    /// 空间参数
    const QVector3D& position() const { return m_position; }
    const QVector3D& direction() const { return m_direction; }
    float range() const { return m_range; }

    /// Spot 参数
    float innerConeAngle() const { return m_innerConeAngle; }
    float outerConeAngle() const { return m_outerConeAngle; }

    /// 类型设置
    void setAmbient();
    bool setDirectional(const QVector3D& direction);
    bool setPoint(const QVector3D& position, float range);
    bool setSpot(const QVector3D& position, const QVector3D& direction, float range, float innerConeAngle, float outerConeAngle);

private:
    friend class LightManager;

    /// LightManager 内部接口
    explicit Light(const QString& name);
    ~Light();

    void setId(LightId id) { m_id = id; }

private:
    LightId m_id;              // 灯光唯一 ID。
    QString m_name;            // 灯光名称。
    LightType m_type;          // 灯光类型。
    bool m_enabled;            // 是否参与场景光照。

    QVector3D m_color;         // RGB 光照颜色。
    float m_intensity;         // 光照强度。

    QVector3D m_position;      // Point / Spot 世界位置。
    QVector3D m_direction;     // Directional / Spot 发光方向。
    float m_range;             // Point / Spot 有效距离。

    float m_innerConeAngle;    // Spot 内锥半角，单位为度。
    float m_outerConeAngle;    // Spot 外锥半角，单位为度。
};

#endif // LIGHT_H