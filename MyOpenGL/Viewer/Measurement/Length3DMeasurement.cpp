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

Length3DMeasurement::Length3DMeasurement()
    : m_state(MeasurementState::Idle)
    , m_hasCursorPosition(false)
    , m_resultMaterial(0)
{
}

MeasurementType Length3DMeasurement::type() const
{
    return MeasurementType::Length3D;
}

MeasurementState Length3DMeasurement::state() const
{
    return m_state;
}

void Length3DMeasurement::reset()
{
    m_state = MeasurementState::Idle;

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

    if (m_state == MeasurementState::Idle || m_state == MeasurementState::Finished)
    {
        m_startPoint = point;
        m_endPoint = MeasurementPoint();
        m_state = MeasurementState::Collecting;

        viewer->update();
        return true;
    }

    if (m_state == MeasurementState::Collecting)
    {
        if (!commitResult(viewer, m_startPoint, point))
        {
            qWarning() << "Length3DMeasurement mousePressEvent failed to commit result.";
            viewer->update();
            return true;
        }

        m_endPoint = point;
        m_state = MeasurementState::Finished;

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

bool Length3DMeasurement::keyPressEvent(OpenGLViewerWidget* viewer, QKeyEvent* event)
{
    if (viewer == 0 || event == 0)
        return false;

    if (event->key() == Qt::Key_Escape)
    {
        reset();
        viewer->update();
    }

    return true;
}

void Length3DMeasurement::drawOverlay(OpenGLViewerWidget* viewer, QPainter& painter) const
{
    if (viewer == 0)
        return;

    const QColor pointColor(255, 210, 40);

    if (m_state == MeasurementState::Idle)
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

    if (m_state == MeasurementState::Finished)
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

    if (m_state != MeasurementState::Collecting || !m_startPoint.valid)
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
    linePen.setWidth(2);
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

    BufferGeometry* geometry = new BufferGeometry("MeasurementLength3DResult", BufferUsage::Static, RenderType::Lines);

    std::vector<GeometryVertexAttribute> attributes;

    GeometryVertexAttribute position;
    position.location = GeometryAttribute::Position;
    position.componentCount = 3;
    position.valueOffset = 0;
    attributes.push_back(position);

    GeometryVertexAttribute colorAttribute;
    colorAttribute.location = GeometryAttribute::Color;
    colorAttribute.componentCount = 3;
    colorAttribute.valueOffset = 3;
    attributes.push_back(colorAttribute);

    geometry->setVertexLayout(6, attributes);

    const QVector3D color(1.0f, 0.82f, 0.16f);

    const std::vector<GLfloat> vertices =
    {
        p1.x(), p1.y(), p1.z(), color.x(), color.y(), color.z(),
        p2.x(), p2.y(), p2.z(), color.x(), color.y(), color.z()
    };

    const std::vector<GLuint> indices = { 0, 1 };

    geometry->setVertexData(vertices);
    geometry->setIndexData(indices);

    if (viewer->resourceManager().adopt(geometry) == InvalidResourceId)
    {
        delete geometry;
        return false;
    }

    RenderItem* item = viewer->measurementItemManager().createItem("MeasurementLength3DResult");

    if (item == 0)
    {
        viewer->resourceManager().remove(geometry->id());
        return false;
    }

    item->setMaterial(m_resultMaterial);
    item->setDepthTestEnabled(false);

    RenderPart* part = item->createPart();

    if (part == 0)
    {
        viewer->measurementItemManager().remove(item->id());
        viewer->resourceManager().remove(geometry->id());
        return false;
    }

    part->setGeometry(geometry);

    if (createPersistentLabel(viewer, item, p1, QPointF(5.0, -15.0), QStringLiteral("P1=%1").arg(pointText(start))) == 0)
        qWarning() << "Length3DMeasurement commitResult failed to create P1 Label.";

    if (createPersistentLabel(viewer, item, p2, QPointF(5.0, 5.0), QStringLiteral("P2=%1").arg(pointText(end))) == 0)
        qWarning() << "Length3DMeasurement commitResult failed to create P2 Label.";

    const float length = (p2 - p1).length();
    const QString lengthText = QStringLiteral("L=%1").arg(QString::number(length, 'f', 3));
    const QVector3D middle = (p1 + p2) * 0.5f;

    if (createPersistentLabel(viewer, item, middle, QPointF(5.0, -15.0), lengthText) == 0)
        qWarning() << "Length3DMeasurement commitResult failed to create Length Label.";

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