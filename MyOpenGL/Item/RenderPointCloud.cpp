#include "RenderPointCloud.h"

#include <QDebug>
#include <QMatrix4x4>
#include <QVector4D>

#include "MyOpenGL/Item/RenderItem.h"
#include "MyOpenGL/Resource/Geometry.h"
#include "MyOpenGL/Render/RenderContext.h"
#include "MyOpenGL/Render/Renderer.h"

RenderPointCloud::RenderPointCloud(RenderPartId id)
    : RenderPart(id)
{
    m_followCamera = true;
    m_pixelScale = false;

    m_depthTestMode = RenderPartStateMode::Enabled;
    m_depthWriteMode = RenderPartStateMode::Enabled;
}

RenderPointCloud::~RenderPointCloud()
{
}

/// Point

int RenderPointCloud::pointCount() const
{
    return m_points.size();
}

const std::vector<QVector3D>& RenderPointCloud::points() const
{
    return m_points;
}

void RenderPointCloud::setPoints(const std::vector<QVector3D>& points)
{
    m_points = points;
}

void RenderPointCloud::clearPoints()
{
    m_points.clear();
}

/// Render

bool RenderPointCloud::draw(Renderer& renderer,
                          const RenderItem& item,
                          const RenderContext& context,
                          const std::vector<const Light*>& lights) const
{
    if (geometry() == 0 || m_points.empty())return true;

    if (!context.isValid())return false;

    std::vector<RenderState> states;
    states.reserve(m_points.size());
    const QVector3D cloudAnchor = anchor3D();
    RenderState baseState;

    //state无法构造时，跳过
    if (!buildRenderState(item, context, anchor3D(),anchor2D(), anchorPixel(), baseState))return true;

    const QMatrix4x4 itemModel =item.transform().matrix();
    const QVector4D baseTranslation = baseState.model.column(3);

    for (std::size_t i = 0; i < m_points.size(); ++i)
    {
        RenderState state = baseState;
        const QVector3D worldOffset =(itemModel * QVector4D(m_points[i], 0.0f)).toVector3D();
        state.model.setColumn(3,baseTranslation + QVector4D(worldOffset, 0.0f));
        states.push_back(state);
    }

    if (!drawStates(renderer,item,lights,states))
    {
        return false;
    }

    return true;
}

bool RenderPointCloud::drawStates(Renderer& renderer,
                            const RenderItem& item,
                            const std::vector<const Light*>& lights,
                            const std::vector<RenderState>& states) const
{
    if (m_geometry == 0 || states.empty())return true;
    //优先使用自带的材质，否则使用Item的材质
    const Material* finalMaterial = m_material;
    if (finalMaterial == 0)finalMaterial = item.material();
    bool drawSucceeded = false;
    /// 非标准模型或非三角形 Geometry 不响应 Item Wireframe DisplayMode。
    if (!isStandardModel() ||geometry()->renderType() != RenderType::Triangles)
    {
        if (finalMaterial == 0)
        {
            qWarning() << "RenderPart drawStates failed:"
                       << "Part requires Material:"
                       << "Item=" << item.name()
                       << "PartId=" << static_cast<qulonglong>(m_id);

            return false;
        }
        drawSucceeded = renderer.drawGeometry(m_geometry,finalMaterial,states,lights);
    }
    else
    {
        switch (item.displayMode())
        {
        case DisplayMode::Shaded:
        {
            if (finalMaterial == 0)
            {
                qWarning() << "RenderPart drawStates failed:"
                           << "shaded Part requires Material:"
                           << "Item=" << item.name()
                           << "PartId=" << static_cast<qulonglong>(m_id);
                return false;
            }

            drawSucceeded = renderer.drawGeometry(m_geometry,finalMaterial,states,lights);
            break;
        }
        case DisplayMode::Wireframe:
        {
            drawSucceeded = renderer.drawWireGeometry(m_geometry,item.edgeColor(),states,false);
            break;
        }
        case DisplayMode::ShadedWithEdges:
        {
            if (finalMaterial == 0)
            {
                qWarning() << "RenderPart drawStates failed:"
                           << "shaded Part requires Material:"
                           << "Item=" << item.name()
                           << "PartId=" << static_cast<qulonglong>(m_id);

                return false;
            }

            drawSucceeded = renderer.drawGeometry(m_geometry,finalMaterial,states,lights);
            if (drawSucceeded)
            {
                drawSucceeded = renderer.drawWireGeometry(m_geometry,item.edgeColor(),states,true);
            }
            break;
        }
        }
    }
    if (!drawSucceeded)
    {
        qWarning() << "RenderPart drawStates failed:"
                   << "Item=" << item.name()
                   << "PartId=" << static_cast<qulonglong>(m_id)
                   << "StateCount=" << static_cast<qulonglong>(states.size())
                   << "Geometry=" << geometry()->name();
        return false;
    }

    return true;
}
