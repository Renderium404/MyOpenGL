#include "RenderLabel.h"

#include <QDebug>

#include "MyOpenGL/Item/RenderItem.h"
#include "MyOpenGL/Material/Material.h"
#include "MyOpenGL/Render/RenderContext.h"
#include "MyOpenGL/Render/Renderer.h"
#include "MyOpenGL/Resource/Geometry.h"

RenderLabel::RenderLabel(RenderLabelId id)
    : RenderPart(id)
{
    m_followCamera = true;
    m_pixelScale = true;

    m_depthTestMode = RenderPartStateMode::Disabled;
    m_depthWriteMode = RenderPartStateMode::Disabled;
}

RenderLabel::~RenderLabel()
{
}

/// Render

bool RenderLabel::draw(Renderer& renderer,
                       const RenderItem& item,
                       const RenderContext& context,
                       const std::vector<const Light*>& lights) const
{
    Q_UNUSED(lights);

    if (m_geometry == 0)return true;

    if (!context.isValid())return false;

    const Material* finalMaterial = m_material;
    if (finalMaterial == 0)finalMaterial = item.material();

    if (finalMaterial == 0)
    {
        qWarning() << "RenderLabel draw failed: Label requires Material:"
                   << "Item=" << item.name()
                   << "LabelId=" << static_cast<qulonglong>(m_id);
        return false;
    }
    RenderState state;

    /// Label 位于 Camera 后方等情况下，本帧直接跳过。
    if (!buildRenderState(item, context, anchor3D(),anchor2D(), anchorPixel(), state))return true;
    /// RenderLabel 专门用于带 Alpha 的屏幕文本显示。
    state.blendEnabled = true;
    /// Label 的绘制规则由 Label 自己决定：不参与 Scene Lighting。
    const std::vector<const Light*> noLights;
    if (!renderer.drawGeometry(m_geometry, finalMaterial, state, noLights))
    {
        return false;
    }
    return true;
}


