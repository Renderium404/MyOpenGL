#ifndef RENDERPART_H
#define RENDERPART_H

#include "AxisAlignedBoundingBox.h"

#include <QPointF>
#include <QVector2D>
#include <QVector3D>

#include <cstdint>
#include <vector>

class Geometry;
class Light;
class Material;
class RenderItem;
class Renderer;
struct RenderContext;
struct RenderState;

/// RenderPart 唯一标识类型，由 RenderItem 统一分配。
typedef std::uint64_t RenderPartId;

/// 无效 RenderPart ID。
const RenderPartId InvalidRenderPartId = static_cast<RenderPartId>(0);

/// RenderPart 对 RenderItem Depth 状态的覆盖方式。
enum class RenderPartStateMode
{
    Inherit,
    Enabled,
    Disabled
};

/// RenderItem 内具有稳定身份的最小可绘制单位。
/// RenderPart 不拥有其引用的 Geometry 和 Material。
class RenderPart
{
public:
    /// Identity
    /// 返回当前 RenderPart 的稳定 ID。
    RenderPartId id() const;

    /// Render
    /// 使用当前 Part 自己的 Anchor 和显示规则绘制 Geometry。
    virtual bool draw(Renderer& renderer,                               //渲染器
                      const RenderItem& item,                           //item实例
                      const RenderContext& context,                     //渲染上下文
                      const std::vector<const Light*>& lights) const;   //渲染灯光

    /// Geometry
    /// 返回当前 Part 引用的 Geometry；RenderPart 不拥有该对象。
    const Geometry* geometry() const{return m_geometry;}
    /// 设置当前 Part 使用的 Geometry；RenderPart 不接管该对象生命周期。
    void setGeometry(const Geometry* geometry){m_geometry=geometry;}

    /// Material
    /// 返回当前 Part 自己的 Material；为空时绘制阶段使用 RenderItem 默认 Material。
    const Material* material() const{return m_material;}
    /// 设置当前 Part 自己的 Material；RenderPart 不接管该对象生命周期。
    void setMaterial(const Material* material){m_material=material;}

    /// Bounds
    /// 判断当前 Part 是否具有有效 LocalBounds。
    bool hasLocalBounds() const;
    /// 返回当前 Part 的 LocalBounds。
    const AxisAlignedBoundingBox& localBounds() const;
    /// 设置当前 Part 的 LocalBounds；无效 Bounds 会清空当前值。
    void setLocalBounds(const AxisAlignedBoundingBox& bounds);
    /// 清空当前 Part 的 LocalBounds。
    void clearLocalBounds();

    /// Anchor
    /// 返回当前 Part 的 Item Local Space 三维 Anchor。
    const QVector3D& anchor3D() const{return m_anchor3D;}
    /// 设置当前 Part 的 Item Local Space 三维 Anchor。
    void setAnchor3D(const QVector3D& anchor){m_anchor3D=anchor;}
    /// 返回当前 Part 沿 Camera Right / Up 方向的场景空间二维偏移。
    const QVector2D& anchor2D() const{return m_anchor2D;}
    /// 设置当前 Part 沿 Camera Right / Up 方向的场景空间二维偏移。
    void setAnchor2D(const QVector2D& anchor){m_anchor2D=anchor;}
    const QPointF& anchorPixel() const{return m_anchorPixel;}
    void setAnchorPixel(const QPointF& anchor){m_anchorPixel=anchor;}


    /// Display Space
    /// 返回当前 Part 是否始终跟随 Camera 朝向。
    bool followCamera() const{return m_followCamera;}
    /// 设置当前 Part 是否始终跟随 Camera 朝向。
    void setFollowCamera(bool enabled){m_followCamera = enabled;}
    /// 返回当前 Part 是否使用 Pixel 尺度。
    bool pixelScale() const{return m_pixelScale;}
    /// 设置当前 Part 是否使用 Pixel 尺度。
    void setPixelScale(bool enabled){m_pixelScale = enabled;}
    /// 判断当前 Part 是否使用标准 Item Model 绘制。
    bool isStandardModel() const{return !m_followCamera &&!m_pixelScale;}

    /// Depth

    /// 返回当前 Part 对 RenderItem Depth Test 状态的覆盖方式。
    RenderPartStateMode depthTestMode() const{return m_depthTestMode;}
    /// 设置当前 Part 对 RenderItem Depth Test 状态的覆盖方式。
    void setDepthTestMode(RenderPartStateMode mode){ m_depthTestMode = mode;}
    /// 返回当前 Part 对 RenderItem Depth Write 状态的覆盖方式。
    RenderPartStateMode depthWriteMode() const{return m_depthWriteMode;}
    /// 设置当前 Part 对 RenderItem Depth Write 状态的覆盖方式。
    void setDepthWriteMode(RenderPartStateMode mode){ m_depthWriteMode = mode;}
protected:
    friend class RenderItem;

    /// RenderItem 内部接口。
    explicit RenderPart(RenderPartId id);
    virtual ~RenderPart();

    /// 使用当前 Part 自己的参数构造 RenderState。
    bool buildRenderState(const RenderItem& item,
                          const RenderContext& context,
                          RenderState& state) const;

    /// 使用指定 Item Local Space Anchor 构造 RenderState。
    /// 与上一接口相比，只替换本次状态计算使用的三维 Anchor。
    bool buildRenderState(const RenderItem& item,
                          const RenderContext& context,
                          const QVector3D& anchor3D,
                          const QVector2D& anchor2D,
                          const QPointF& anchorPixel,
                          RenderState& state) const;

protected:
    RenderPartId m_id = InvalidRenderPartId; // 当前 Part 的稳定 ID。
    const Geometry* m_geometry = 0; // 当前引用的 Geometry，不拥有。
    const Material* m_material = 0; // 当前 Part Material，不拥有；为空时继承 Item Material。
    AxisAlignedBoundingBox m_localBounds; // 当前 Part 自身 Local Space Bounds。
    QVector3D m_anchor3D = QVector3D(0.0f, 0.0f, 0.0f); // Item Local Space 三维 Anchor。
    QVector2D m_anchor2D = QVector2D(0.0f, 0.0f);       // Camera Right / Up 方向场景空间偏移。
    QPointF m_anchorPixel = QPointF(0.0, 0.0);           // 最终屏幕 Pixel 偏移。
    bool m_followCamera = false; // 是否使用 Camera Basis 作为 Model 方向。
    bool m_pixelScale = false;   // 是否将 Geometry 单位解释为 Pixel。
    RenderPartStateMode m_depthTestMode = RenderPartStateMode::Inherit;  // Depth Test 覆盖方式。
    RenderPartStateMode m_depthWriteMode = RenderPartStateMode::Inherit; // Depth Write 覆盖方式。
};

#endif // RENDERPART_H