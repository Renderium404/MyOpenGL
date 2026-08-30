#ifndef RENDERPOINTCLOUD_H
#define RENDERPOINTCLOUD_H

#include "RenderPart.h"
#include <QVector3D>
#include <QVector>
#include <vector>

class RenderPointCloud : public RenderPart
{
public:
    /// Point

    /// 返回当前 Point 数量。
    int pointCount() const;
    /// 返回当前全部 Item Local Space Point。
    const std::vector<QVector3D>& points() const;
    /// 整体替换当前 Point 集合；空集合是合法状态。
    void setPoints(const std::vector<QVector3D>& points);
    /// 清空全部 Point。
    void clearPoints();
    /// Render
    bool draw(Renderer& renderer,
                const RenderItem& item,
                const RenderContext& context,
                const std::vector<const Light*>& lights) const override;

protected:
    friend class RenderItem;

    /// RenderItem 内部接口。
    explicit RenderPointCloud(RenderPartId id);
    ~RenderPointCloud() override;
    bool drawStates(Renderer& renderer,
                    const RenderItem& item,
                    const std::vector<const Light*>& lights,
                    const std::vector<RenderState>& states) const;
private:
    std::vector<QVector3D> m_points; // 相对于 RenderPointCloud Anchor 的 Item Local Space Point。
};

#endif // RENDERPOINTCLOUD_H