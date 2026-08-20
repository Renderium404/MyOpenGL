#include "Scene.h"

#include "RenderItem.h"

#include <QDebug>

#include <algorithm>

Scene::Scene()
{
}

Scene::~Scene()
{
    clear();
}

/// Item 管理

RenderItem* Scene::createItem(const QString& name)
{
    RenderItem* item = new RenderItem(name);
    m_items.push_back(item);
    return item;
}

bool Scene::removeItem(RenderItem* item)
{
    if (item == 0)
    {
        qWarning() << "Scene removeItem failed: item is null.";
        return false;
    }

    std::vector<RenderItem*>::iterator it = std::find(m_items.begin(), m_items.end(), item);

    if (it == m_items.end())
    {
        qWarning() << "Scene removeItem failed: item does not belong to this Scene:" << item->name();
        return false;
    }

    delete *it;
    m_items.erase(it);
    return true;
}

void Scene::clear()
{
    for (std::size_t i = 0; i < m_items.size(); ++i)
        delete m_items[i];

    m_items.clear();
}

/// Item 查询

int Scene::itemCount() const
{
    return static_cast<int>(m_items.size());
}

RenderItem* Scene::item(int index)
{
    if (index < 0 || index >= static_cast<int>(m_items.size()))
    {
        qWarning() << "Scene item failed: index is out of range:" << index;
        return 0;
    }

    return m_items[index];
}

const RenderItem* Scene::item(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_items.size()))
    {
        qWarning() << "Scene item failed: index is out of range:" << index;
        return 0;
    }

    return m_items[index];
}

/// Scene Bounds

bool Scene::worldBounds(AxisAlignedBoundingBox& bounds, bool visibleOnly) const
{
    bounds.reset();

    for (std::size_t i = 0; i < m_items.size(); ++i)
    {
        const RenderItem* currentItem = m_items[i];

        if (currentItem == 0)
            continue;

        if (visibleOnly && !currentItem->isVisible())
            continue;

        if (!currentItem->hasLocalBounds())
            continue;

        bounds.expandToInclude(currentItem->worldBounds());
    }

    return bounds.isValid();
}
