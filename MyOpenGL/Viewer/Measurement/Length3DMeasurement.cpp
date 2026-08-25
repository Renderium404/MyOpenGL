#include "Length3DMeasurement.h"

#include <QFontMetrics>
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
    m_previewPoint = MeasurementPoint();

    m_cursorPosition = QPointF();
    m_hasCursorPosition = false;
}

bool Length3DMeasurement::mousePressEvent(OpenGLViewerWidget* viewer, QMouseEvent* event)
{
    if (viewer == 0 || event == 0)
        return false;

    m_cursorPosition = event->pos();
    m_hasCursorPosition = true;

    if (event->button() != Qt::LeftButton)
        return true;

    MeasurementPoint point;

    if (m_state == MeasurementState::Idle || m_state == MeasurementState::Finished)
    {
        m_startPoint = MeasurementPoint();
        m_endPoint = MeasurementPoint();

        if (!viewportPointToScene(viewer, event->pos(), point))
        {
            m_previewPoint = MeasurementPoint();
            viewer->update();
            return true;
        }

        m_startPoint = point;
        m_previewPoint = point;
        m_state = MeasurementState::Collecting;

        viewer->update();
        return true;
    }

    if (m_state == MeasurementState::Collecting)
    {
        if (!viewportPointToScene(viewer, event->pos(), point))
        {
            m_previewPoint = MeasurementPoint();
            viewer->update();
            return true;
        }

        if (!commitResult(viewer, m_startPoint.worldPosition, point.worldPosition))
            return true;

        m_endPoint = point;
        m_previewPoint = MeasurementPoint();
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

    m_cursorPosition = event->pos();
    m_hasCursorPosition = true;

    MeasurementPoint point;

    if (viewportPointToScene(viewer, event->pos(), point))
        m_previewPoint = point;
    else
        m_previewPoint = MeasurementPoint();

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

    /// 等待第一个点。
    if (m_state == MeasurementState::Idle || m_state == MeasurementState::Finished)
    {
        if (!m_hasCursorPosition)
            return;

        QPoint currentPosition = m_cursorPosition.toPoint();

        if (m_previewPoint.valid)
        {
            QPoint projectedPosition;

            if (viewer->worldPointAtScene(m_previewPoint.worldPosition, projectedPosition))
                currentPosition = projectedPosition;
        }

        painter.setPen(pointColor);
        painter.setBrush(pointColor);
        painter.drawEllipse(currentPosition, 4, 4);

        const QString pointLabel = QStringLiteral("P1=%1").arg(pointText(m_previewPoint));
        drawOverlayLabel(painter, QPointF(currentPosition) + QPointF(10.0, -30.0), pointLabel);

        return;
    }

    /// 已确定第一个点，正在选择第二个点。
    if (m_state != MeasurementState::Collecting || !m_startPoint.valid)
        return;

    QPoint startPosition;

    if (!viewer->worldPointAtScene(m_startPoint.worldPosition, startPosition))
        return;

    QPoint currentPosition = m_cursorPosition.toPoint();

    if (m_previewPoint.valid)
    {
        QPoint projectedPosition;

        if (viewer->worldPointAtScene(m_previewPoint.worldPosition, projectedPosition))
            currentPosition = projectedPosition;
    }

    /// 测量线。
    QPen linePen(pointColor);
    linePen.setWidth(2);
    linePen.setStyle(Qt::DashLine);

    painter.setPen(linePen);
    painter.setBrush(pointColor);

    if (m_previewPoint.valid)
        painter.drawLine(startPosition, currentPosition);

    painter.drawEllipse(startPosition, 4, 4);
    painter.drawEllipse(currentPosition, 4, 4);

    /// P1。
    const QString startLabel = QStringLiteral("P1=%1").arg(pointText(m_startPoint));
    drawOverlayLabel(painter, QPointF(startPosition) + QPointF(10.0, -30.0), startLabel);

    /// P2。
    const QString currentLabel = QStringLiteral("P2=%1").arg(pointText(m_previewPoint));
    drawOverlayLabel(painter, QPointF(currentPosition) + QPointF(10.0, 10.0), currentLabel);

    /// 长度。
    QString lengthText = QStringLiteral("L=?");

    if (m_previewPoint.valid)
    {
        const float length = (m_previewPoint.worldPosition - m_startPoint.worldPosition).length();
        lengthText = QStringLiteral("L=%1").arg(QString::number(length, 'f', 3));
    }

    const QPointF middlePosition((startPosition.x() + currentPosition.x()) * 0.5, (startPosition.y() + currentPosition.y()) * 0.5);
    drawOverlayLabel(painter, middlePosition + QPointF(10.0, -30.0), lengthText);
}
bool Length3DMeasurement::viewportPointToScene(OpenGLViewerWidget* viewer, const QPoint& viewportPosition, MeasurementPoint& point) const
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

    m_resultMaterial = viewer->materialManager().createMaterial("MeasurementLength3DMaterial");

    if (m_resultMaterial == 0)
        return false;

    if (!m_resultMaterial->setSurfaceMode(SurfaceMode::VertexColor))
        return false;

    m_resultMaterial->setLightingEnabled(false);

    return true;
}

bool Length3DMeasurement::commitResult(OpenGLViewerWidget* viewer, const QVector3D& start, const QVector3D& end)
{
    if (viewer == 0)
        return false;

    if (!ensureResultMaterial(viewer))
        return false;

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
        start.x(), start.y(), start.z(), color.x(), color.y(), color.z(),
        end.x(), end.y(), end.z(), color.x(), color.y(), color.z()
    };

    const std::vector<GLuint> indices =
    {
        0, 1
    };

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

    return true;
}

QString Length3DMeasurement::pointText(const MeasurementPoint& point) const
{
    if (!point.valid)
        return QStringLiteral("(?, ?, ?)");

    return QStringLiteral("(%1, %2, %3)")
        .arg(QString::number(point.worldPosition.x(), 'f', 3))
        .arg(QString::number(point.worldPosition.y(), 'f', 3))
        .arg(QString::number(point.worldPosition.z(), 'f', 3));
}