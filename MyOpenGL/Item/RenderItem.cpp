#include "RenderItem.h"

#include <QDebug>
#include <QVector4D>
#include <cmath>
#include <cfloat>
#include <algorithm>
#include <cfloat>
#include <set>
#include "MyOpenGL/Resource/BufferGeometry.h"
namespace
{

bool intersectRayTriangle(const QVector3D& rayOrigin, const QVector3D& rayDirection,
                          const QVector3D& vertex0, const QVector3D& vertex1, const QVector3D& vertex2,
                          float& distance)
{
    const float epsilon = 1.0e-8f;

    const QVector3D edge1 = vertex1 - vertex0;
    const QVector3D edge2 = vertex2 - vertex0;

    const QVector3D p = QVector3D::crossProduct(rayDirection, edge2);
    const float determinant = QVector3D::dotProduct(edge1, p);

    // 双面命中，不进行 Back Face Culling。
    if (std::fabs(determinant) <= epsilon)
        return false;

    const float inverseDeterminant = 1.0f / determinant;
    const QVector3D t = rayOrigin - vertex0;
    const float u = QVector3D::dotProduct(t, p) * inverseDeterminant;

    if (u < 0.0f || u > 1.0f)
        return false;

    const QVector3D q = QVector3D::crossProduct(t, edge1);
    const float v = QVector3D::dotProduct(rayDirection, q) * inverseDeterminant;

    if (v < 0.0f || u + v > 1.0f)
        return false;

    const float rayDistance = QVector3D::dotProduct(edge2, q) * inverseDeterminant;

    if (rayDistance < 0.0f)
        return false;

    distance = rayDistance;
    return true;
}

QVector3D attributePosition(const AttributeValue& value)
{
    return QVector3D(value[0], value[1], value[2]);
}

}



RenderItemRayHit::RenderItemRayHit()
    : partId(InvalidRenderPartId)
    , distance(0.0f)
    , position(0.0f, 0.0f, 0.0f)
{
}

RenderItem::RenderItem(const QString& name)
    : m_id(InvalidRenderItemId)
    , m_name(name)
    , m_nextPartId(1)
    , m_nextLabelId(1)
    , m_material(0)
    , m_visible(true)
    , m_type(DisplayMode::Shaded)
    , m_edgeColor(0.05f, 0.05f, 0.05f, 1.0f)
    , m_depthTestEnabled(true)
{
}

RenderItem::~RenderItem()
{
    clearLabels();
    clearParts();
}

/// 基本信息

QString RenderItem::type() const
{
    switch (m_type)
    {
    case DisplayMode::Shaded:
        return "Shaded";

    case DisplayMode::Wireframe:
        return "Wireframe";

    case DisplayMode::ShadedWithEdges:
        return "ShadedWithEdges";
    }

    return "Unknown";
}

/// Part 管理

RenderPart* RenderItem::createPart()
{
    const RenderPartId id = allocatePartId();

    if (id == InvalidRenderPartId)
    {
        qWarning() << "RenderItem createPart failed: unable to allocate RenderPartId:"
                   << "Item=" << m_name;
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
    m_nextPartId = 1;
    m_localBoundsCache.reset();
}

int RenderItem::partCount() const
{
    return static_cast<int>(m_parts.size());
}

bool RenderItem::containsPart(RenderPartId id) const
{
    return m_partsById.find(id) != m_partsById.end();
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
/// Label 管理

RenderLabel* RenderItem::createLabel()
{
    const RenderLabelId id = allocateLabelId();

    if (id == InvalidRenderLabelId)
    {
        qWarning() << "RenderItem createLabel failed: unable to allocate RenderLabelId:"
                   << "Item=" << m_name;

        return 0;
    }

    RenderLabel* result = new RenderLabel(id);

    m_labels.push_back(result);
    m_labelsById[id] = result;

    return result;
}

bool RenderItem::removeLabel(RenderLabelId id)
{
    std::map<RenderLabelId, RenderLabel*>::iterator mapIterator = m_labelsById.find(id);

    if (mapIterator == m_labelsById.end())
        return false;

    RenderLabel* target = mapIterator->second;

    std::vector<RenderLabel*>::iterator vectorIterator =
        std::find(m_labels.begin(), m_labels.end(), target);

    if (vectorIterator == m_labels.end())
    {
        qWarning() << "RenderItem removeLabel failed: internal Label collection is inconsistent:"
                   << "Item=" << m_name
                   << "LabelId=" << static_cast<qulonglong>(id);

        return false;
    }

    m_labels.erase(vectorIterator);
    m_labelsById.erase(mapIterator);

    delete target;

    return true;
}

void RenderItem::clearLabels()
{
    for (std::size_t i = 0; i < m_labels.size(); ++i)
        delete m_labels[i];

    m_labels.clear();
    m_labelsById.clear();

    m_nextLabelId = 1;
}

int RenderItem::labelCount() const
{
    return static_cast<int>(m_labels.size());
}

bool RenderItem::containsLabel(RenderLabelId id) const
{
    return m_labelsById.find(id) != m_labelsById.end();
}

RenderLabel* RenderItem::labelAt(int index)
{
    if (index < 0 || index >= static_cast<int>(m_labels.size()))
        return 0;

    return m_labels[static_cast<std::size_t>(index)];
}

const RenderLabel* RenderItem::labelAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_labels.size()))
        return 0;

    return m_labels[static_cast<std::size_t>(index)];
}

