#include "Scene.h"

#include "RenderItem.h"

#include <QDebug>
#include <QVector4D>

#include <algorithm>
#include <cfloat>

ScenePrimitivePickQuery::ScenePrimitivePickQuery()
    : rayOrigin(0.0f, 0.0f, 0.0f)
    , rayDirection(0.0f, 0.0f, -1.0f)
    , screenPosition(0.0, 0.0)
    , viewportWidth(0)
    , viewportHeight(0)
    , pixelTolerance(5.0f)
    , filterPrimitiveType(false)
    , requiredPrimitiveType(PrimitivePickTriangle)
{
}

ScenePointHit::ScenePointHit()
    : item(0)
    , partId(DefaultRenderPartId)
    , vertexIndex(-1)
    , screenDistance(0.0f)
    , distance(0.0f)
    , position(0.0f, 0.0f, 0.0f)
{
}

ScenePrimitiveHit::ScenePrimitiveHit()
    : item(0)
    , partId(DefaultRenderPartId)
    , type(PrimitivePickTriangle)
    , primitiveIndex(-1)
    , vertexCount(0)
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

/// Picking

bool Scene::raycast(const RenderItemCandidates& candidates, const QVector3D& rayOrigin, const QVector3D& rayDirection, SceneRayHit& hit, bool visibleOnly) const
{
    hit = SceneRayHit();

    const float directionEpsilon = 1.0e-12f;

    if (rayDirection.lengthSquared() <= directionEpsilon)
    {
        qWarning() << "Scene raycast failed: ray direction is zero.";
        return false;
    }

    const QVector3D worldDirection = rayDirection.normalized();
    float nearestDistance = FLT_MAX;

    for (std::size_t i = 0; i < candidates.size(); ++i)
    {
        RenderItem* currentItem = candidates[i];

        if (currentItem == 0)
            continue;

        if (std::find(m_items.begin(), m_items.end(), currentItem) == m_items.end())
        {
            qWarning() << "Scene raycast ignored: candidate does not belong to this Scene.";
            continue;
        }

        if (visibleOnly && !currentItem->isVisible())
            continue;

        if (!currentItem->hasLocalBounds())
            continue;

        const AxisAlignedBoundingBox bounds = currentItem->worldBounds();
        float distance = 0.0f;

        if (!bounds.intersectRay(rayOrigin, worldDirection, distance))
            continue;

        if (distance >= nearestDistance)
            continue;

        nearestDistance = distance;
        hit.item = currentItem;
        hit.distance = distance;
        hit.position = rayOrigin + worldDirection * distance;
    }

    return hit.item != 0;
}

bool Scene::pickPoint(const RenderItemCandidates& candidates, const ScenePrimitivePickQuery& query, ScenePointHit& hit, bool visibleOnly) const
{
    hit = ScenePointHit();

    const float directionEpsilon = 1.0e-12f;

    if (query.rayDirection.lengthSquared() <= directionEpsilon)
    {
        qWarning() << "Scene pickPoint failed: ray direction is zero.";
        return false;
    }

    if (query.viewportWidth <= 0 || query.viewportHeight <= 0 || query.pixelTolerance <= 0.0f)
    {
        qWarning() << "Scene pickPoint failed: screen picking query is invalid.";
        return false;
    }

    const QVector3D worldDirection = query.rayDirection.normalized();
    float bestScreenDistance = FLT_MAX;
    float bestWorldDistance = FLT_MAX;

    for (std::size_t itemIndex = 0; itemIndex < candidates.size(); ++itemIndex)
    {
        RenderItem* currentItem = candidates[itemIndex];

        if (currentItem == 0)
            continue;

        if (std::find(m_items.begin(), m_items.end(), currentItem) == m_items.end())
        {
            qWarning() << "Scene pickPoint ignored: candidate does not belong to this Scene.";
            continue;
        }

        if (visibleOnly && !currentItem->isVisible())
            continue;

        const QMatrix4x4 model = currentItem->transform().matrix();
        bool invertible = false;
        const QMatrix4x4 inverseModel = model.inverted(&invertible);

        if (!invertible)
            continue;

        PrimitivePickContext localContext;
        localContext.rayOrigin = (inverseModel * QVector4D(query.rayOrigin, 1.0f)).toVector3D();

        const QVector3D localSecondPoint = (inverseModel * QVector4D(query.rayOrigin + worldDirection, 1.0f)).toVector3D();
        localContext.rayDirection = localSecondPoint - localContext.rayOrigin;

        if (localContext.rayDirection.lengthSquared() <= directionEpsilon)
            continue;

        localContext.rayDirection.normalize();
        localContext.screenPosition = query.screenPosition;
        localContext.localToClip = query.viewProjection * model;
        localContext.viewportWidth = query.viewportWidth;
        localContext.viewportHeight = query.viewportHeight;
        localContext.pixelTolerance = query.pixelTolerance;

        for (int partIndex = 0; partIndex < currentItem->partCount(); ++partIndex)
        {
            const RenderPart* currentPart = currentItem->partAt(partIndex);

            if (currentPart == 0 || currentPart->primitivePickSource() == 0)
                continue;

            PointPickHit localHit;

            if (!currentPart->primitivePickSource()->pickPoint(localContext, localHit))
                continue;

            const QVector3D worldPosition = (model * QVector4D(localHit.position, 1.0f)).toVector3D();
            const float worldDistance = QVector3D::dotProduct(worldPosition - query.rayOrigin, worldDirection);

            if (worldDistance < 0.0f)
                continue;

            // Point Snap 首先服从屏幕距离；屏幕距离近似相同时选择更靠近 Camera 的 Part Vertex。
            const float screenDistanceEpsilon = 1.0e-4f;
            const bool betterScreenDistance = localHit.screenDistance + screenDistanceEpsilon < bestScreenDistance;
            const bool sameScreenDistance = qAbs(localHit.screenDistance - bestScreenDistance) <= screenDistanceEpsilon;

            if (!betterScreenDistance && !(sameScreenDistance && worldDistance < bestWorldDistance))
                continue;

            bestScreenDistance = localHit.screenDistance;
            bestWorldDistance = worldDistance;

            hit.item = currentItem;
            hit.partId = currentPart->id();
            hit.vertexIndex = localHit.vertexIndex;
            hit.screenDistance = localHit.screenDistance;
            hit.distance = worldDistance;
            hit.position = worldPosition;
        }
    }

    return hit.item != 0;
}

