#include "MeasurementTool.h"

#include <QFontMetricsF>
#include <QPainter>

void MeasurementTool::drawOverlayLabel(QPainter& painter, const QPointF& position, const QString& text)
{
    if (text.isEmpty())
        return;

    QFontMetricsF fontMetrics(painter.font());
    QRectF textRect = fontMetrics.boundingRect(QRectF(), Qt::AlignLeft | Qt::AlignTop, text);

    textRect.adjust(-6.0, -4.0, 6.0, 4.0);
    textRect.moveTopLeft(position);

    painter.fillRect(textRect, QColor(0, 0, 0, 160));

    painter.setPen(QColor(255, 230, 120));
    painter.drawText(textRect.adjusted(6.0, 4.0, -6.0, -4.0), Qt::AlignLeft | Qt::AlignTop, text);
}