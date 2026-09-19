#include "RenderItem.h"

#include <QDebug>
#include <QMatrix4x4>
#include <QVector4D>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>

#include "MyOpenGL/Material/Material.h"
#include "MyOpenGL/Resource/Geometry.h"
namespace
{

bool intersectRayTriangle(const QVector3D& rayOrigin, const QVector3D& rayDirection, const QVector3D& vertex0, const QVector3D& vertex1, const QVector3D& vertex2, float& distance)
{
    const float epsilon = 1.0e-8f;

    const QVector3D edge1 = vertex1 - vertex0;
    const QVector3D edge2 = vertex2 - vertex0;

    const QVector3D p = QVector3D::crossProduct(rayDirection, edge2);
    const float determinant = QVector3D::dotProduct(edge1, p);

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
    , m_material(0)
    , m_visible(true)
    , m_type(DisplayMode::Shaded)
    , m_edgeColor(0.05f, 0.05f, 0.05f, 1.0f)
    , m_depthTestEnabled(true)
    , m_depthWriteEnabled(true)
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

/// Render

bool RenderItem::drawParts(Renderer& renderer,
                           const RenderContext& context,
                           const std::vector<const Light*>& lights) const
{
    if (!m_visible)
        return true;

    for (std::size_t i = 0; i < m_parts.size(); ++i)
    {
        const RenderPart* currentPart = m_parts[i];

        if (currentPart == 0)
        {
            qWarning() << "RenderItem drawParts failed: null RenderPart in internal collection:"
                       << "Item=" << m_name
                       << "Index=" << static_cast<qulonglong>(i);
            return false;
        }

        if (!currentPart->draw(renderer, *this, context, lights))
        {
            qWarning() << "RenderItem drawParts failed while drawing RenderPart:"
                       << "Item=" << m_name
                       << "PartId=" << static_cast<qulonglong>(currentPart->id());
            return false;
        }
    }

    return true;
}

bool RenderItem::drawLabels(Renderer& renderer,
                            const RenderContext& context,
                            const std::vector<const Light*>& lights) const
{
    if (!m_visible)
        return true;

    for (std::size_t i = 0; i < m_labels.size(); ++i)
    {
        const RenderLabel* currentLabel = m_labels[i];

        if (currentLabel == 0)
        {
            qWarning() << "RenderItem drawLabels failed: null RenderLabel in internal collection:"
                       << "Item=" << m_name
                       << "Index=" << static_cast<qulonglong>(i);
            return false;
        }

        if (!currentLabel->draw(renderer, *this, context, lights))
        {
            qWarning() << "RenderItem drawLabels failed while drawing RenderLabel:"
                       << "Item=" << m_name
                       << "LabelId=" << static_cast<qulonglong>(currentLabel->id());
            return false;
        }
    }

    return true;
}

/// Part 组织

bool RenderItem::addPart(RenderPart* part)
{
    if (part == 0 || part->id() == InvalidRenderPartId || containsPart(part->id()) || containsLabel(part->id()))
        return false;

    m_parts.push_back(part);
    m_partsById[part->id()] = part;
    m_localBoundsCache.reset();
    return true;
}

bool RenderItem::removePart(RenderPartId id)
{
    std::map<RenderPartId, RenderPart*>::iterator mapIterator = m_partsById.find(id);

    if (mapIterator == m_partsById.end())
        return false;

    RenderPart* target = mapIterator->second;
    std::vector<RenderPart*>::iterator vectorIterator = std::find(m_parts.begin(), m_parts.end(), target);

    if (vectorIterator == m_parts.end())
        return false;

    m_parts.erase(vectorIterator);
    m_partsById.erase(mapIterator);
    m_localBoundsCache.reset();
    return true;
}

void RenderItem::clearParts()
{
    m_parts.clear();
    m_partsById.clear();
    m_localBoundsCache.reset();
}

/// Part 查询

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
    std::map<RenderPartId, RenderPart*>::iterator iterator = m_partsById.find(id);
    return iterator != m_partsById.end() ? iterator->second : 0;
}

const RenderPart* RenderItem::part(RenderPartId id) const
{
    std::map<RenderPartId, RenderPart*>::const_iterator iterator = m_partsById.find(id);
    return iterator != m_partsById.end() ? iterator->second : 0;
}

/// Label 组织

bool RenderItem::addLabel(RenderLabel* label)
{
    if (label == 0 || label->id() == InvalidRenderLabelId || containsPart(label->id()) || containsLabel(label->id()))
        return false;

    m_labels.push_back(label);
    m_labelsById[label->id()] = label;
    return true;
}

bool RenderItem::removeLabel(RenderLabelId id)
{
    std::map<RenderLabelId, RenderLabel*>::iterator mapIterator = m_labelsById.find(id);

    if (mapIterator == m_labelsById.end())
        return false;

    RenderLabel* target = mapIterator->second;
    std::vector<RenderLabel*>::iterator vectorIterator = std::find(m_labels.begin(), m_labels.end(), target);

    if (vectorIterator == m_labels.end())
        return false;

    m_labels.erase(vectorIterator);
    m_labelsById.erase(mapIterator);
    return true;
}

void RenderItem::clearLabels()
{
    m_labels.clear();
    m_labelsById.clear();
}

/// Label 查询

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
    std::map<RenderLabelId, RenderLabel*>::iterator iterator = m_labelsById.find(id);
    return iterator != m_labelsById.end() ? iterator->second : 0;
}

