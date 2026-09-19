#include "ItemManager.h"

#include <QColor>
#include <QDebug>
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>

#include <algorithm>

#include "MyOpenGL/Core/ResourceManager.h"
#include "MyOpenGL/Material/Material.h"
#include "MyOpenGL/Material/MaterialManager.h"
#include "MyOpenGL/Resource/BufferGeometry.h"
#include "MyOpenGL/Resource/Texture.h"
#include "RenderPointCloud.h"

ItemManager::PartRecord::PartRecord()
    : part(0)
    , kind(PartKind::Standard)
{
}

ItemManager::ItemManager()
    : m_nextItemId(1)
    , m_nextPartId(1)
{
}

ItemManager::~ItemManager()
{
    clear();
}

/// Item 管理

RenderItem* ItemManager::createItem(const QString& name)
{
    const RenderItemId id = allocateItemId();

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
    ItemMap::iterator iterator = m_itemsById.find(id);
    return iterator != m_itemsById.end() ? iterator->second : 0;
}

const RenderItem* ItemManager::get(RenderItemId id) const
{
    ItemMap::const_iterator iterator = m_itemsById.find(id);
    return iterator != m_itemsById.end() ? iterator->second : 0;
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
    for (std::size_t index = 0; index < m_items.size(); ++index)
    {
        RenderItem* item = m_items[index];

        if (item == 0)
            continue;

        item->setId(InvalidRenderItemId);
        delete item;
    }

    m_items.clear();
    m_itemsById.clear();

    PartMap::iterator partIterator = m_partsById.begin();

    while (partIterator != m_partsById.end())
    {
        delete partIterator->second.part;
        ++partIterator;
    }

    m_partsById.clear();
    m_nextItemId = 1;
    m_nextPartId = 1;
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

/// Part 生命周期

RenderPart* ItemManager::createPart()
{
    const RenderPartId id = allocatePartId();

    if (id == InvalidRenderPartId)
        return 0;

    RenderPart* part = new RenderPart(id);

    if (!registerPart(part, PartKind::Standard))
    {
        delete part;
        return 0;
    }

    return part;
}

RenderLabel* ItemManager::createLabel()
{
    const RenderPartId id = allocatePartId();

    if (id == InvalidRenderPartId)
        return 0;

    RenderLabel* label = new RenderLabel(id);

    if (!registerPart(label, PartKind::Label))
    {
        delete label;
        return 0;
    }

    return label;
}

RenderLabel* ItemManager::createTextLabel(ResourceManager& resourceManager, MaterialManager& materialManager,
                                          const QString& text, int textPixelSize)
{
    if (text.isEmpty() || textPixelSize <= 0)
        return 0;

    RenderLabel* label = createLabel();

    if (label == 0)
        return 0;

    const RenderLabelId labelId = label->id();

    QFont font;
    font.setPixelSize(textPixelSize);
    const QFontMetrics metrics(font);

    const int horizontalPadding = 4;
    const int verticalPadding = 2;
    const int textWidth = metrics.width(text);
    const int textHeight = metrics.height();
    const int imageWidth = textWidth + horizontalPadding * 2;
    const int imageHeight = textHeight + verticalPadding * 2;

    if (imageWidth <= 0 || imageHeight <= 0)
    {
        removePart(labelId);
        return 0;
    }

    QImage image(imageWidth, imageHeight, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.fillRect(image.rect(), QColor(0, 0, 0, 160));
    painter.setFont(font);
    painter.setPen(QColor(255, 230, 120));
    painter.drawText(QRect(horizontalPadding, verticalPadding, textWidth, textHeight), Qt::AlignLeft | Qt::AlignVCenter, text);
    painter.end();

    const QString suffix = QStringLiteral("%1").arg(static_cast<qulonglong>(labelId));
    Texture* texture = new Texture(QStringLiteral("RenderLabelTextTexture_%1").arg(suffix));

    if (!texture->setImage(image))
    {
        delete texture;
        removePart(labelId);
        return 0;
    }

    if (resourceManager.adopt(texture) == InvalidResourceId)
    {
        delete texture;
        removePart(labelId);
        return 0;
    }

    BufferGeometry* geometry = new BufferGeometry(QStringLiteral("RenderLabelTextGeometry_%1").arg(suffix), BufferUsage::Static, RenderType::Triangles);
    std::vector<GeometryVertexAttribute> attributes;

    GeometryVertexAttribute position;
    position.location = GeometryAttribute::Position;
    position.componentCount = 3;
    position.valueOffset = 0;
    attributes.push_back(position);

    GeometryVertexAttribute texCoord;
    texCoord.location = GeometryAttribute::TexCoord;
    texCoord.componentCount = 2;
    texCoord.valueOffset = 3;
    attributes.push_back(texCoord);

    geometry->setVertexLayout(5, attributes);

    const float width = static_cast<float>(imageWidth);
    const float height = static_cast<float>(imageHeight);
    const std::vector<GLfloat> vertices =
    {
        0.0f,  0.0f,   0.0f, 0.0f, 1.0f,
        width, 0.0f,   0.0f, 1.0f, 1.0f,
        width, height, 0.0f, 1.0f, 0.0f,
        0.0f,  height, 0.0f, 0.0f, 0.0f
    };
    const std::vector<GLuint> indices = {0, 1, 2, 0, 2, 3};

    geometry->setVertexData(vertices);
    geometry->setIndexData(indices);

    if (resourceManager.adopt(geometry) == InvalidResourceId)
    {
        delete geometry;
        resourceManager.remove(texture->id());
        removePart(labelId);
        return 0;
    }

    Material* material = materialManager.createMaterial(QStringLiteral("RenderLabelTextMaterial_%1").arg(suffix));

    if (material == 0)
    {
        resourceManager.remove(geometry->id());
        resourceManager.remove(texture->id());
        removePart(labelId);
        return 0;
    }

    if (!material->setSurfaceMode(SurfaceMode::Texture))
    {
        materialManager.remove(material->id());
        resourceManager.remove(geometry->id());
        resourceManager.remove(texture->id());
        removePart(labelId);
        return 0;
    }

    material->setLightingEnabled(false);
    material->setTexture(texture);
    material->setColor(QVector4D(1.0f, 1.0f, 1.0f, 1.0f));

    label->setGeometry(geometry);
    label->setMaterial(material);
    return label;
}

RenderPointCloud* ItemManager::createRenderPointCloud()
{
    const RenderPartId id = allocatePartId();

    if (id == InvalidRenderPartId)
        return 0;

    RenderPointCloud* pointCloud = new RenderPointCloud(id);

    if (!registerPart(pointCloud, PartKind::Standard))
    {
        delete pointCloud;
        return 0;
    }

    return pointCloud;
}

RenderPart* ItemManager::getPart(RenderPartId id)
{
    PartMap::iterator iterator = m_partsById.find(id);
    return iterator != m_partsById.end() ? iterator->second.part : 0;
}

const RenderPart* ItemManager::getPart(RenderPartId id) const
{
    PartMap::const_iterator iterator = m_partsById.find(id);
    return iterator != m_partsById.end() ? iterator->second.part : 0;
}

bool ItemManager::containsPart(RenderPartId id) const
{
    return m_partsById.find(id) != m_partsById.end();
}

std::size_t ItemManager::partCount() const
{
    return m_partsById.size();
}

bool ItemManager::removePart(RenderPartId id)
{
    PartMap::iterator iterator = m_partsById.find(id);

    if (iterator == m_partsById.end())
        return false;

    PartRecord& record = iterator->second;

    for (std::size_t index = 0; index < m_items.size(); ++index)
    {
        RenderItem* item = m_items[index];

        if (item == 0)
            continue;

        if (record.kind == PartKind::Label)
        {
            if (item->containsLabel(id) && !item->removeLabel(id))
                return false;
        }
        else
        {
            if (item->containsPart(id) && !item->removePart(id))
                return false;
        }
    }

    delete record.part;
    m_partsById.erase(iterator);
    return true;
}

std::size_t ItemManager::partItemCount(RenderPartId id) const
{
    PartMap::const_iterator iterator = m_partsById.find(id);

    if (iterator == m_partsById.end())
        return 0;

    std::size_t result = 0;

    for (std::size_t index = 0; index < m_items.size(); ++index)
    {
        const RenderItem* item = m_items[index];

        if (item == 0)
            continue;

        if (iterator->second.kind == PartKind::Label)
        {
            if (item->containsLabel(id))
                ++result;
        }
        else
        {
            if (item->containsPart(id))
                ++result;
        }
    }

    return result;
}

/// Item Bounds

bool ItemManager::worldBounds(AxisAlignedBoundingBox& bounds, bool visibleOnly) const
{
    bounds.reset();

    for (std::size_t index = 0; index < m_items.size(); ++index)
    {
        const RenderItem* currentItem = m_items[index];

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

/// 内部 Part 注册

bool ItemManager::registerPart(RenderPart* part, PartKind kind)
{
    if (part == 0 || part->id() == InvalidRenderPartId || containsPart(part->id()))
        return false;

    PartRecord record;
    record.part = part;
    record.kind = kind;
    m_partsById[part->id()] = record;
    return true;
}

/// ID 分配

RenderItemId ItemManager::allocateItemId()
{
    while (m_nextItemId == InvalidRenderItemId || contains(m_nextItemId))
        ++m_nextItemId;

    const RenderItemId id = m_nextItemId;
    ++m_nextItemId;
    return id;
}

RenderPartId ItemManager::allocatePartId()
{
    while (m_nextPartId == InvalidRenderPartId || containsPart(m_nextPartId))
        ++m_nextPartId;

    const RenderPartId id = m_nextPartId;
    ++m_nextPartId;
    return id;
}
