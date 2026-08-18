#include "RenderItem.h"

#include <QDebug>

#include <algorithm>
#include <set>

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

/// Part 所有权

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
    {
        qWarning() << "RenderItem removePart failed: PartId does not exist:"
                   << "Item=" << m_name
                   << "PartId=" << static_cast<qulonglong>(id);
        return false;
    }

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
    for (std::size_t index = 0; index < m_parts.size(); ++index)
        delete m_parts[index];

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
    {
        qWarning() << "RenderItem partAt failed: index is out of range:"
                   << "Item=" << m_name
                   << "Index=" << index;
        return 0;
    }

    return m_parts[static_cast<std::size_t>(index)];
}

const RenderPart* RenderItem::partAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_parts.size()))
    {
        qWarning() << "RenderItem partAt failed: index is out of range:"
                   << "Item=" << m_name
                   << "Index=" << index;
        return 0;
    }

    return m_parts[static_cast<std::size_t>(index)];
}

RenderPart* RenderItem::part(RenderPartId id)
{
    std::map<RenderPartId, RenderPart*>::iterator iterator = m_partsById.find(id);
    return iterator == m_partsById.end() ? 0 : iterator->second;
}

const RenderPart* RenderItem::part(RenderPartId id) const
{
    std::map<RenderPartId, RenderPart*>::const_iterator iterator = m_partsById.find(id);
    return iterator == m_partsById.end() ? 0 : iterator->second;
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

    // 批量提交先验证全部命令和 PartId 唯一性，避免输入错误导致 Item 只应用一半状态。
    std::set<RenderPartId> updatedPartIds;

    for (std::size_t index = 0; index < updates.size(); ++index)
    {
        const RenderPartUpdate& update = updates[index];

        if (!update.isValid())
        {
            qWarning() << "RenderItem applyPartUpdates failed: invalid RenderPartUpdate:"
                       << "Item=" << m_name
                       << "PartId=" << static_cast<qulonglong>(update.partId)
                       << "Operation=" << static_cast<int>(update.operation);
            return false;
        }

        if (!updatedPartIds.insert(update.partId).second)
        {
            qWarning() << "RenderItem applyPartUpdates failed: duplicate PartId in one batch:"
                       << "Item=" << m_name
                       << "PartId=" << static_cast<qulonglong>(update.partId);
            return false;
        }
    }

    for (std::size_t index = 0; index < updates.size(); ++index)
    {
        const RenderPartUpdate& update = updates[index];

        if (update.operation == RenderPartUpdateRemove)
        {
            // Update 层的 Remove 是状态命令；目标 Part 已不存在时已经达到期望状态，因此不产生 Warning。
            if (part(update.partId) != 0)
            {
                if (!removePart(update.partId))
                    return false;
            }

            continue;
        }

        RenderPart* targetPart = part(update.partId);

        if (targetPart == 0)
        {
            targetPart = createPart(update.partId);

            if (targetPart == 0)
                return false;
        }

        // Geometry Replace 只接受最小基础数据。
        // 旧 Picker / Bounds 与新 Geometry 可能不再匹配，因此在替换时主动清除；需要这些能力的调用方可随后按需重新绑定。
        targetPart->setGeometry(update.geometry);
        targetPart->setPrimitivePickSource(0);
        targetPart->clearLocalBounds();
    }

    return true;
}

/// 旧单 Geometry 兼容接口

const Geometry* RenderItem::geometry() const
{
    const RenderPart* defaultPart = part(DefaultRenderPartId);
    return defaultPart != 0 ? defaultPart->geometry() : 0;
}

void RenderItem::setGeometry(const Geometry* geometry)
{
    RenderPart* defaultPart = ensureDefaultPart();

    if (defaultPart != 0)
        defaultPart->setGeometry(geometry);
}

const PrimitivePickSource* RenderItem::primitivePickSource() const
{
    const RenderPart* defaultPart = part(DefaultRenderPartId);
    return defaultPart != 0 ? defaultPart->primitivePickSource() : 0;
}

void RenderItem::setPrimitivePickSource(const PrimitivePickSource* source)
{
    RenderPart* defaultPart = ensureDefaultPart();

    if (defaultPart != 0)
        defaultPart->setPrimitivePickSource(source);
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

void RenderItem::setLocalBounds(const AxisAlignedBoundingBox& bounds)
{
    RenderPart* defaultPart = ensureDefaultPart();

    if (defaultPart != 0)
        defaultPart->setLocalBounds(bounds);
}

void RenderItem::clearLocalBounds()
{
    RenderPart* defaultPart = part(DefaultRenderPartId);

    if (defaultPart != 0)
        defaultPart->clearLocalBounds();
}

AxisAlignedBoundingBox RenderItem::worldBounds() const
{
    rebuildLocalBoundsCache();

    if (!m_localBoundsCache.isValid())
        return AxisAlignedBoundingBox();

    return m_localBoundsCache.transformed(m_transform.matrix());
}

/// 显示状态

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

RenderPart* RenderItem::ensureDefaultPart()
{
    RenderPart* result = part(DefaultRenderPartId);

    if (result != 0)
        return result;

    result = createPart(DefaultRenderPartId);

    // Default Part 创建不可能与自身重复；若内部状态损坏，后续旧接口无法安全继续。
    if (result == 0)
        qWarning() << "RenderItem ensureDefaultPart failed:" << m_name;

    return result;
}

void RenderItem::rebuildLocalBoundsCache() const
{
    m_localBoundsCache.reset();

    for (std::size_t index = 0; index < m_parts.size(); ++index)
    {
        const RenderPart* currentPart = m_parts[index];

        if (currentPart != 0 && currentPart->hasLocalBounds())
            m_localBoundsCache.expandToInclude(currentPart->localBounds());
    }
}
