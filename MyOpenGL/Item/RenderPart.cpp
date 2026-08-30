#include "RenderPart.h"

#include <QDebug>
#include <QMatrix4x4>
#include <QVector4D>
#include <QtMath>

#include "MyOpenGL/Item/RenderItem.h"
#include "MyOpenGL/Material/Material.h"
#include "MyOpenGL/Render/RenderContext.h"
#include "MyOpenGL/Render/Renderer.h"
#include "MyOpenGL/Resource/Geometry.h"

namespace
{

bool resolvePartState(bool itemState, RenderPartStateMode mode)
{
    switch (mode)
    {
    case RenderPartStateMode::Inherit:
        return itemState;

    case RenderPartStateMode::Enabled:
        return true;

    case RenderPartStateMode::Disabled:
        return false;
    }

    qWarning() << "RenderPart resolvePartState failed:"
               << "unsupported state mode:"
               << static_cast<int>(mode);

    return itemState;
}

//获取1像素在锚点处对应的世界单位
bool calculateWorldUnitsPerPixel(const RenderContext& context,
                                 const QVector3D& worldAnchor,
                                 const QVector3D& cameraRight,
                                 const QVector3D& cameraUp,
                                 float& worldPerPixelX,
                                 float& worldPerPixelY)
{
    const QVector4D anchorClip =context.projection *context.view *QVector4D(worldAnchor, 1.0f);//获取锚点在裁剪空间的位置
    const QVector4D xClip =context.projection *context.view *QVector4D(worldAnchor + cameraRight, 1.0f);//获取锚点向相机X方向平移一单位后的点
    const QVector4D yClip =context.projection *context.view *QVector4D(worldAnchor + cameraUp, 1.0f);//获取锚点向相机Y方向平移一单位后的点

    if (anchorClip.w() <= 1.0e-8f ||xClip.w() <= 1.0e-8f ||yClip.w() <= 1.0e-8f)
    {
        return false;
    }
    //获取锚点在屏幕的坐标
    const float anchorPixelX =(anchorClip.x() / anchorClip.w() * 0.5f + 0.5f) *static_cast<float>(context.viewportWidth);
    const float anchorPixelY =(anchorClip.y() / anchorClip.w() * 0.5f + 0.5f) *static_cast<float>(context.viewportHeight);
    //获取锚点向相机X方向平移一单位后再屏幕的坐标
    const float xPixelX =(xClip.x() / xClip.w() * 0.5f + 0.5f) *static_cast<float>(context.viewportWidth);
    const float xPixelY =(xClip.y() / xClip.w() * 0.5f + 0.5f) *static_cast<float>(context.viewportHeight);
    //获取锚点向相机Y方向平移一单位后再屏幕的坐标
    const float yPixelX =(yClip.x() / yClip.w() * 0.5f + 0.5f) *static_cast<float>(context.viewportWidth);
    const float yPixelY =(yClip.y() / yClip.w() * 0.5f + 0.5f) *static_cast<float>(context.viewportHeight);
    //获取X，Y轴方向1世界单位对应的像素偏移向量
    const QVector2D xPixelVector(xPixelX - anchorPixelX,xPixelY - anchorPixelY);
    const QVector2D yPixelVector(yPixelX - anchorPixelX,yPixelY - anchorPixelY);
    //获取X，Y轴方向1世界单位对应的像素数量
    const float pixelsPerWorldX = xPixelVector.length();
    const float pixelsPerWorldY = yPixelVector.length();

    if (pixelsPerWorldX <= 1.0e-8f ||pixelsPerWorldY <= 1.0e-8f)
    {
        return false;
    }
    //取倒数，获取1像素对应的世界单位
    worldPerPixelX = 1.0f / pixelsPerWorldX;
    worldPerPixelY = 1.0f / pixelsPerWorldY;

    return true;
}

}

RenderPart::RenderPart(RenderPartId id)
    : m_id(id)
{
}

RenderPart::~RenderPart()
{
}

/// Identity

RenderPartId RenderPart::id() const
{
    return m_id;
}

/// Render

bool RenderPart::draw(Renderer& renderer,
                      const RenderItem& item,
                      const RenderContext& context,
                      const std::vector<const Light*>& lights) const
{

    if (m_geometry == 0)return true;
    if (!context.isValid())return false;
    RenderState state;
    if (!buildRenderState(item, context, m_anchor3D,m_anchor2D, m_anchorPixel, state))
    {
        return true;
    }
    const Material* finalMaterial = m_material;
    if (finalMaterial == 0)finalMaterial = item.material();
    bool drawSucceeded = false;
    if (!isStandardModel() ||m_geometry->renderType() != RenderType::Triangles)
    {
        if (finalMaterial == 0)return false;
        drawSucceeded = renderer.drawGeometry(m_geometry,finalMaterial,state,lights);
    }
    else
    {
        switch (item.displayMode())
        {
        case DisplayMode::Shaded:
        {
            if (finalMaterial == 0)return false;
            drawSucceeded = renderer.drawGeometry(m_geometry,finalMaterial,state,lights);
            break;
        }
        case DisplayMode::Wireframe:
        {
            drawSucceeded = renderer.drawWireGeometry(m_geometry,item.edgeColor(),state,false);
            break;
        }
        case DisplayMode::ShadedWithEdges:
        {
            if (finalMaterial == 0)return false;
            drawSucceeded = renderer.drawGeometry(m_geometry,finalMaterial,state,lights);
            drawSucceeded = drawSucceeded&&renderer.drawWireGeometry(m_geometry,item.edgeColor(),state,true);
            break;
        }
        }
    }
    if (!drawSucceeded)
    {
        qWarning() << "RenderPart draw failed:"
                << "Item=" << item.name()
                << "PartId=" << static_cast<qulonglong>(m_id)
                << "Geometry=" << m_geometry->name();

        return false;
    }

    return true;
}


