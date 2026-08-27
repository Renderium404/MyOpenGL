#include "Length3DMeasurement.h"

#include <QDebug>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>

#include <vector>

#include "MyOpenGL/Item/RenderItem.h"
#include "MyOpenGL/Item/RenderPart.h"
#include "MyOpenGL/Material/Material.h"
#include "MyOpenGL/Resource/BufferGeometry.h"
#include "MyOpenGL/Viewer/OpenGLViewerWidget.h"
#include "MyOpenGL/Viewer/Modeling/SimpleModeling.h"
Length3DMeasurement::Length3DMeasurement()
    : m_hasCursorPosition(false)
    , m_resultMaterial(0)
{
}

MeasurementType Length3DMeasurement::type() const
{
    return MeasurementType::Length3D;
}


void Length3DMeasurement::reset()
{
    setState(MeasurementState::Idle);

    m_startPoint = MeasurementPoint();
    m_endPoint = MeasurementPoint();
    m_currentPoint = MeasurementPoint();

    m_cursorPosition = QPointF();
    m_hasCursorPosition = false;
}

bool Length3DMeasurement::mousePressEvent(OpenGLViewerWidget* viewer, QMouseEvent* event)
{
    if (viewer == 0 || event == 0)
        return false;

    m_cursorPosition = event->localPos();
    m_hasCursorPosition = true;

    if (event->button() != Qt::LeftButton)
        return true;

    MeasurementPoint point;

    if (!viewportPointToScene(viewer, event->localPos(), point))
    {
        m_currentPoint = MeasurementPoint();
        viewer->update();
        return true;
    }

    m_currentPoint = point;

    if (state() == MeasurementState::Idle || state() == MeasurementState::Finished)
    {
        m_startPoint = point;
        m_endPoint = MeasurementPoint();
        setState(MeasurementState::Collecting );

        viewer->update();
        return true;
    }

    if (state() == MeasurementState::Collecting)
    {
        if (!commitResult(viewer, m_startPoint, point))
        {
            qWarning() << "Length3DMeasurement mousePressEvent failed to commit result.";
            viewer->update();
            return true;
        }

        m_endPoint = point;
        setState(MeasurementState::Finished );

        viewer->update();
        return true;
    }

    return true;
}

bool Length3DMeasurement::mouseMoveEvent(OpenGLViewerWidget* viewer, QMouseEvent* event)
{
    if (viewer == 0 || event == 0)
        return false;

    m_cursorPosition = event->localPos();
    m_hasCursorPosition = true;

    MeasurementPoint point;

    if (viewportPointToScene(viewer, event->localPos(), point))
        m_currentPoint = point;
    else
        m_currentPoint = MeasurementPoint();

    viewer->update();
    return true;
}

bool Length3DMeasurement::mouseReleaseEvent(OpenGLViewerWidget* viewer, QMouseEvent* event)
{
    Q_UNUSED(viewer);
    Q_UNUSED(event);

    return true;
}
void Length3DMeasurement::drawOverlay(OpenGLViewerWidget* viewer, QPainter& painter) const
{
    if (viewer == 0)
        return;

    const QVector4D& color = lineColor();
    const QColor pointColor = QColor::fromRgbF(color.x(), color.y(), color.z(), color.w());


    if (state() == MeasurementState::Idle)
    {
        if (!m_hasCursorPosition)
            return;

        QPointF currentPosition = m_cursorPosition;

        if (m_currentPoint.valid)
        {
            QPointF projectedPosition;

            if (viewer->worldPointAtScene(m_currentPoint.worldPosition, projectedPosition))
                currentPosition = projectedPosition;
        }

        painter.setPen(pointColor);
        painter.setBrush(pointColor);
        painter.drawEllipse(currentPosition, 4.0, 4.0);

        drawOverlayLabel(painter, currentPosition + QPointF(10.0, -30.0), QStringLiteral("P1=%1").arg(pointText(m_currentPoint)));
        return;
    }

    if (state() == MeasurementState::Finished)
    {
        if (!m_hasCursorPosition)
            return;

        QPointF currentPosition = m_cursorPosition;

        if (m_currentPoint.valid)
        {
            QPointF projectedPosition;

            if (viewer->worldPointAtScene(m_currentPoint.worldPosition, projectedPosition))
                currentPosition = projectedPosition;
        }

        painter.setPen(pointColor);
        painter.setBrush(pointColor);
        painter.drawEllipse(currentPosition, 4.0, 4.0);

        drawOverlayLabel(painter, currentPosition + QPointF(10.0, -30.0), QStringLiteral("P=%1").arg(pointText(m_currentPoint)));
        return;
    }

    if (state() != MeasurementState::Collecting || !m_startPoint.valid)
        return;

    QPointF startPosition;

    if (!viewer->worldPointAtScene(m_startPoint.worldPosition, startPosition))
        return;

    QPointF currentPosition = m_cursorPosition;

    if (m_currentPoint.valid)
    {
        QPointF projectedPosition;

        if (viewer->worldPointAtScene(m_currentPoint.worldPosition, projectedPosition))
            currentPosition = projectedPosition;
    }

    QPen linePen(pointColor);
    linePen.setWidthF(lineWidth());
    linePen.setStyle(Qt::DashLine);

    painter.setPen(linePen);
    painter.setBrush(pointColor);

    if (m_currentPoint.valid)
        painter.drawLine(startPosition, currentPosition);

    painter.drawEllipse(startPosition, 4.0, 4.0);
    painter.drawEllipse(currentPosition, 4.0, 4.0);

    drawOverlayLabel(painter, startPosition + QPointF(10.0, -30.0), QStringLiteral("P1=%1").arg(pointText(m_startPoint)));
    drawOverlayLabel(painter, currentPosition + QPointF(10.0, 10.0), QStringLiteral("P2=%1").arg(pointText(m_currentPoint)));

    QString lengthText = QStringLiteral("L=?");

    if (m_currentPoint.valid)
    {
        const float length = (m_currentPoint.worldPosition - m_startPoint.worldPosition).length();
        lengthText = QStringLiteral("L=%1").arg(QString::number(length, 'f', 3));
    }

    const QPointF middlePosition((startPosition.x() + currentPosition.x()) * 0.5, (startPosition.y() + currentPosition.y()) * 0.5);
    drawOverlayLabel(painter, middlePosition + QPointF(10.0, -30.0), lengthText);
}

