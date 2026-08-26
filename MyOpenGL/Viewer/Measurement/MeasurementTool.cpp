#include "MeasurementTool.h"

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QRect>
#include <QVector4D>

#include <vector>

#include "MyOpenGL/Item/RenderItem.h"
#include "MyOpenGL/Item/RenderLabel.h"
#include "MyOpenGL/Material/Material.h"
#include "MyOpenGL/Resource/BufferGeometry.h"
#include "MyOpenGL/Resource/Texture.h"
#include "MyOpenGL/Viewer/OpenGLViewerWidget.h"

RenderLabel* MeasurementTool::createPersistentLabel(OpenGLViewerWidget* viewer,RenderItem* item,const QVector3D& anchorPosition,const QPointF& pixelOffset,const QString& text)
{
    if (viewer == 0 || item == 0 || text.isEmpty())
        return 0;
    /// ------------------------------------------------
    /// RenderLabel
    /// ------------------------------------------------
    RenderLabel* label = item->createLabel();
    if (label == 0)
        return 0;
    const RenderLabelId labelId = label->id();
    /// 为当前 Label 创建稳定的资源调试名称。
    const QString resourceName =QStringLiteral("MeasurementLabel_%1_%2").arg(static_cast<qulonglong>(item->id())).arg(static_cast<qulonglong>(labelId));

    /// ------------------------------------------------
    /// Text Image
    /// ------------------------------------------------

    const int fontPixelSize = 16;
    const int horizontalPadding = 6;
    const int verticalPadding = 4;

    QFont font;
    font.setPixelSize(fontPixelSize);

    const QFontMetrics metrics(font);

    /// 使用 boundingRect，兼容较老 Qt5。
    const QRect textBounds = metrics.boundingRect(text);
    const int textWidth = textBounds.width();
    const int textHeight = metrics.height();
    const int imageWidth =textWidth +horizontalPadding * 2;
    const int imageHeight =textHeight +verticalPadding * 2;
    if (imageWidth <= 0 || imageHeight <= 0)
    {
        item->removeLabel(labelId);
        return 0;
    }

    QImage image(imageWidth,imageHeight,QImage::Format_RGBA8888);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    /// 字体抗锯齿由 QPainter 完成。
    painter.setRenderHint(QPainter::TextAntialiasing,true);
    /// Label 背景。
    painter.fillRect(image.rect(), QColor(0, 0, 0, 160));
    painter.setFont(font);
    painter.setPen(QColor(255, 230, 120));
    const QRect textRect(horizontalPadding,verticalPadding,textWidth,textHeight);
    painter.drawText(textRect,Qt::AlignLeft | Qt::AlignVCenter,text);
    painter.end();
    /// ------------------------------------------------
    /// Texture
    /// ------------------------------------------------
    Texture* texture =new Texture(resourceName +QStringLiteral("_Texture"));
    if (!texture->setImage(image))
    {
        delete texture;
        item->removeLabel(labelId);
        return 0;
    }

    if (viewer->resourceManager().adopt(texture) == InvalidResourceId)
    {
        delete texture;
        item->removeLabel(labelId);
        return 0;
    }

    const ResourceId textureId =texture->id();
    /// ------------------------------------------------
    /// Geometry
    /// ------------------------------------------------
    ///
    /// RenderLabel Geometry 使用 Pixel 坐标。
    ///
    /// Geometry 原点固定为 (0, 0)。
    /// Label 在屏幕上的偏移由 RenderLabel::pixelOffset()
    /// 统一控制，不再烘焙进 Geometry。

    BufferGeometry* geometry =new BufferGeometry(resourceName +QStringLiteral("_Geometry"),BufferUsage::Static,RenderType::Triangles);
    std::vector<GeometryVertexAttribute> attributes;
    GeometryVertexAttribute position;
    position.location = GeometryAttribute::Position;
    position.componentCount = 3;
    position.valueOffset = 0;
    attributes.push_back(position);
    GeometryVertexAttribute texCoord;
    texCoord.location = GeometryAttribute::TexCoord;
    texCoord.componentCount = 2;
    texCoord.valueOffset = 3;
    attributes.push_back(texCoord);
    geometry->setVertexLayout(5,attributes);

    const float width =static_cast<float>(imageWidth);

    const float height =static_cast<float>(imageHeight);

    /// QImage 原点位于左上角。
    /// OpenGL Quad 原点位于左下角，
    /// 因此通过 TexCoord V 翻转完成对应。
    const std::vector<GLfloat> vertices =
    {
        0.0f,  0.0f,   0.0f, 0.0f, 1.0f,
        width, 0.0f,   0.0f, 1.0f, 1.0f,
        width, height, 0.0f, 1.0f, 0.0f,
        0.0f,  height, 0.0f, 0.0f, 0.0f
    };

    const std::vector<GLuint> indices =
    {
        0, 1, 2,
        0, 2, 3
    };

    geometry->setVertexData(vertices);
    geometry->setIndexData(indices);

    if (viewer->resourceManager().adopt(geometry) == InvalidResourceId)
    {
        viewer->resourceManager().remove(textureId);

        delete geometry;

        item->removeLabel(labelId);

        return 0;
    }

    const ResourceId geometryId =
        geometry->id();

    /// ------------------------------------------------
    /// Material
    /// ------------------------------------------------

    Material* material =viewer->materialManager().createMaterial(resourceName +QStringLiteral("_Material"));

    if (material == 0)
    {
        viewer->resourceManager().remove(geometryId);
        viewer->resourceManager().remove(textureId);

        item->removeLabel(labelId);

        return 0;
    }

    if (!material->setSurfaceMode(SurfaceMode::Texture))
    {
        viewer->materialManager().remove(material->id());

        viewer->resourceManager().remove(geometryId);

        viewer->resourceManager().remove(textureId);

        item->removeLabel(labelId);

        return 0;
    }

    material->setLightingEnabled(false);
    material->setTexture(texture);

    /// Texture 原色直接输出。
    material->setColor(
        QVector4D(
            1.0f,
            1.0f,
            1.0f,
            1.0f));

    /// ------------------------------------------------
    /// Label State
    /// ------------------------------------------------

    label->setText(text);

    label->setAnchorPosition(
        anchorPosition);

    label->setPixelOffset(
        pixelOffset);

    label->setGeometry(
        geometry);

    label->setMaterial(
        material);

    label->setVisible(true);

    return label;
}

void MeasurementTool::drawOverlayLabel(
    QPainter& painter,
    const QPointF& position,
    const QString& text)
{
    if (text.isEmpty())
        return;

    QFontMetricsF fontMetrics(painter.font());

    QRectF textRect =fontMetrics.boundingRect(QRectF(),Qt::AlignLeft | Qt::AlignTop,text);

    textRect.adjust(-6.0,-4.0,6.0,4.0);
    textRect.moveTopLeft(position);
    painter.fillRect(textRect,QColor(0, 0, 0, 160));
    painter.setPen(QColor(255, 230, 120));
    painter.drawText(textRect.adjusted(6.0,4.0,-6.0,-4.0),Qt::AlignLeft | Qt::AlignTop,text);
}