#include "Angle2DMeasurement.h"

#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>

#include <cmath>

#include "MyOpenGL/Camera/Camera.h"
#include "MyOpenGL/Viewer/OpenGLViewerWidget.h"

Angle2DMeasurement::Angle2DMeasurement()
    : m_state(MeasurementState::Idle)
    , m_pointCount(0)
    , m_planeOrigin(0.0f, 0.0f, 0.0f)
{
}

MeasurementType Angle2DMeasurement::type() const
{
    return MeasurementType::Angle2D;
}

MeasurementState Angle2DMeasurement::state() const
{
    return m_state;
}

void Angle2DMeasurement::reset()
{
    m_state = MeasurementState::Idle;
    m_pointCount = 0;

    m_firstPoint = MeasurementPoint();
    m_vertexPoint = MeasurementPoint();
    m_endPoint = MeasurementPoint();
    m_previewPoint = MeasurementPoint();

    m_planeOrigin = QVector3D(0.0f, 0.0f, 0.0f);
    m_planeNormal = QVector3D();
}

bool Angle2DMeasurement::mousePressEvent(OpenGLViewerWidget* viewer, QMouseEvent* event)
{
    if (viewer == 0 || event == 0)
        return false;

    if (event->button() != Qt::LeftButton)
        return true;

    const Camera* camera = viewer->cameraManager().activeCamera();

    if (camera == 0)
        return true;

    if (m_state == MeasurementState::Idle || m_state == MeasurementState::Finished)
    {
        reset();

        m_planeOrigin = QVector3D(0.0f, 0.0f, 0.0f);
        m_planeNormal = camera->forward().normalized();

        if (!viewportPointToPlane(viewer, event->pos(), m_firstPoint))
            return true;

        m_previewPoint = m_firstPoint;
        m_pointCount = 1;
        m_state = MeasurementState::Collecting;

        viewer->update();
        return true;
    }

    if (m_pointCount == 1)
    {
        if (!viewportPointToPlane(viewer, event->pos(), m_vertexPoint))
            return true;

        m_previewPoint = m_vertexPoint;
        m_pointCount = 2;

        viewer->update();
        return true;
    }

    if (m_pointCount == 2)
    {
        if (!viewportPointToPlane(viewer, event->pos(), m_endPoint))
            return true;

        m_previewPoint = MeasurementPoint();
        m_pointCount = 3;
        m_state = MeasurementState::Finished;

        viewer->update();
        return true;
    }

    return true;
}

bool Angle2DMeasurement::mouseMoveEvent(OpenGLViewerWidget* viewer, QMouseEvent* event)
{
    if (viewer == 0 || event == 0)
        return false;

    if (m_state != MeasurementState::Collecting)
        return true;

    MeasurementPoint point;

    if (viewportPointToPlane(viewer, event->pos(), point))
        m_previewPoint = point;
    else
        m_previewPoint = MeasurementPoint();

    viewer->update();
    return true;
}

bool Angle2DMeasurement::mouseReleaseEvent(OpenGLViewerWidget* viewer, QMouseEvent* event)
{
    Q_UNUSED(viewer);
    Q_UNUSED(event);

    return true;
}

bool Angle2DMeasurement::keyPressEvent(OpenGLViewerWidget* viewer, QKeyEvent* event)
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

void Angle2DMeasurement::drawOverlay(OpenGLViewerWidget* viewer, QPainter& painter) const
{
    if (viewer == 0 || !m_firstPoint.valid)
        return;

    QPoint firstPosition;

    if (!viewer->worldPointAtScene(m_firstPoint.worldPosition, firstPosition))
        return;

    QPen linePen(QColor(255, 210, 40));
    linePen.setWidth(2);

    if (m_state == MeasurementState::Collecting)
        linePen.setStyle(Qt::DashLine);

    painter.setPen(linePen);
    painter.setBrush(QColor(255, 210, 40));

    /// 正在选择角点。
    if (m_pointCount == 1 && m_previewPoint.valid)
    {
        QPoint previewPosition;

        if (!viewer->worldPointAtScene(m_previewPoint.worldPosition, previewPosition))
            return;

        painter.drawLine(firstPosition, previewPosition);
        painter.drawEllipse(firstPosition, 4, 4);
        painter.drawEllipse(previewPosition, 4, 4);

        return;
    }

    if (!m_vertexPoint.valid)
        return;

    QPoint vertexPosition;

    if (!viewer->worldPointAtScene(m_vertexPoint.worldPosition, vertexPosition))
        return;

    const MeasurementPoint* targetPoint = 0;

    if (m_state == MeasurementState::Finished && m_endPoint.valid)
        targetPoint = &m_endPoint;
    else if (m_state == MeasurementState::Collecting && m_previewPoint.valid)
        targetPoint = &m_previewPoint;

    if (targetPoint == 0)
        return;

    QPoint endPosition;

    if (!viewer->worldPointAtScene(targetPoint->worldPosition, endPosition))
        return;

    painter.drawLine(vertexPosition, firstPosition);
    painter.drawLine(vertexPosition, endPosition);

    painter.drawEllipse(firstPosition, 4, 4);
    painter.drawEllipse(vertexPosition, 4, 4);
    painter.drawEllipse(endPosition, 4, 4);

    QString angleText = QStringLiteral("A=?");
    double angle = 0.0;

    if (angleValue(m_firstPoint.worldPosition, m_vertexPoint.worldPosition, targetPoint->worldPosition, angle))
        angleText = QStringLiteral("A=%1°").arg(QString::number(angle, 'f', 2));

    drawOverlayLabel(painter, QPointF(vertexPosition) + QPointF(8.0, -28.0), angleText);
}


bool Angle2DMeasurement::viewportPointToPlane(OpenGLViewerWidget* viewer, const QPoint& viewportPosition, MeasurementPoint& point) const
{
    if (viewer == 0)
        return false;

    const Camera* camera = viewer->cameraManager().activeCamera();

    if (camera == 0 || viewer->width() <= 0 || viewer->height() <= 0)
        return false;

    QVector3D rayOrigin;
    QVector3D rayDirection;

    if (!camera->screenPointToRay(viewportPosition.x(), viewportPosition.y(), viewer->width(), viewer->height(), rayOrigin, rayDirection))
        return false;

    const float denominator = QVector3D::dotProduct(rayDirection, m_planeNormal);

    if (qAbs(denominator) <= 1.0e-8f)
        return false;

    const float distance = QVector3D::dotProduct(m_planeOrigin - rayOrigin, m_planeNormal) / denominator;

    if (distance < 0.0f)
        return false;

    point.viewportPosition = viewportPosition;
    point.worldPosition = rayOrigin + rayDirection * distance;
    point.valid = true;

    return true;
}

bool Angle2DMeasurement::angleValue(const QVector3D& first, const QVector3D& vertex, const QVector3D& end, double& angle) const
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