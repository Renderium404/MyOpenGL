#include "Length2DMeasurement.h"

#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QVector4D>

#include "MyOpenGL/Camera/Camera.h"
#include "MyOpenGL/Viewer/OpenGLViewerWidget.h"

Length2DMeasurement::Length2DMeasurement()
    : m_state(MeasurementState::Idle)
    , m_planeOrigin(0.0f, 0.0f, 0.0f)
{
}

MeasurementType Length2DMeasurement::type() const
{
    return MeasurementType::Length2D;
}

MeasurementState Length2DMeasurement::state() const
{
    return m_state;
}

void Length2DMeasurement::reset()
{
    m_state = MeasurementState::Idle;

    m_startPoint = MeasurementPoint();
    m_endPoint = MeasurementPoint();
    m_previewPoint = MeasurementPoint();

    m_planeOrigin = QVector3D(0.0f, 0.0f, 0.0f);
    m_planeNormal = QVector3D();
}

bool Length2DMeasurement::mousePressEvent(OpenGLViewerWidget* viewer, QMouseEvent* event)
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

        if (!viewportPointToPlane(viewer, event->pos(), m_startPoint))
            return true;

        m_previewPoint = m_startPoint;
        m_state = MeasurementState::Collecting;

        viewer->update();
        return true;
    }

    if (m_state == MeasurementState::Collecting)
    {
        if (!viewportPointToPlane(viewer, event->pos(), m_endPoint))
            return true;

        m_previewPoint = MeasurementPoint();
        m_state = MeasurementState::Finished;

        viewer->update();
        return true;
    }

    return true;
}

bool Length2DMeasurement::mouseMoveEvent(OpenGLViewerWidget* viewer, QMouseEvent* event)
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

bool Length2DMeasurement::mouseReleaseEvent(OpenGLViewerWidget* viewer, QMouseEvent* event)
{
    Q_UNUSED(viewer);
    Q_UNUSED(event);

    return true;
}

bool Length2DMeasurement::keyPressEvent(OpenGLViewerWidget* viewer, QKeyEvent* event)
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

void Length2DMeasurement::drawOverlay(OpenGLViewerWidget* viewer, QPainter& painter) const
{
    if (viewer == 0 || !m_startPoint.valid)
        return;

    const MeasurementPoint* targetPoint = 0;

    if (m_state == MeasurementState::Collecting && m_previewPoint.valid)
        targetPoint = &m_previewPoint;
    else if (m_state == MeasurementState::Finished && m_endPoint.valid)
        targetPoint = &m_endPoint;

    if (targetPoint == 0)
        return;

    QPointF startPosition;
    QPointF endPosition;

    if (!worldPointToViewport(viewer, m_startPoint.worldPosition, startPosition))
        return;

    if (!worldPointToViewport(viewer, targetPoint->worldPosition, endPosition))
        return;

    const double length = static_cast<double>((targetPoint->worldPosition - m_startPoint.worldPosition).length());

    QPen linePen(QColor(255, 210, 40));
    linePen.setWidth(2);

    if (m_state == MeasurementState::Collecting)
        linePen.setStyle(Qt::DashLine);

    painter.setPen(linePen);
    painter.setBrush(QColor(255, 210, 40));

    painter.drawLine(startPosition, endPosition);
    painter.drawEllipse(startPosition, 4.0, 4.0);
    painter.drawEllipse(endPosition, 4.0, 4.0);

    const QPointF middlePosition((startPosition.x() + endPosition.x()) * 0.5, (startPosition.y() + endPosition.y()) * 0.5);

    const QString text = QStringLiteral("L=%1").arg(QString::number(length, 'f', 3));

    drawOverlayLabel(painter, middlePosition + QPointF(8.0, -28.0), text);
}

bool Length2DMeasurement::viewportPointToPlane(OpenGLViewerWidget* viewer, const QPoint& viewportPosition, MeasurementPoint& point) const
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

bool Length2DMeasurement::worldPointToViewport(OpenGLViewerWidget* viewer, const QVector3D& worldPosition, QPointF& viewportPosition) const
{
    if (viewer == 0)
        return false;

    const Camera* camera = viewer->cameraManager().activeCamera();

    if (camera == 0 || viewer->width() <= 0 || viewer->height() <= 0)
        return false;

    const float aspect = static_cast<float>(viewer->width()) / static_cast<float>(viewer->height());
    const QVector4D clip = camera->projectionMatrix(aspect) * camera->viewMatrix() * QVector4D(worldPosition, 1.0f);

    if (qAbs(clip.w()) <= 1.0e-8f)
        return false;

    if (camera->projectionType() == ProjectionType::Perspective && clip.w() <= 0.0f)
        return false;

    const float ndcX = clip.x() / clip.w();
    const float ndcY = clip.y() / clip.w();
    const float ndcZ = clip.z() / clip.w();

    if (ndcZ < -1.0f || ndcZ > 1.0f)
        return false;

    viewportPosition.setX((ndcX * 0.5f + 0.5f) * viewer->width());
    viewportPosition.setY((0.5f - ndcY * 0.5f) * viewer->height());

    return true;
}