bool Length3DMeasurement::viewportPointToScene(OpenGLViewerWidget* viewer, const QPointF& viewportPosition, MeasurementPoint& point) const
{
    if (viewer == 0)
        return false;

    QVector3D worldPosition;

    if (!viewer->scenePointAtWorld(viewportPosition, worldPosition))
        return false;

    point.viewportPosition = viewportPosition;
    point.worldPosition = worldPosition;
    point.valid = true;

    return true;
}

bool Length3DMeasurement::ensureResultMaterial(OpenGLViewerWidget* viewer)
{
    if (viewer == 0)
        return false;

    if (m_resultMaterial != 0)
        return true;

    Material* material = viewer->materialManager().createMaterial("MeasurementLength3DMaterial");

    if (material == 0)
        return false;

    if (!material->setSurfaceMode(SurfaceMode::VertexColor))
    {
        viewer->materialManager().remove(material->id());
        return false;
    }

    material->setLightingEnabled(false);
    m_resultMaterial = material;

    return true;
}

bool Length3DMeasurement::commitResult(OpenGLViewerWidget* viewer, const MeasurementPoint& start, const MeasurementPoint& end)
{
    if (viewer == 0 || !start.valid || !end.valid)
        return false;

    if (!ensureResultMaterial(viewer))
        return false;

    const QVector3D& p1 = start.worldPosition;
    const QVector3D& p2 = end.worldPosition;

    RenderItem* item = viewer->measurementItemManager().createItem("MeasurementLength3DResult");

    if (item == 0)
        return false;

    item->setMaterial(m_resultMaterial);
    item->setDepthTestEnabled(false);

    const QVector4D& measurementColor = lineColor();
    const QVector3D geometryColor(measurementColor.x(), measurementColor.y(), measurementColor.z());

    BufferGeometry* geometry = SimpleModeling::createLine("MeasurementLength3DLine", p1, p2, geometryColor, lineWidth());

    if (geometry == 0 || viewer->resourceManager().adopt(geometry) == InvalidResourceId)
    {
        delete geometry;
        viewer->measurementItemManager().remove(item->id());
        return false;
    }

    RenderPart* part = item->createPart();

    if (part != 0)
        part->setGeometry(geometry);

    const QVector2D sceneAnchor(0.0f, 0.0f);

    createPersistentLabel(viewer, item, p1, sceneAnchor, QPointF(5.0, -15.0), QStringLiteral("P1=%1").arg(pointText(start)));
    createPersistentLabel(viewer, item, p2, sceneAnchor, QPointF(5.0, 5.0), QStringLiteral("P2=%1").arg(pointText(end)));

    const float length = (p2 - p1).length();
    const QVector3D middle = (p1 + p2) * 0.5f;

    createPersistentLabel(viewer, item, middle, sceneAnchor, QPointF(5.0, -15.0), QStringLiteral("L=%1").arg(QString::number(length, 'f', 3)));

    return true;
}
QString Length3DMeasurement::pointText(const MeasurementPoint& point) const
{
    if (!point.valid)
        return QStringLiteral("(?, ?, ?)");

    return QStringLiteral("(%1, %2, %3)").arg(QString::number(point.worldPosition.x(), 'f', 3))
                                        .arg(QString::number(point.worldPosition.y(), 'f', 3))
                                        .arg(QString::number(point.worldPosition.z(), 'f', 3));
}