#ifndef LIGHT_H
#define LIGHT_H

#include <QString>
#include <QVector3D>

class LightManager;

/// 灯光唯一标识类型，由 LightManager 统一分配。
typedef unsigned int LightId;

/// 无效灯光 ID。
const LightId InvalidLightId = 0;

/// 灯光类型。
/// 不同灯光类型使用 Light 中不同的空间参数。
enum LightType
{
    LightTypeDirectional, // 方向光，只使用方向，不使用位置和距离衰减。
    LightTypePoint,       // 点光源，从一个位置向所有方向发光。
    LightTypeSpot         // 聚光灯，从一个位置沿指定方向形成锥形光束。
};

/// 获取灯光类型的调试名称。
const char* lightTypeName(LightType type);

/// 单个场景灯光状态。
/// 保存颜色、强度以及不同灯光类型所需的空间参数，不直接操作 OpenGL。
class Light
{
public:
    explicit Light(const QString& name = "Light");

    /// 灯光基本信息
    LightId id() const;
    const QString& name() const;
    LightType type() const;
    bool isEnabled() const;
    void setEnabled(bool enabled);

    /// 光照属性
    const QVector3D& color() const;
    float intensity() const;
    bool setColor(const QVector3D& color); // 设置 RGB 光照颜色，三个分量必须大于等于 0。
    bool setIntensity(float intensity);    // 设置光照强度，0 表示灯光不产生光照贡献。

    /// 空间属性
    const QVector3D& position() const;
    const QVector3D& direction() const;
    float range() const;
    float innerConeAngle() const;
    float outerConeAngle() const;

    /// 灯光类型设置
    bool setDirectional(const QVector3D& direction); // 切换为方向光，并设置从光源指向场景的单位方向。
    bool setPoint(const QVector3D& position, float range); // 切换为点光源，并设置位置和有效影响距离。
    bool setSpot(const QVector3D& position, const QVector3D& direction, float range, float innerConeAngle, float outerConeAngle); // 切换为聚光灯并设置完整锥体参数。

private:
    friend class LightManager;

    void setId(LightId id); // 仅允许 LightManager 设置灯光 ID。

private:
    LightId m_id;               // 当前灯光唯一标识，未注册时为 InvalidLightId。
    QString m_name;             // 当前灯光调试名称。
    LightType m_type;           // 当前灯光类型。
    bool m_enabled;             // 当前灯光是否参与场景光照。
    QVector3D m_color;          // 当前灯光 RGB 强度比例，不限制 HDR 分量大于 1。
    float m_intensity;          // 当前灯光整体强度倍率。
    QVector3D m_position;       // Point / Spot 灯光的世界坐标位置。
    QVector3D m_direction;      // Directional / Spot 灯光从光源指向场景的单位方向。
    float m_range;              // Point / Spot 灯光的有效影响距离。
    float m_innerConeAngle;     // Spot 完整强度区域的半锥角，单位为度。
    float m_outerConeAngle;     // Spot 光照衰减到边界的半锥角，单位为度。
};

#endif // LIGHT_H