const RenderLabel* RenderItem::label(RenderLabelId id) const
{
    std::map<RenderLabelId, RenderLabel*>::const_iterator iterator = m_labelsById.find(id);
    return iterator != m_labelsById.end() ? iterator->second : 0;
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

/// Interaction

bool RenderItem::raycastBox(const QVector3D& rayOrigin, const QVector3D& rayDirection, RenderItemRayHit& hit) const
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

    const QVector3D itemLocalOrigin = (inverseModel * QVector4D(rayOrigin, 1.0f)).toVector3D();
    const QVector3D itemLocalSecondPoint = (inverseModel * QVector4D(rayOrigin + worldDirection, 1.0f)).toVector3D();

    QVector3D itemLocalDirection = itemLocalSecondPoint - itemLocalOrigin;

    if (itemLocalDirection.lengthSquared() <= 1.0e-12f)
        return false;

    itemLocalDirection.normalize();

    bool found = false;
    float nearestDistance = FLT_MAX;

    for (std::size_t i = 0; i < m_parts.size(); ++i)
    {
        const RenderPart* currentPart = m_parts[i];

        if (currentPart == 0 || !currentPart->isStandardModel() || !currentPart->hasLocalBounds())
            continue;

        const QVector3D partLocalOrigin = itemLocalOrigin - currentPart->anchor3D();

        float partLocalDistance = 0.0f;

        if (!currentPart->localBounds().intersectRay(partLocalOrigin, itemLocalDirection, partLocalDistance))
            continue;

        const QVector3D partLocalPosition = partLocalOrigin + itemLocalDirection * partLocalDistance;
        const QVector3D itemLocalPosition = partLocalPosition + currentPart->anchor3D();
        const QVector3D worldPosition = (model * QVector4D(itemLocalPosition, 1.0f)).toVector3D();

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

    const QVector3D itemLocalOrigin = (inverseModel * QVector4D(rayOrigin, 1.0f)).toVector3D();
    const QVector3D itemLocalSecondPoint = (inverseModel * QVector4D(rayOrigin + worldDirection, 1.0f)).toVector3D();

    QVector3D itemLocalDirection = itemLocalSecondPoint - itemLocalOrigin;

    if (itemLocalDirection.lengthSquared() <= 1.0e-12f)
        return false;

    itemLocalDirection.normalize();

    bool found = false;
    float nearestDistance = FLT_MAX;

    for (std::size_t i = 0; i < m_parts.size(); ++i)
    {
        const RenderPart* currentPart = m_parts[i];

        if (currentPart == 0 || !currentPart->isStandardModel())
            continue;

        const Geometry* geometry = currentPart->geometry();

        if (geometry == 0 || geometry->renderType() != RenderType::Triangles)
            continue;

        const QVector3D partLocalOrigin = itemLocalOrigin - currentPart->anchor3D();

        if (currentPart->hasLocalBounds())
        {
            float boundsDistance = 0.0f;

            if (!currentPart->localBounds().intersectRay(partLocalOrigin, itemLocalDirection, boundsDistance))
                continue;
        }

        AttributeIterator positionBegin = geometry->attributeBegin(GeometryAttribute::Position);
        AttributeIterator positionEnd = geometry->attributeEnd(GeometryAttribute::Position);

        if (positionBegin == positionEnd || positionBegin.componentCount() < 3)
            continue;

        const AttributeIterator::difference_type vertexCount = positionEnd - positionBegin;

        if (vertexCount <= 0)
            continue;

        IndexIterator indexBegin = geometry->indexBegin();
        IndexIterator indexEnd = geometry->indexEnd();

        if (indexBegin == indexEnd || indexEnd - indexBegin < 3)
            continue;

        for (IndexIterator indexIt = indexBegin; indexEnd - indexIt >= 3; indexIt += 3)
        {
            const GLuint index0 = indexIt[0];
            const GLuint index1 = indexIt[1];
            const GLuint index2 = indexIt[2];

            if (index0 >= static_cast<GLuint>(vertexCount) || index1 >= static_cast<GLuint>(vertexCount) || index2 >= static_cast<GLuint>(vertexCount))
                continue;

            const QVector3D vertex0 = attributePosition(positionBegin[index0]);
            const QVector3D vertex1 = attributePosition(positionBegin[index1]);
            const QVector3D vertex2 = attributePosition(positionBegin[index2]);

            float partLocalDistance = 0.0f;

            if (!intersectRayTriangle(partLocalOrigin, itemLocalDirection, vertex0, vertex1, vertex2, partLocalDistance))
                continue;

            const QVector3D partLocalPosition = partLocalOrigin + itemLocalDirection * partLocalDistance;
            const QVector3D itemLocalPosition = partLocalPosition + currentPart->anchor3D();
            const QVector3D worldPosition = (model * QVector4D(itemLocalPosition, 1.0f)).toVector3D();

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

bool RenderItem::raycast(const QVector3D& rayOrigin, const QVector3D& rayDirection, RenderItemRayHit& hit) const
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

bool RenderItem::depthWriteEnabled() const
{
    return m_depthWriteEnabled;
}

void RenderItem::setDepthWriteEnabled(bool enabled)
{
    m_depthWriteEnabled = enabled;
}

/// Bounds

void RenderItem::rebuildLocalBoundsCache() const
{
    m_localBoundsCache.reset();

    for (std::size_t i = 0; i < m_parts.size(); ++i)
    {
        const RenderPart* currentPart = m_parts[i];

        if (currentPart == 0 || !currentPart->isStandardModel() || !currentPart->hasLocalBounds())
            continue;

        QMatrix4x4 partTransform;
        partTransform.translate(currentPart->anchor3D());

        const AxisAlignedBoundingBox partBounds = currentPart->localBounds().transformed(partTransform);

        if (partBounds.isValid())
            m_localBoundsCache.expandToInclude(partBounds);
    }
}