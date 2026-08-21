#ifndef MATERIALMANAGER_H
#define MATERIALMANAGER_H

#include "Material.h"

#include <cstddef>
#include <map>

/// 材质管理器。
/// 负责 MaterialId 分配、材质所有权以及材质查询和删除。
class MaterialManager
{
public:
    MaterialManager();
    ~MaterialManager();

    /// 材质管理
    Material* createMaterial(const QString& name = "Material");
    Material* get(MaterialId id);                   // 获取指定材质，不存在时返回 0。
    const Material* get(MaterialId id) const;       // 获取指定只读材质，不存在时返回 0。
    bool contains(MaterialId id) const;
    std::size_t count() const;
    bool remove(MaterialId id);                     // 删除指定材质，不影响材质引用的 Texture。
    void clear();                                   // 删除全部材质。

private:
    typedef std::map<MaterialId, Material*> MaterialMap;
    MaterialId allocateId();

private:
    MaterialMap m_materials; // 当前管理的全部 Material，MaterialManager 拥有这些对象。
    MaterialId m_nextId;     // 下一个可分配 MaterialId，0 保留为 InvalidMaterialId。
};

#endif // MATERIALMANAGER_H