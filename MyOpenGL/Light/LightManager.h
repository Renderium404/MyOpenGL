#ifndef LIGHTMANAGER_H
#define LIGHTMANAGER_H

#include "Light.h"

#include <cstddef>
#include <map>
#include <vector>

/// 场景灯光管理器。
/// 负责 LightId 分配、灯光所有权以及灯光查询。
class LightManager
{
public:
    LightManager();
    ~LightManager();

    /// 灯光管理
    Light* createLight(const QString& name = "Light");
    Light* get(LightId id);
    const Light* get(LightId id) const;
    bool contains(LightId id) const;
    std::size_t count() const;
    bool remove(LightId id);
    void clear();

    /// 灯光查询，找到所有可用的灯光
    void enabledLights(std::vector<const Light*>& lights) const;

private:
    typedef std::map<LightId, Light*> LightMap;

    LightId allocateId();

private:
    LightMap m_lights; // 当前管理的全部 Light，LightManager 拥有这些对象。
    LightId m_nextId;  // 下一个可分配 LightId。
};

#endif // LIGHTMANAGER_H
