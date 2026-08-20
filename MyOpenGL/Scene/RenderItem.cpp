#include "RenderItem.h"

#include <QDebug>
#include <QVector4D>

#include <algorithm>
#include <cfloat>
#include <set>

RenderItemRayHit::RenderItemRayHit()
    : partId(DefaultRenderPartId)
    , distance(0.0f)
    , position(0.0f, 0.0f, 0.0f)
{
}

const char* renderItemDisplayModeName(RenderItemDisplayMode mode)
{
    switch (mode)
    {
    case RenderItemDisplayShaded:
        return "Shaded";

    case RenderItemDisplayWireframe:
        return "Wireframe";

    case RenderItemDisplayShadedWithEdges:
        return "ShadedWithEdges";
    }

    return "Unknown";
}

RenderItem::RenderItem(const QString& name)
    : m_name(name)
    , m_material(0)
    , m_visible(true)
    , m_displayMode(RenderItemDisplayShaded)
    , m_edgeColor(0.05f, 0.05f, 0.05f, 1.0f)
    , m_depthTestEnabled(true)
{
}

RenderItem::~RenderItem()
{
    clearParts();
}

/// 基本信息

const QString& RenderItem::name() const
{
    return m_name;
}

/// Part 管理

RenderPart* RenderItem::createPart(RenderPartId id)
{
    if (m_partsById.find(id) != m_partsById.end())
    {
        qWarning() << "RenderItem createPart failed: duplicate PartId:"
                   << "Item=" << m_name
                   << "PartId=" << static_cast<qulonglong>(id);
        return 0;
    }

    RenderPart* result = new RenderPart(id);

    m_parts.push_back(result);
    m_partsById[id] = result;

    return result;
}

bool RenderItem::removePart(RenderPartId id)
{
    std::map<RenderPartId, RenderPart*>::iterator mapIterator = m_partsById.find(id);

    if (mapIterator == m_partsById.end())
        return false;

    RenderPart* target = mapIterator->second;
    std::vector<RenderPart*>::iterator vectorIterator = std::find(m_parts.begin(), m_parts.end(), target);

    if (vectorIterator == m_parts.end())
    {
        qWarning() << "RenderItem removePart failed: internal Part collection is inconsistent:"
                   << "Item=" << m_name
                   << "PartId=" << static_cast<qulonglong>(id);
        return false;
    }

    m_parts.erase(vectorIterator);
    m_partsById.erase(mapIterator);

    delete target;

    return true;
}

void RenderItem::clearParts()
{
    for (std::size_t i = 0; i < m_parts.size(); ++i)
        delete m_parts[i];

    m_parts.clear();
    m_partsById.clear();
    m_localBoundsCache.reset();
}

int RenderItem::partCount() const
{
    return static_cast<int>(m_parts.size());
}

RenderPart* RenderItem::partAt(int index)
{
    if (index < 0 || index >= static_cast<int>(m_parts.size()))
        return 0;

    return m_parts[static_cast<std::size_t>(index)];
}

const RenderPart* RenderItem::partAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_parts.size()))
        return 0;

    return m_parts[static_cast<std::size_t>(index)];
}

RenderPart* RenderItem::part(RenderPartId id)
{
    std::map<RenderPartId, RenderPart*>::iterator it = m_partsById.find(id);
    return it != m_partsById.end() ? it->second : 0;
}

const RenderPart* RenderItem::part(RenderPartId id) const
{
    std::map<RenderPartId, RenderPart*>::const_iterator it = m_partsById.find(id);
    return it != m_partsById.end() ? it->second : 0;
}

/// Part Update

bool RenderItem::applyPartUpdate(const RenderPartUpdate& update)
{
    std::vector<RenderPartUpdate> updates;
    updates.push_back(update);

    return applyPartUpdates(updates);
}

bool RenderItem::applyPartUpdates(const std::vector<RenderPartUpdate>& updates)
{
    if (updates.empty())
        return true;

    std::set<RenderPartId> updatedPartIds;

    for (std::size_t i = 0; i < updates.size(); ++i)
    {
        const RenderPartUpdate& update = updates[i];

        if (!update.isValid())
        {
            qWarning() << "RenderItem applyPartUpdates failed: invalid RenderPartUpdate:"
                       << "Item=" << m_name
                       << "PartId=" << static_cast<qulonglong>(update.partId);
            return false;
        }

        if (!updatedPartIds.insert(update.partId).second)
        {
            qWarning() << "RenderItem applyPartUpdates failed: duplicate PartId:"
                       << "Item=" << m_name
                       << "PartId=" << static_cast<qulonglong>(update.partId);
            return false;
        }
    }

    for (std::size_t i = 0; i < updates.size(); ++i)
    {
        const RenderPartUpdate& update = updates[i];

        if (update.operation == RenderPartUpdateRemove)
        {
            if (part(update.partId) != 0 && !removePart(update.partId))
                return false;

            continue;
        }

        RenderPart* targetPart = part(update.partId);

        if (targetPart == 0)
        {
            targetPart = createPart(update.partId);

            if (targetPart == 0)
                return false;
        }

        targetPart->setGeometry(update.geometry);
        targetPart->setLocalBounds(update.localBounds);
    }

    return true;
}

