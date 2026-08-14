#include "RenderItem.h"

RenderItem::RenderItem(const QString& name)
    : m_name(name)
    , m_mesh(0)
    , m_material(0)
    , m_primitivePickSource(0)
    , m_visible(true)
    , m_depthTestEnabled(true)
{
}

/// 基本信息

const QString& RenderItem::name() const
{
    return m_name;
}

/// 绘制引用

const RenderableMesh* RenderItem::mesh() const
{
    return m_mesh;
}

const Material* RenderItem::material() const
{
    return m_material;
}

void RenderItem::setMesh(const RenderableMesh* mesh)
{
    m_mesh = mesh;
}

void RenderItem::setMaterial(const Material* material)
{
    m_material = material;
}

/// Primitive Picking

const PrimitivePickSource* RenderItem::primitivePickSource() const
{
    return m_primitivePickSource;
}

void RenderItem::setPrimitivePickSource(const PrimitivePickSource* source)
{
    m_primitivePickSource = source;
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
    return m_localBounds.isValid();
}

const AxisAlignedBoundingBox& RenderItem::localBounds() const
{
    return m_localBounds;
}

void RenderItem::setLocalBounds(const AxisAlignedBoundingBox& bounds)
{
    if (!bounds.isValid())
    {
        m_localBounds.reset();
        return;
    }

    m_localBounds = bounds;
}

void RenderItem::clearLocalBounds()
{
    m_localBounds.reset();
}

AxisAlignedBoundingBox RenderItem::worldBounds() const
{
    if (!m_localBounds.isValid())
        return AxisAlignedBoundingBox();

    return m_localBounds.transformed(m_transform.matrix());
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

bool RenderItem::depthTestEnabled() const
{
    return m_depthTestEnabled;
}

void RenderItem::setDepthTestEnabled(bool enabled)
{
    m_depthTestEnabled = enabled;
}