RenderLabel* RenderItem::label(RenderLabelId id)
{
    std::map<RenderLabelId, RenderLabel*>::iterator it = m_labelsById.find(id);

    return it != m_labelsById.end() ? it->second : 0;
}

const RenderLabel* RenderItem::label(RenderLabelId id) const
{
    std::map<RenderLabelId, RenderLabel*>::const_iterator it = m_labelsById.find(id);

    return it != m_labelsById.end() ? it->second : 0;
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

        if (!containsPart(update.partId))
        {
            qWarning() << "RenderItem applyPartUpdates failed: Part does not exist:"
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
            if (!removePart(update.partId))
                return false;

            continue;
        }

        RenderPart* targetPart = part(update.partId);

        if (targetPart == 0)
            return false;

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

bool RenderItem::raycastBox(
    const QVector3D& rayOrigin,
    const QVector3D& rayDirection,
    RenderItemRayHit& hit) const
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

bool RenderItem::raycastPoint(const QVector3D& rayOrigin, const QVector3D& rayDirection, RenderItemRayHit& hit) const
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

    // World Ray -> Item Local Ray。
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

        if (currentPart == 0)
            continue;

        const Geometry* geometry = currentPart->geometry();

        if (geometry == 0)
            continue;

        // 当前精确命中只处理三角形 Primitive。
        if (geometry->renderType() != RenderType::Triangles)
            continue;

        // 第一层：Part Bounds 粗筛。
        if (currentPart->hasLocalBounds())
        {
            float boundsDistance = 0.0f;

            if (!currentPart->localBounds().intersectRay(localOrigin, localDirection, boundsDistance))
                continue;
        }

        // 第二层：取得 Position Attribute。
        AttributeIterator positionBegin = geometry->attributeBegin(GeometryAttribute::Position);
        AttributeIterator positionEnd = geometry->attributeEnd(GeometryAttribute::Position);

        if (positionBegin == positionEnd)
            continue;

        if (positionBegin.componentCount() < 3)
            continue;

        const AttributeIterator::difference_type vertexCount = positionEnd - positionBegin;

        if (vertexCount <= 0)
            continue;

        // 第三层：取得 Triangle Index。
        IndexIterator indexBegin = geometry->indexBegin();
        IndexIterator indexEnd = geometry->indexEnd();

        if (indexBegin == indexEnd)
            continue;

        if (indexEnd - indexBegin < 3)
            continue;

        // 每三个 Index 组成一个 Triangle。
        for (IndexIterator indexIt = indexBegin; indexEnd - indexIt >= 3; indexIt += 3)
        {
            const GLuint index0 = indexIt[0];
            const GLuint index1 = indexIt[1];
            const GLuint index2 = indexIt[2];

            if (index0 >= static_cast<GLuint>(vertexCount) ||
                index1 >= static_cast<GLuint>(vertexCount) ||
                index2 >= static_cast<GLuint>(vertexCount))
            {
                continue;
            }

            const QVector3D vertex0 = attributePosition(positionBegin[index0]);
            const QVector3D vertex1 = attributePosition(positionBegin[index1]);
            const QVector3D vertex2 = attributePosition(positionBegin[index2]);

            float localDistance = 0.0f;

            if (!intersectRayTriangle(localOrigin, localDirection, vertex0, vertex1, vertex2, localDistance))
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
    }

    return found;
}

bool RenderItem::raycast(
    const QVector3D& rayOrigin,
    const QVector3D& rayDirection,
    RenderItemRayHit& hit) const
{
    return raycastPoint(rayOrigin, rayDirection, hit);
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
bool RenderItem::setDisplayMode(DisplayMode mode)
{
    switch (mode)
    {
    case DisplayMode::Shaded:
    case DisplayMode::Wireframe:
    case DisplayMode::ShadedWithEdges:
        m_type = mode;
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

/// PartManager 内部接口

RenderPartId RenderItem::allocatePartId()
{
    while (m_nextPartId == InvalidRenderPartId || containsPart(m_nextPartId))
        ++m_nextPartId;

    const RenderPartId id = m_nextPartId;

    ++m_nextPartId;

    return id;
}
RenderLabelId RenderItem::allocateLabelId()
{
    while (m_nextLabelId == InvalidRenderLabelId || containsLabel(m_nextLabelId))
        ++m_nextLabelId;

    const RenderLabelId id = m_nextLabelId;

    ++m_nextLabelId;

    return id;
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