/// Material

const Material* RenderItem::material() const
{
    return m_material;
}

void RenderItem::setMaterial(const Material* material)
{
    m_material = material;
}

/// Transform

Transform& RenderItem::transform()
{
    return m_transform;
}

const Transform& RenderItem::transform() const
{
    return m_transform;
}

/// Bounds

bool RenderItem::hasLocalBounds() const
{
    rebuildLocalBoundsCache();
    return m_localBoundsCache.isValid();
}

const AxisAlignedBoundingBox& RenderItem::localBounds() const
{
    rebuildLocalBoundsCache();
    return m_localBoundsCache;
}

AxisAlignedBoundingBox RenderItem::worldBounds() const
{
    rebuildLocalBoundsCache();

    if (!m_localBoundsCache.isValid())
        return AxisAlignedBoundingBox();

    return m_localBoundsCache.transformed(m_transform.matrix());
}

/// Item Interaction

bool RenderItem::raycast(const QVector3D& rayOrigin, const QVector3D& rayDirection, RenderItemRayHit& hit) const
{
    hit = RenderItemRayHit();

    if (rayDirection.lengthSquared() <= 1.0e-12f)
        return false;

    const QVector3D worldDirection = rayDirection.normalized();
    const QMatrix4x4 model = m_transform.matrix();

    bool invertible = false;
    const QMatrix4x4 inverseModel = model.inverted(&invertible);

    if (!invertible)
        return false;

    const QVector3D localOrigin = (inverseModel * QVector4D(rayOrigin, 1.0f)).toVector3D();
    const QVector3D localSecondPoint = (inverseModel * QVector4D(rayOrigin + worldDirection, 1.0f)).toVector3D();

    QVector3D localDirection = localSecondPoint - localOrigin;

    if (localDirection.lengthSquared() <= 1.0e-12f)
        return false;

    localDirection.normalize();

    bool found = false;
    float nearestDistance = FLT_MAX;

    for (std::size_t i = 0; i < m_parts.size(); ++i)
    {
        const RenderPart* currentPart = m_parts[i];

        if (currentPart == 0 || !currentPart->hasLocalBounds())
            continue;

        float localDistance = 0.0f;

        if (!currentPart->localBounds().intersectRay(localOrigin, localDirection, localDistance))
            continue;

        const QVector3D localPosition = localOrigin + localDirection * localDistance;
        const QVector3D worldPosition = (model * QVector4D(localPosition, 1.0f)).toVector3D();

        float worldDistance = QVector3D::dotProduct(worldPosition - rayOrigin, worldDirection);

        if (worldDistance < -1.0e-6f)
            continue;

        if (worldDistance < 0.0f)
            worldDistance = 0.0f;

        if (worldDistance >= nearestDistance)
            continue;

        nearestDistance = worldDistance;

        hit.partId = currentPart->id();
        hit.distance = worldDistance;
        hit.position = worldPosition;

        found = true;
    }

    return found;
}

/// Display

bool RenderItem::isVisible() const
{
    return m_visible;
}

void RenderItem::setVisible(bool visible)
{
    m_visible = visible;
}

RenderItemDisplayMode RenderItem::displayMode() const
{
    return m_displayMode;
}

bool RenderItem::setDisplayMode(RenderItemDisplayMode mode)
{
    switch (mode)
    {
    case RenderItemDisplayShaded:
    case RenderItemDisplayWireframe:
    case RenderItemDisplayShadedWithEdges:
        m_displayMode = mode;
        return true;
    }

    qWarning() << "RenderItem setDisplayMode failed: unsupported display mode:" << static_cast<int>(mode);
    return false;
}

const QVector4D& RenderItem::edgeColor() const
{
    return m_edgeColor;
}

void RenderItem::setEdgeColor(const QVector4D& color)
{
    m_edgeColor = color;
}

bool RenderItem::depthTestEnabled() const
{
    return m_depthTestEnabled;
}

void RenderItem::setDepthTestEnabled(bool enabled)
{
    m_depthTestEnabled = enabled;
}

/// 内部辅助

void RenderItem::rebuildLocalBoundsCache() const
{
    m_localBoundsCache.reset();

    for (std::size_t i = 0; i < m_parts.size(); ++i)
    {
        const RenderPart* currentPart = m_parts[i];

        if (currentPart != 0 && currentPart->hasLocalBounds())
            m_localBoundsCache.expandToInclude(currentPart->localBounds());
    }
}
