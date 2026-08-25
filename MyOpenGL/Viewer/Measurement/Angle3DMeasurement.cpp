#include "Angle3DMeasurement.h"

#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>

#include <cmath>
#include <vector>

#include "MyOpenGL/Item/RenderItem.h"
#include "MyOpenGL/Item/RenderPart.h"
#include "MyOpenGL/Material/Material.h"
#include "MyOpenGL/Resource/BufferGeometry.h"
#include "MyOpenGL/Viewer/OpenGLViewerWidget.h"

Angle3DMeasurement::Angle3DMeasurement()
    : m_state(MeasurementState::Idle)
    , m_pointCount(0)
    , m_hasCursorPosition(false)
    , m_resultMaterial(0)
{
}

MeasurementType Angle3DMeasurement::type() const
{
    return MeasurementType::Angle3D;
}

MeasurementState Angle3DMeasurement::state() const
{
    return m_state;
}

void Angle3DMeasurement::reset()
{
    m_state = MeasurementState::Idle;
    m_pointCount = 0;

    m_firstPoint = MeasurementPoint();
    m_vertexPoint = MeasurementPoint();
    m_endPoint = MeasurementPoint();
    m_previewPoint = MeasurementPoint();

    m_cursorPosition = QPointF();
    m_hasCursorPosition = false;
}

bool Angle3DMeasurement::mousePressEvent(OpenGLViewerWidget* viewer, QMouseEvent* event)
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
        m_firstPoint = MeasurementPoint();
        m_vertexPoint = MeasurementPoint();
        m_endPoint = MeasurementPoint();
        m_pointCount = 0;

        if (!viewportPointToScene(viewer, event->pos(), point))
        {
            m_previewPoint = MeasurementPoint();
            viewer->update();
            return true;
        }

        m_firstPoint = point;
        m_previewPoint = point;
        m_pointCount = 1;
        m_state = MeasurementState::Collecting;

        viewer->update();
        return true;
    }

    if (m_pointCount == 1)
    {
        if (!viewportPointToScene(viewer, event->pos(), point))
        {
            m_previewPoint = MeasurementPoint();
            viewer->update();
            return true;
        }

        m_vertexPoint = point;
        m_previewPoint = point;
        m_pointCount = 2;

        viewer->update();
        return true;
    }

    if (m_pointCount == 2)
    {
        if (!viewportPointToScene(viewer, event->pos(), point))
        {
            m_previewPoint = MeasurementPoint();
            viewer->update();
            return true;
        }

        if (!commitResult(viewer, m_firstPoint.worldPosition, m_vertexPoint.worldPosition, point.worldPosition))
            return true;

        m_endPoint = point;
        m_previewPoint = MeasurementPoint();
        m_pointCount = 3;
        m_state = MeasurementState::Finished;

        viewer->update();
        return true;
    }

    return true;
}

bool Angle3DMeasurement::mouseMoveEvent(OpenGLViewerWidget* viewer, QMouseEvent* event)
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

bool Angle3DMeasurement::mouseReleaseEvent(OpenGLViewerWidget* viewer, QMouseEvent* event)
{
    Q_UNUSED(viewer);
    Q_UNUSED(event);

    return true;
}

bool Angle3DMeasurement::keyPressEvent(OpenGLViewerWidget* viewer, QKeyEvent* event)
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