bool Scene::pickPrimitive(const RenderItemCandidates& candidates, const ScenePrimitivePickQuery& query, ScenePrimitiveHit& hit, bool visibleOnly) const
{
    hit = ScenePrimitiveHit();

    const float directionEpsilon = 1.0e-12f;

    if (query.rayDirection.lengthSquared() <= directionEpsilon)
    {
        qWarning() << "Scene pickPrimitive failed: ray direction is zero.";
        return false;
    }

    if (query.viewportWidth <= 0 || query.viewportHeight <= 0 || query.pixelTolerance <= 0.0f)
    {
        qWarning() << "Scene pickPrimitive failed: screen picking query is invalid.";
        return false;
    }

    const QVector3D worldDirection = query.rayDirection.normalized();
    float nearestDistance = FLT_MAX;

    for (std::size_t itemIndex = 0; itemIndex < candidates.size(); ++itemIndex)
    {
        RenderItem* currentItem = candidates[itemIndex];

        if (currentItem == 0)
            continue;

        if (std::find(m_items.begin(), m_items.end(), currentItem) == m_items.end())
        {
            qWarning() << "Scene pickPrimitive ignored: candidate does not belong to this Scene.";
            continue;
        }

        if (visibleOnly && !currentItem->isVisible())
            continue;

        const QMatrix4x4 model = currentItem->transform().matrix();
        bool invertible = false;
        const QMatrix4x4 inverseModel = model.inverted(&invertible);

        if (!invertible)
            continue;

        PrimitivePickContext localContext;
        localContext.rayOrigin = (inverseModel * QVector4D(query.rayOrigin, 1.0f)).toVector3D();

        const QVector3D localSecondPoint = (inverseModel * QVector4D(query.rayOrigin + worldDirection, 1.0f)).toVector3D();
        localContext.rayDirection = localSecondPoint - localContext.rayOrigin;

        if (localContext.rayDirection.lengthSquared() <= directionEpsilon)
            continue;

        localContext.rayDirection.normalize();
        localContext.screenPosition = query.screenPosition;
        localContext.localToClip = query.viewProjection * model;
        localContext.viewportWidth = query.viewportWidth;
        localContext.viewportHeight = query.viewportHeight;
        localContext.pixelTolerance = query.pixelTolerance;

        for (int partIndex = 0; partIndex < currentItem->partCount(); ++partIndex)
        {
            const RenderPart* currentPart = currentItem->partAt(partIndex);

            if (currentPart == 0 || currentPart->primitivePickSource() == 0)
                continue;

            PrimitivePickHit localHit;

            // 不使用精确 Ray-AABB Broad Phase：
            // Line Picking 使用 Pixel Tolerance，数学 Ray 未穿过零厚度 Bounds 时仍然可能合法命中线段。
            if (!currentPart->primitivePickSource()->pickPrimitive(localContext, localHit))
                continue;

            if (query.filterPrimitiveType && localHit.type != query.requiredPrimitiveType)
                continue;

            if (localHit.vertexCount <= 0 || localHit.vertexCount > 3)
            {
                qWarning() << "Scene pickPrimitive ignored: PrimitivePickSource returned invalid vertex count.";
                continue;
            }

            const QVector3D worldPosition = (model * QVector4D(localHit.position, 1.0f)).toVector3D();
            const float worldDistance = QVector3D::dotProduct(worldPosition - query.rayOrigin, worldDirection);

            if (worldDistance < 0.0f || worldDistance >= nearestDistance)
                continue;

            nearestDistance = worldDistance;
            hit.item = currentItem;
            hit.partId = currentPart->id();
            hit.type = localHit.type;
            hit.primitiveIndex = localHit.primitiveIndex;
            hit.vertexCount = localHit.vertexCount;
            hit.distance = worldDistance;
            hit.position = worldPosition;
            hit.barycentric = localHit.barycentric;

            for (int vertexIndex = 0; vertexIndex < localHit.vertexCount; ++vertexIndex)
                hit.vertices[vertexIndex] = (model * QVector4D(localHit.vertices[vertexIndex], 1.0f)).toVector3D();
        }
    }

    return hit.item != 0;
}

