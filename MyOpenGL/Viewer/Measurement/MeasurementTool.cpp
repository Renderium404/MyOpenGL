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

RenderLabel* MeasurementTool::createPersistentLabel(OpenGLViewerWidget* viewer, RenderItem* item, const QVector3D& anchorPosition, const QPointF& pixelOffset, const QString& text)
{
    if (viewer == 0 || item == 0 || text.isEmpty())
        return 0;

    const int textPixelSize = 16; // 测量标签默认字号。
    RenderLabel* label = item->createTextLabel(viewer->resourceManager(), viewer->materialManager(), text, textPixelSize);
    if (label == 0)
        return 0;

    label->setAnchorWorld(anchorPosition);
    label->setAnchorSence(QVector2D(0.0f, 0.0f));
    label->setPixelOffset(pixelOffset);
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