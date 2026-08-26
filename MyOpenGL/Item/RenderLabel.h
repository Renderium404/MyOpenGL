#ifndef RENDERLABEL_H
#define RENDERLABEL_H

#include <QPointF>
#include <QString>
#include <QVector3D>

#include <cstdint>

class Geometry;
class Material;
class RenderItem;

/// RenderLabel 唯一标识类型，由 RenderItem 统一分配。
typedef std::uint64_t RenderLabelId;

/// 无效 RenderLabel ID。
const RenderLabelId InvalidRenderLabelId = static_cast<RenderLabelId>(0);

/// RenderItem 持有的持久化屏幕标注。
///
/// anchorPosition 位于 Item Local Space。
/// Viewer 绘制时按照：
///
/// Item Local Anchor
///     -> Item Transform
///     -> World
///     -> View / Projection
///     -> Screen Pixel
///
/// Geometry 使用屏幕 Pixel 坐标描述 Label Quad。
///
/// RenderLabel 不拥有 Geometry 和 Material，
/// 它们的生命周期分别由 ResourceManager 和 MaterialManager 管理。
class RenderLabel
{
public:
    /// Identity
    RenderLabelId id() const
    {
        return m_id;
    }

    /// Text
    const QString& text() const;
    void setText(const QString& text);

    /// Anchor
    ///
    /// Label 锚点位于 Item Local Space。
    const QVector3D& anchorPosition() const;
    void setAnchorPosition(const QVector3D& position);

    /// Screen Offset
    ///
    /// 使用 Qt 风格 Pixel Offset：
    /// +X 向右。
    /// +Y 向下。
    ///
    /// Viewer 转换到 OpenGL Pixel 坐标时负责翻转 Y。
    const QPointF& pixelOffset() const;
    void setPixelOffset(const QPointF& offset);

    /// Geometry
    ///
    /// Geometry 使用屏幕 Pixel 坐标。
    /// RenderLabel 不拥有该对象。
    const Geometry* geometry() const;
    void setGeometry(const Geometry* geometry);

    /// Material
    ///
    /// 通常为 SurfaceMode::Texture 的无光照 Material。
    /// RenderLabel 不拥有该对象。
    const Material* material() const;
    void setMaterial(const Material* material);

    /// Display
    bool isVisible() const;
    void setVisible(bool visible);

    /// State
    ///
    /// 判断当前 Label 是否具有完整的绘制资源。
    bool isRenderable() const;

private:
    friend class RenderItem;

    /// RenderItem 内部接口。
    explicit RenderLabel(RenderLabelId id);
    ~RenderLabel();

private:
    RenderLabelId m_id;

    QString m_text;                   // Label 语义文本。
    QVector3D m_anchorPosition;       // Item Local Space 锚点。
    QPointF m_pixelOffset;            // 屏幕 Pixel 偏移。

    const Geometry* m_geometry;       // 屏幕空间 Quad Geometry，不拥有。
    const Material* m_material;       // Label Material，不拥有。

    bool m_visible;                   // 是否参与绘制。
};

#endif // RENDERLABEL_H