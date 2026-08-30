#ifndef RENDERLABEL_H
#define RENDERLABEL_H

#include "RenderPart.h"

#include <QPointF>

/// RenderLabel ID 与 RenderPart 共用同一个 ID 空间。
typedef RenderPartId RenderLabelId;

/// 无效 RenderLabel ID。
const RenderLabelId InvalidRenderLabelId = InvalidRenderPartId;

/// 专门用于文本显示的 RenderPart。
class RenderLabel : public RenderPart
{
public:
    /// Render
    bool draw(Renderer& renderer,const RenderItem& item,const RenderContext& context,const std::vector<const Light*>& lights) const override;
protected:
    friend class RenderItem;
    /// RenderItem 内部接口。
    explicit RenderLabel(RenderLabelId id);
    ~RenderLabel() override;
};

#endif // RENDERLABEL_H
