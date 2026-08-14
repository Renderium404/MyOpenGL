#include "Scene.h"

#include "Scene/RenderItem.h"

#include <QDebug>

#include <algorithm>
#include <cfloat>

ScenePrimitiveHit::ScenePrimitiveHit()
    : item(0)
    , primitiveIndex(-1)
    , distance(0.0f)
    , position(0.0f, 0.0f, 0.0f)
    , barycentric(0.0f, 0.0f, 0.0f)
{
    vertices[0] = QVector3D(0.0f, 0.0f, 0.0f);
    vertices[1] = QVector3D(0.0f, 0.0f, 0.0f);
    vertices[2] = QVector3D(0.0f, 0.0f, 0.0f);
}

SceneRayHit::SceneRayHit()
    : item(0)
    , distance(0.0f)
    , position(0.0f, 0.0f, 0.0f)
{
}

Scene::Scene()
    : m_selectedItem(0)
{
}

Scene::~Scene()
{
    clear();
}

/// Item 所有权

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

    if (m_selectedItem == item)
        m_selectedItem = 0;

    delete *it;
    m_items.erase(it);
    return true;
}

void Scene::clear()
{
    for (std::size_t i = 0; i < m_items.size(); ++i)
        delete m_items[i];

    m_items.clear();
    m_selectedItem = 0;
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


/// Picking / Selection

bool Scene::raycast(const QVector3D& rayOrigin, const QVector3D& rayDirection, SceneRayHit& hit, bool visibleOnly, bool skipPrimitivePickable)
{
    hit = SceneRayHit();

    float nearestDistance = FLT_MAX;

    for (std::size_t i = 0; i < m_items.size(); ++i)
    {
        RenderItem* currentItem = m_items[i];

        if (currentItem == 0)
            continue;

        if (visibleOnly && !currentItem->isVisible())
            continue;

        // 精确 Primitive Picking 失败后的 Fallback 只处理没有 Primitive Picker 的 Item，
        // 避免 Ray 没有命中 Triangle 却因为落在同一对象 AABB 内而产生误选择。
        if (skipPrimitivePickable && currentItem->primitivePickSource() != 0)
            continue;

        // 第一版对象 Picking 与 Fit All 使用同一个明确边界：没有 Local Bounds 的辅助 Item 不参与命中。
        if (!currentItem->hasLocalBounds())
            continue;

        const AxisAlignedBoundingBox bounds = currentItem->worldBounds();
        float distance = 0.0f;

        if (!bounds.intersectRay(rayOrigin, rayDirection, distance))
            continue;

        if (distance >= nearestDistance)
            continue;

        nearestDistance = distance;
        hit.item = currentItem;
        hit.distance = distance;
        hit.position = rayOrigin + rayDirection * distance;
    }

    return hit.item != 0;
}

bool Scene::raycastPrimitive(const QVector3D& rayOrigin, const QVector3D& rayDirection, ScenePrimitiveHit& hit, bool visibleOnly)
{
    hit = ScenePrimitiveHit();

    const float directionEpsilon = 1.0e-12f;

    if (rayDirection.lengthSquared() <= directionEpsilon)
    {
        qWarning() << "Scene raycastPrimitive failed: ray direction is zero.";
        return false;
    }

    const QVector3D worldDirection = rayDirection.normalized();
    float nearestDistance = FLT_MAX;

    for (std::size_t i = 0; i < m_items.size(); ++i)
    {
        RenderItem* currentItem = m_items[i];

        if (currentItem == 0)
            continue;

        if (visibleOnly && !currentItem->isVisible())
            continue;

        const PrimitivePickSource* pickSource = currentItem->primitivePickSource();

        if (pickSource == 0)
            continue;

        // 有 Bounds 时先执行廉价 AABB Broad Phase；没有 Bounds 的自定义 Picker 仍然可以直接参与精确 Picking。
        if (currentItem->hasLocalBounds())
        {
            float broadPhaseDistance = 0.0f;

            if (!currentItem->worldBounds().intersectRay(rayOrigin, worldDirection, broadPhaseDistance))
                continue;
        }

        const QMatrix4x4 model = currentItem->transform().matrix();
        bool invertible = false;
        const QMatrix4x4 inverseModel = model.inverted(&invertible);

        if (!invertible)
            continue;

        const QVector3D localOrigin = (inverseModel * QVector4D(rayOrigin, 1.0f)).toVector3D();
        const QVector3D localSecondPoint = (inverseModel * QVector4D(rayOrigin + worldDirection, 1.0f)).toVector3D();
        const QVector3D localDirection = localSecondPoint - localOrigin;

        if (localDirection.lengthSquared() <= directionEpsilon)
            continue;

        PrimitivePickHit localHit;

        if (!pickSource->raycastPrimitive(localOrigin, localDirection.normalized(), localHit))
            continue;

        const QVector3D worldPosition = (model * QVector4D(localHit.position, 1.0f)).toVector3D();
        const float worldDistance = QVector3D::dotProduct(worldPosition - rayOrigin, worldDirection);

        if (worldDistance < 0.0f || worldDistance >= nearestDistance)
            continue;

        nearestDistance = worldDistance;
        hit.item = currentItem;
        hit.primitiveIndex = localHit.primitiveIndex;
        hit.distance = worldDistance;
        hit.position = worldPosition;
        hit.barycentric = localHit.barycentric;

        for (int vertexIndex = 0; vertexIndex < 3; ++vertexIndex)
            hit.vertices[vertexIndex] = (model * QVector4D(localHit.vertices[vertexIndex], 1.0f)).toVector3D();
    }

    return hit.item != 0;
}

RenderItem* Scene::selectedItem()
{
    return m_selectedItem;
}

const RenderItem* Scene::selectedItem() const
{
    return m_selectedItem;
}

bool Scene::setSelectedItem(RenderItem* item)
{
    if (item == 0)
    {
        qWarning() << "Scene setSelectedItem failed: item is null; use clearSelection() to clear current Selection.";
        return false;
    }

    if (std::find(m_items.begin(), m_items.end(), item) == m_items.end())
    {
        qWarning() << "Scene setSelectedItem failed: item does not belong to this Scene:" << item->name();
        return false;
    }

    m_selectedItem = item;
    return true;
}

void Scene::clearSelection()
{
    m_selectedItem = 0;
}
