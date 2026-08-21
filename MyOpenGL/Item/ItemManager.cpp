#include "ItemManager.h"

#include <QDebug>

#include <algorithm>

ItemManager::ItemManager()
    : m_nextId(1)
{
}

ItemManager::~ItemManager()
{
    clear();
}

/// Item 管理

RenderItem* ItemManager::createItem(const QString& name)
{
    const RenderItemId id = allocateId();

    if (id == InvalidRenderItemId)
    {
        qWarning() << "ItemManager createItem failed: unable to allocate RenderItemId:" << name;
        return 0;
    }

    RenderItem* item = new RenderItem(name);

    item->setId(id);
    m_items.push_back(item);
    m_itemsById[id] = item;

    return item;
}

RenderItem* ItemManager::get(RenderItemId id)
{
    ItemMap::iterator it = m_itemsById.find(id);
    return it != m_itemsById.end() ? it->second : 0;
}

const RenderItem* ItemManager::get(RenderItemId id) const
{
    ItemMap::const_iterator it = m_itemsById.find(id);
    return it != m_itemsById.end() ? it->second : 0;
}

bool ItemManager::contains(RenderItemId id) const
{
    return m_itemsById.find(id) != m_itemsById.end();
}

std::size_t ItemManager::count() const
{
    return m_items.size();
}

bool ItemManager::remove(RenderItemId id)
{
    ItemMap::iterator mapIterator = m_itemsById.find(id);

    if (mapIterator == m_itemsById.end())
    {
        qWarning() << "ItemManager remove failed: item does not exist:" << id;
        return false;
    }

    RenderItem* item = mapIterator->second;
    std::vector<RenderItem*>::iterator vectorIterator = std::find(m_items.begin(), m_items.end(), item);

    if (vectorIterator == m_items.end())
    {
        qWarning() << "ItemManager remove failed: internal Item collection is inconsistent:" << id;
        return false;
    }

    m_items.erase(vectorIterator);
    m_itemsById.erase(mapIterator);

    if (item != 0)
    {
        item->setId(InvalidRenderItemId);
        delete item;
    }

    return true;
}

void ItemManager::clear()
{
    for (std::size_t i = 0; i < m_items.size(); ++i)
    {
        RenderItem* item = m_items[i];

        if (item != 0)
        {
            item->setId(InvalidRenderItemId);
            delete item;
        }
    }

    m_items.clear();
    m_itemsById.clear();
    m_nextId = 1;
}

/// Item 查询

RenderItem* ItemManager::itemAt(int index)
{
    if (index < 0 || index >= static_cast<int>(m_items.size()))
    {
        qWarning() << "ItemManager itemAt failed: index is out of range:" << index;
        return 0;
    }

    return m_items[static_cast<std::size_t>(index)];
}

const RenderItem* ItemManager::itemAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_items.size()))
    {
        qWarning() << "ItemManager itemAt failed: index is out of range:" << index;
        return 0;
    }

    return m_items[static_cast<std::size_t>(index)];
}

/// Item Bounds

bool ItemManager::worldBounds(AxisAlignedBoundingBox& bounds, bool visibleOnly) const
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

/// RenderItemId

RenderItemId ItemManager::allocateId()
{
    while (m_nextId == InvalidRenderItemId || contains(m_nextId))
        ++m_nextId;

    const RenderItemId id = m_nextId;

    ++m_nextId;

    return id;
}