void Angle3DMeasurement::drawOverlay(OpenGLViewerWidget* viewer, QPainter& painter) const
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

    if (m_state != MeasurementState::Collecting || !m_firstPoint.valid)
        return;

    QPoint firstPosition;

    if (!viewer->worldPointAtScene(m_firstPoint.worldPosition, firstPosition))
        return;

    QPoint currentPosition = m_cursorPosition.toPoint();

    if (m_previewPoint.valid)
    {
        QPoint projectedPosition;

        if (viewer->worldPointAtScene(m_previewPoint.worldPosition, projectedPosition))
            currentPosition = projectedPosition;
    }

    /// 第一个固定点。
    painter.setPen(pointColor);
    painter.setBrush(pointColor);
    painter.drawEllipse(firstPosition, 4, 4);

    const QString firstLabel = QStringLiteral("P1=%1").arg(pointText(m_firstPoint));
    drawOverlayLabel(painter, QPointF(firstPosition) + QPointF(10.0, -30.0), firstLabel);

    /// 正在选择第二个点。
    if (m_pointCount == 1)
    {
        QPen previewPen(pointColor);
        previewPen.setWidth(2);
        previewPen.setStyle(Qt::DashLine);

        painter.setPen(previewPen);

        if (m_previewPoint.valid)
            painter.drawLine(firstPosition, currentPosition);

        painter.setBrush(pointColor);
        painter.drawEllipse(currentPosition, 4, 4);

        const QString secondLabel = QStringLiteral("P2=%1").arg(pointText(m_previewPoint));
        drawOverlayLabel(painter, QPointF(currentPosition) + QPointF(10.0, 10.0), secondLabel);

        drawOverlayLabel(painter, QPointF(currentPosition) + QPointF(10.0, 40.0), QStringLiteral("A=?"));

        return;
    }

    /// 正在选择第三个点。
    if (m_pointCount != 2 || !m_vertexPoint.valid)
        return;

    QPoint vertexPosition;

    if (!viewer->worldPointAtScene(m_vertexPoint.worldPosition, vertexPosition))
        return;

    /// 第一条已经确定的边。
    QPen fixedPen(pointColor);
    fixedPen.setWidth(2);

    painter.setPen(fixedPen);
    painter.drawLine(vertexPosition, firstPosition);

    /// 第二条预览边。
    QPen previewPen(pointColor);
    previewPen.setWidth(2);
    previewPen.setStyle(Qt::DashLine);

    painter.setPen(previewPen);

    if (m_previewPoint.valid)
        painter.drawLine(vertexPosition, currentPosition);

    painter.setBrush(pointColor);

    painter.drawEllipse(vertexPosition, 4, 4);
    painter.drawEllipse(currentPosition, 4, 4);

    /// P2。
    const QString vertexLabel = QStringLiteral("P2=%1").arg(pointText(m_vertexPoint));
    drawOverlayLabel(painter, QPointF(vertexPosition) + QPointF(10.0, 10.0), vertexLabel);

    /// P3。
    const QString currentLabel = QStringLiteral("P3=%1").arg(pointText(m_previewPoint));
    drawOverlayLabel(painter, QPointF(currentPosition) + QPointF(10.0, 10.0), currentLabel);

    /// 角度。
    QString angleText = QStringLiteral("A=?");

    if (m_previewPoint.valid)
    {
        double angle = 0.0;

        if (angleValue(m_firstPoint.worldPosition, m_vertexPoint.worldPosition, m_previewPoint.worldPosition, angle))
            angleText = QStringLiteral("A=%1°").arg(QString::number(angle, 'f', 2));
    }

    drawOverlayLabel(painter, QPointF(vertexPosition) + QPointF(10.0, -30.0), angleText);
}

bool Angle3DMeasurement::viewportPointToScene(OpenGLViewerWidget* viewer, const QPoint& viewportPosition, MeasurementPoint& point) const
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

bool Angle3DMeasurement::ensureResultMaterial(OpenGLViewerWidget* viewer)
{
    if (viewer == 0)
        return false;

    if (m_resultMaterial != 0)
        return true;

    m_resultMaterial = viewer->materialManager().createMaterial("MeasurementAngle3DMaterial");

    if (m_resultMaterial == 0)
        return false;

    if (!m_resultMaterial->setSurfaceMode(SurfaceMode::VertexColor))
        return false;

    m_resultMaterial->setLightingEnabled(false);

    return true;
}

bool Angle3DMeasurement::commitResult(OpenGLViewerWidget* viewer, const QVector3D& first, const QVector3D& vertex, const QVector3D& end)
{
    if (viewer == 0)
        return false;

    if (!ensureResultMaterial(viewer))
        return false;

    BufferGeometry* geometry = new BufferGeometry("MeasurementAngle3DResult", BufferUsage::Static, RenderType::Lines);

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
        first.x(), first.y(), first.z(), color.x(), color.y(), color.z(),
        vertex.x(), vertex.y(), vertex.z(), color.x(), color.y(), color.z(),

        vertex.x(), vertex.y(), vertex.z(), color.x(), color.y(), color.z(),
        end.x(), end.y(), end.z(), color.x(), color.y(), color.z()
    };

    const std::vector<GLuint> indices =
    {
        0, 1,
        2, 3
    };

    geometry->setVertexData(vertices);
    geometry->setIndexData(indices);

    if (viewer->resourceManager().adopt(geometry) == InvalidResourceId)
    {
        delete geometry;
        return false;
    }

    RenderItem* item = viewer->measurementItemManager().createItem("MeasurementAngle3DResult");

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

bool Angle3DMeasurement::angleValue(const QVector3D& first, const QVector3D& vertex, const QVector3D& end, double& angle) const
{
    const QVector3D firstDirection = first - vertex;
    const QVector3D endDirection = end - vertex;

    const double firstLength = firstDirection.length();
    const double endLength = endDirection.length();

    if (firstLength <= 1.0e-8 || endLength <= 1.0e-8)
        return false;

    double cosine = QVector3D::dotProduct(firstDirection, endDirection) / (firstLength * endLength);
    cosine = qBound(-1.0, cosine, 1.0);

    angle = std::acos(cosine) * 180.0 / 3.14159265358979323846;

    return true;
}

QString Angle3DMeasurement::pointText(const MeasurementPoint& point) const
{
    if (!point.valid)
        return QStringLiteral("(?, ?, ?)");

    return QStringLiteral("(%1, %2, %3)")
        .arg(QString::number(point.worldPosition.x(), 'f', 3))
        .arg(QString::number(point.worldPosition.y(), 'f', 3))
        .arg(QString::number(point.worldPosition.z(), 'f', 3));
}