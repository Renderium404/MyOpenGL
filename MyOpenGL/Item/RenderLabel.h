#ifndef RENDERLABEL_H
#define RENDERLABEL_H

#include "RenderPart.h"

#include <QPointF>

/// RenderLabel ID 与 RenderPart 共用同一个 ID 空间。
typedef RenderPartId RenderLabelId;

/// 无效 RenderLabel ID。
const RenderLabelId InvalidRenderLabelId = InvalidRenderPartId;

/// 专门用于文本显示的 RenderPart。
/// 默认跟随 Camera 朝向，并使用屏幕 Pixel 尺度。
/// 相比 RenderPart，额外提供最终屏幕 Pixel 偏移。
class RenderLabel : public RenderPart
{
public:
    /// Pixel Offset
    const QPointF& pixelOffset() const;
    void setPixelOffset(const QPointF& offset);

protected:
    friend class RenderItem;

    /// RenderItem 内部接口。
    explicit RenderLabel(RenderLabelId id);
    ~RenderLabel() override;

private:
    QPointF m_pixelOffset = QPointF(0.0, 0.0); // 最终屏幕 Pixel 偏移。
};

#endif // RENDERLABEL_H