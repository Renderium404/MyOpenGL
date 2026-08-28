#include "MeasurementTool.h"

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QRect>
#include <QVector4D>
#include <QKeyEvent>
#include <QMouseEvent>
#include <vector>

#include "MyOpenGL/Item/RenderItem.h"
#include "MyOpenGL/Item/RenderLabel.h"
#include "MyOpenGL/Material/Material.h"
#include "MyOpenGL/Resource/BufferGeometry.h"
#include "MyOpenGL/Resource/Texture.h"
#include "MyOpenGL/Viewer/OpenGLViewerWidget.h"
MeasurementTool::MeasurementTool()
    : m_state(MeasurementState::Idle)
    , m_lineColor(1.0f, 0.82f, 0.16f, 1.0f)
    , m_lineWidth(2.0f)
{
}

MeasurementTool::~MeasurementTool()
{
}

MeasurementState MeasurementTool::state() const
{
    return m_state;
}

void MeasurementTool::setState(MeasurementState state)
{
    m_state = state;
}

const QVector4D& MeasurementTool::lineColor() const
{
    return m_lineColor;
}

void MeasurementTool::setLineColor(const QVector4D& color)
{
    m_lineColor = color;
}

float MeasurementTool::lineWidth() const
{
    return m_lineWidth;
}

bool MeasurementTool::setLineWidth(float width)
{
    if (width <= 0.0f)
        return false;

    m_lineWidth = width;
    return true;
}
RenderLabel* MeasurementTool::createPersistentLabel(OpenGLViewerWidget* viewer, RenderItem* item, const QVector3D& anchor3D, const QVector2D& anchor2D, const QPointF& pixelOffset, const QString& text)
{
    if (viewer == 0 || item == 0 || text.isEmpty())
        return 0;

    const int textPixelSize = 16; // 测量标签默认字号。

    RenderLabel* label = item->createTextLabel(viewer->resourceManager(), viewer->materialManager(), text, textPixelSize);

    if (label == 0)
        return 0;

    label->setAnchor3D(anchor3D);
    label->setAnchor2D(anchor2D);
    label->setPixelOffset(pixelOffset);

    return label;
}
bool MeasurementTool::keyPressEvent(OpenGLViewerWidget* viewer, QKeyEvent* event)
{
    if (viewer == 0 || event == 0)
        return false;

    if (event->key() == Qt::Key_Escape)
    {
        reset();
        setState(MeasurementState::Finished);
        viewer->update();
    }

    return true;
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