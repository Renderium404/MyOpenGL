#ifndef RENDERPART_H
#define RENDERPART_H

#include "AxisAlignedBoundingBox.h"

#include <QVector2D>
#include <QVector3D>

#include <cstdint>

class Geometry;
class Material;
class RenderItem;

/// RenderPart 唯一标识类型，由 RenderItem 统一分配。
typedef std::uint64_t RenderPartId;

/// 无效 RenderPart ID。
const RenderPartId InvalidRenderPartId = static_cast<RenderPartId>(0);

/// RenderPart 对 RenderItem Depth 状态的覆盖方式。
enum class RenderPartStateMode
{
    Inherit,  // 继承 RenderItem 状态。
    Enabled,  // 强制启用。
    Disabled  // 强制关闭。
};

/// RenderItem 内具有稳定身份的最小可绘制单位。
/// RenderPart 不拥有其引用的 Geometry 和 Material。
class RenderPart
{
public:
    /// Identity

    RenderPartId id() const;

    /// Geometry

    const Geometry* geometry() const;
    void setGeometry(const Geometry* geometry);

    /// Material

    const Material* material() const;
    void setMaterial(const Material* material);

    /// Bounds

    bool hasLocalBounds() const;
    const AxisAlignedBoundingBox& localBounds() const;
    void setLocalBounds(const AxisAlignedBoundingBox& bounds);
    void clearLocalBounds();

    /// Anchor

    const QVector3D& anchor3D() const;
    void setAnchor3D(const QVector3D& anchor);

    const QVector2D& anchor2D() const;
    void setAnchor2D(const QVector2D& anchor);

    /// Display Space

    bool followCamera() const;
    void setFollowCamera(bool enabled);

    bool pixelSize() const;
    void setPixelSize(bool enabled);

    /// 判断当前 Part 是否为标准三维模型。
    /// 标准模型不跟随 Camera，并且不使用 Pixel 尺度。
    bool isStandardModel() const;

    /// Depth

    RenderPartStateMode depthTestMode() const;
    void setDepthTestMode(RenderPartStateMode mode);

    RenderPartStateMode depthWriteMode() const;
    void setDepthWriteMode(RenderPartStateMode mode);

protected:
    friend class RenderItem;

    /// RenderItem 内部接口。
    explicit RenderPart(RenderPartId id);
    virtual ~RenderPart();

protected:
    RenderPartId m_id = InvalidRenderPartId;                         // Part 唯一 ID，由 RenderItem 统一分配。

    const Geometry* m_geometry = 0;                                  // Part Geometry，不拥有。
    const Material* m_material = 0;                                  // Part Material Override，不拥有；为空时使用 Item Material。

    AxisAlignedBoundingBox m_localBounds;                            // Geometry Local Space Bounds。

    QVector3D m_anchor3D = QVector3D(0.0f, 0.0f, 0.0f);             // Item Local Space 三维锚点。
    QVector2D m_anchor2D = QVector2D(0.0f, 0.0f);                   // View Plane Scene Space 二维偏移。

    bool m_followCamera = false;                                     // Geometry 局部 XY 是否始终跟随 Camera Viewport 朝向。
    bool m_pixelSize = false;                                        // Geometry XY 尺寸是否按屏幕 Pixel 解释。

    RenderPartStateMode m_depthTestMode = RenderPartStateMode::Inherit;  // Depth Test 对 Item 状态的覆盖方式。
    RenderPartStateMode m_depthWriteMode = RenderPartStateMode::Inherit; // Depth Write 对 Item 状态的覆盖方式。
};

#endif // RENDERPART_H