/// RenderState
bool RenderPart::buildRenderState(const RenderItem& item,
                                  const RenderContext& context,
                                  RenderState& state) const
{
    return buildRenderState(
        item,
        context,
        m_anchor3D,
        m_anchor2D,
        m_anchorPixel,
        state);
}

bool RenderPart::buildRenderState(const RenderItem& item,
                                  const RenderContext& context,
                                  const QVector3D& anchor3D,
                                  const QVector2D& anchor2D,
                                  const QPointF& anchorPixel,
                                  RenderState& state) const
{
    if (!context.isValid())return false;
    const QMatrix4x4 itemModel = item.transform().matrix();
    const QVector3D cameraForward =context.cameraForward.normalized();
    const QVector3D cameraUp =context.cameraUp.normalized();
    const QVector3D sceneRight =QVector3D::crossProduct(cameraForward,cameraUp).normalized();

    if (sceneRight.lengthSquared() <= 1.0e-12f)return false;

    const QVector3D sceneUp =QVector3D::crossProduct(sceneRight,cameraForward).normalized();
    const QVector3D sceneBack =QVector3D::crossProduct(sceneRight,sceneUp).normalized();

    /// 叠加三维锚点
    QVector3D worldAnchor =(itemModel *QVector4D(anchor3D, 1.0f)).toVector3D();

    /// 叠加二维锚点
    worldAnchor += sceneRight * anchor2D.x();
    worldAnchor += sceneUp * anchor2D.y();

    //叠加像素锚点
    float worldPerPixelX = 0.0f;
    float worldPerPixelY = 0.0f;
    const bool hasPixelOffset =qAbs(anchorPixel.x()) > 1.0e-8 ||qAbs(anchorPixel.y()) > 1.0e-8;
    const bool needPixelScale =m_pixelScale ||hasPixelOffset;
    if (needPixelScale)
    {
        if (!calculateWorldUnitsPerPixel(context,worldAnchor,sceneRight,sceneUp,worldPerPixelX,worldPerPixelY))
        {
            return false;
        }
    }

    if (hasPixelOffset)
    {
        worldAnchor +=sceneRight *static_cast<float>(anchorPixel.x()) *worldPerPixelX;
        worldAnchor -=sceneUp *static_cast<float>(anchorPixel.y()) *worldPerPixelY;
    }

    state = RenderState();
    state.view = context.view;
    state.projection = context.projection;
    state.viewport = RenderViewport(0,0,context.viewportWidth,context.viewportHeight);

    state.depthTestEnabled =resolvePartState(item.depthTestEnabled(),m_depthTestMode);
    state.depthWriteEnabled =resolvePartState(item.depthWriteEnabled(),m_depthWriteMode);
    state.blendEnabled = false;

    /// 标准模型。
    if (!m_followCamera &&!m_pixelScale)
    {
        state.model = itemModel;
        state.model.setColumn(3,QVector4D(worldAnchor,1.0f));
        return true;
    }
    const QVector3D itemXAxis =itemModel.column(0).toVector3D();
    const QVector3D itemYAxis =itemModel.column(1).toVector3D();
    const QVector3D itemZAxis =itemModel.column(2).toVector3D();

    const float scaleX = itemXAxis.length();
    const float scaleY = itemYAxis.length();
    const float scaleZ = itemZAxis.length();

    if (scaleX <= 1.0e-8f ||scaleY <= 1.0e-8f ||scaleZ <= 1.0e-8f)
    {
        qWarning() << "RenderPart buildRenderState failed:"
                   << "Item transform contains zero scale:"
                   << "Item=" << item.name()
                   << "PartId=" << static_cast<qulonglong>(m_id);
        return false;
    }

    QVector3D axisX;
    QVector3D axisY;
    QVector3D axisZ;

    if (m_followCamera)
    {
        axisX = sceneRight;
        axisY = sceneUp;
        axisZ = sceneBack;
    }
    else
    {
        axisX = itemXAxis / scaleX;
        axisY = itemYAxis / scaleY;
        axisZ = itemZAxis / scaleZ;
    }
    float baseScale = 1.0f;
    if (m_pixelScale)
    {
        /// X / Y Pixel 对应的 World Scale 取平均值，
        /// 避免 Viewport 非等比例情况下产生明显单轴偏差。
        baseScale =(worldPerPixelX +worldPerPixelY) *0.5f;
    }
    const QVector3D modelXAxis =axisX *scaleX *baseScale;
    const QVector3D modelYAxis =axisY *scaleY *baseScale;
    const QVector3D modelZAxis =axisZ *scaleZ *baseScale;

    state.model.setToIdentity();
    state.model.setColumn(0,QVector4D(modelXAxis,0.0f));
    state.model.setColumn(1,QVector4D(modelYAxis,0.0f));
    state.model.setColumn(2,QVector4D(modelZAxis,0.0f));
    state.model.setColumn(3,QVector4D(worldAnchor,1.0f));
    return true;
}
/// Bounds

bool RenderPart::hasLocalBounds() const
{
    return m_localBounds.isValid();
}

const AxisAlignedBoundingBox& RenderPart::localBounds() const
{
    return m_localBounds;
}

void RenderPart::setLocalBounds(const AxisAlignedBoundingBox& bounds)
{
    if (!bounds.isValid())
    {
        m_localBounds.reset();
        return;
    }

    m_localBounds = bounds;
}

void RenderPart::clearLocalBounds()
{
    m_localBounds.reset();
}

