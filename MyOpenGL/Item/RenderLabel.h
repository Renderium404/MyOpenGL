#ifndef RENDERLABEL_H
#define RENDERLABEL_H

#include <QPointF>
#include <QVector2D>
#include <QVector3D>

#include <cstdint>

class Geometry;
class Material;
class RenderItem;

/// RenderLabel 唯一标识类型，由 RenderItem 统一分配。
typedef std::uint64_t RenderLabelId;

/// 无效 RenderLabel ID。
const RenderLabelId InvalidRenderLabelId = static_cast<RenderLabelId>(0);

/// 跟随屏幕运动的可渲染对象，用于 Item 的辅助信息显示。
///
/// anchorWorld：世界空间基准锚点。
/// anchorSence：基于当前屏幕二维标尺的坐标，单位为网格尺度。
/// pixelOffset：投影完成后的最终 Pixel 微调。
class RenderLabel
{
public:
    /// Identity
    RenderLabelId id() const
    {
        return m_id;
    }

    /// World Anchor
    const QVector3D& anchorWorld() const;
    void setAnchorWorld(const QVector3D& anchor);

    const QVector2D& anchorSence() const;
    void setAnchorSence(const QVector2D& anchor);

    const QPointF& pixelOffset() const;
    void setPixelOffset(const QPointF& offset);

    /// Geometry
    const Geometry* geometry() const;
    void setGeometry(const Geometry* geometry);

    /// Material
    const Material* material() const;
    void setMaterial(const Material* material);

    /// Display
    bool isVisible() const;
    void setVisible(bool visible);

    /// State
    bool isRenderable() const;

private:
    friend class RenderItem;

    explicit RenderLabel(RenderLabelId id);
    ~RenderLabel();

private:
    RenderLabelId m_id;
    QVector3D m_anchorWorld;          // 世界锚点。
    QVector2D m_anchorScence;         // 二维标尺坐标，单位为网格尺度。
    QPointF m_pixelOffset;            // 最终屏幕 Pixel 偏移。
    const Geometry* m_geometry;       // 几何数据，不拥有。
    const Material* m_material;       // 材质，不拥有。
    bool m_visible;                   // 是否参与绘制。
};

#endif // RENDERLABEL_H