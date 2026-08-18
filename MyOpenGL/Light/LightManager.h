#ifndef LIGHTMANAGER_H
#define LIGHTMANAGER_H

#include "Light.h"

#include <cstddef>
#include <map>
#include <vector>

/// 场景灯光管理器。
/// 负责 LightId、灯光所有权以及场景级环境光参数。
class LightManager
{
public:
    LightManager();
    ~LightManager();

    /// 灯光管理
    LightId add(Light* light);             // 注册灯光并取得所有权，成功后返回分配的 LightId。
    Light* get(LightId id);                // 获取指定灯光，不存在时返回 0。
    const Light* get(LightId id) const;    // 获取指定只读灯光，不存在时返回 0。
    const Light* firstEnabledDirectionalLight() const; // 兼容查询：获取第一个启用的方向光；Renderer 多光源路径不依赖该接口。
    void enabledLights(std::vector<const Light*>& lights) const; // 返回当前全部启用灯光的借用指针，顺序按 LightId 递增。
    bool contains(LightId id) const;
    std::size_t count() const;
    bool remove(LightId id);               // 删除指定灯光。
    void clear();                          // 删除全部灯光。

    /// 环境光
    const QVector3D& ambientColor() const;
    float ambientIntensity() const;
    bool setAmbientColor(const QVector3D& color); // 设置整个场景的环境光 RGB 比例。
    bool setAmbientIntensity(float intensity);    // 设置环境光整体强度，0 表示关闭环境光。

private:
    typedef std::map<LightId, Light*> LightMap;

    LightMap m_lights;             // 当前管理的全部 Light，LightManager 拥有这些对象。
    LightId m_nextId;              // 下一个可分配 LightId，0 保留为 InvalidLightId。
    QVector3D m_ambientColor;      // 场景级环境光 RGB 比例。
    float m_ambientIntensity;      // 场景级环境光整体强度倍率。
};

#endif // LIGHTMANAGER_H
