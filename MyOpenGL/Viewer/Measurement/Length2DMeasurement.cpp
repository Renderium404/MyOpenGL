#include "Length2DMeasurement.h"

#include <QDebug>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>

#include <vector>

#include "MyOpenGL/Camera/Camera.h"
#include "MyOpenGL/Item/RenderItem.h"
#include "MyOpenGL/Item/RenderPart.h"
#include "MyOpenGL/Item/RenderLabel.h"
#include "MyOpenGL/Material/Material.h"
#include "MyOpenGL/Resource/BufferGeometry.h"
#include "MyOpenGL/Viewer/OpenGLViewerWidget.h"

Length2DMeasurement::Length2DMeasurement()
    : m_state(MeasurementState::Idle)
    , m_hasCursorPosition(false)
    , m_planeOrigin(0.0f, 0.0f, 0.0f)
    , m_resultMaterial(0)
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
    m_currentPoint = MeasurementPoint();

    m_cursorPosition = QPointF();
    m_hasCursorPosition = false;

    m_planeOrigin = QVector3D(0.0f, 0.0f, 0.0f);
    m_planeNormal = QVector3D();
    m_planeXAxis = QVector3D();
    m_planeYAxis = QVector3D();
}

bool Length2DMeasurement::mousePressEvent(OpenGLViewerWidget* viewer, QMouseEvent* event)
{
    if (viewer == 0 || event == 0)
        return false;

    m_cursorPosition = event->localPos();
    m_hasCursorPosition = true;

    if (event->button() != Qt::LeftButton)
        return true;

    const Camera* camera = viewer->cameraManager().activeCamera();

    if (camera == 0)
        return true;

    MeasurementPoint point;

    /// 开始新一轮测量。
    if (m_state == MeasurementState::Idle || m_state == MeasurementState::Finished)
    {
        reset();

        m_cursorPosition = event->localPos();
        m_hasCursorPosition = true;

        /// 本轮二维测量固定使用当前 Camera 对应的测量平面。
        m_planeOrigin = QVector3D(0.0f, 0.0f, 0.0f);
        m_planeNormal = camera->forward().normalized();
        m_planeXAxis = camera->right().normalized();
        m_planeYAxis = camera->up().normalized();

        if (!viewportPointToPlane(viewer, event->localPos(), point))
        {
            m_currentPoint = MeasurementPoint();

            viewer->update();
            return true;
        }

        m_startPoint = point;
        m_currentPoint = point;
        m_state = MeasurementState::Collecting;

        viewer->update();
        return true;
    }

    /// 确定终点并提交持久化结果。
    if (m_state == MeasurementState::Collecting)
    {
        if (!viewportPointToPlane(viewer, event->localPos(), point))
        {
            m_currentPoint = MeasurementPoint();

            viewer->update();
            return true;
        }

        if (!commitResult(viewer, m_startPoint, point))
        {
            qWarning() << "Length2DMeasurement mousePressEvent failed to commit result.";

            viewer->update();
            return true;
        }

        m_currentPoint = point;
        m_endPoint = point;
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

    m_cursorPosition = event->localPos();
    m_hasCursorPosition = true;

    const Camera* camera = viewer->cameraManager().activeCamera();

    if (camera == 0)
        return true;

    /// 尚未开始时，二维测量平面跟随 Camera。
    if (m_state == MeasurementState::Idle)
    {
        m_planeOrigin = QVector3D(0.0f, 0.0f, 0.0f);
        m_planeNormal = camera->forward().normalized();
        m_planeXAxis = camera->right().normalized();
        m_planeYAxis = camera->up().normalized();
    }

    MeasurementPoint point;

    if (viewportPointToPlane(viewer, event->localPos(), point))
        m_currentPoint = point;
    else
        m_currentPoint = MeasurementPoint();

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
    if (viewer == 0)
        return;

    const QColor pointColor(255, 210, 40);

    /// ------------------------------------------------
    /// Idle
    /// ------------------------------------------------
    ///
    /// 尚未开始：
    /// 显示当前可选择的 P1。

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

        drawOverlayLabel(
            painter,
            currentPosition + QPointF(10.0, -30.0),
            QStringLiteral("P1=%1").arg(pointText(m_currentPoint)));

        return;
    }

    /// ------------------------------------------------
    /// Finished
    /// ------------------------------------------------
    ///
    /// 已完成的：
    ///
    /// Line
    /// P1
    /// P2
    /// Length
    ///
    /// 已经全部提交到 measurementItemManager。
    ///
    /// QPainter 这里只保留当前鼠标位置提示。

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

        drawOverlayLabel(
            painter,
            currentPosition + QPointF(10.0, -30.0),
            QStringLiteral("P=%1").arg(pointText(m_currentPoint)));

        return;
    }

    /// ------------------------------------------------
    /// Collecting
    /// ------------------------------------------------

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

    /// 临时测量线。

    QPen linePen(pointColor);
    linePen.setWidth(2);
    linePen.setStyle(Qt::DashLine);

    painter.setPen(linePen);
    painter.setBrush(pointColor);

    if (m_currentPoint.valid)
        painter.drawLine(startPosition, currentPosition);

    /// 临时测量点。

    painter.drawEllipse(startPosition, 4.0, 4.0);
    painter.drawEllipse(currentPosition, 4.0, 4.0);

    /// P1。

    drawOverlayLabel(
        painter,
        startPosition + QPointF(8.0, -28.0),
        QStringLiteral("P1=%1").arg(pointText(m_startPoint)));

    /// P2。

    drawOverlayLabel(
        painter,
        currentPosition + QPointF(8.0, 10.0),
        QStringLiteral("P2=%1").arg(pointText(m_currentPoint)));

    /// 当前长度。

    QString lengthText = QStringLiteral("L=?");

    if (m_currentPoint.valid)
    {
        const double length =
            static_cast<double>(
                (m_currentPoint.worldPosition -
                 m_startPoint.worldPosition).length());

        lengthText =
            QStringLiteral("L=%1")
                .arg(QString::number(length, 'f', 3));
    }

    const QPointF middlePosition(
        (startPosition.x() + currentPosition.x()) * 0.5,
        (startPosition.y() + currentPosition.y()) * 0.5);

    drawOverlayLabel(
        painter,
        middlePosition + QPointF(8.0, -28.0),
        lengthText);
}

bool Length2DMeasurement::viewportPointToPlane(
    OpenGLViewerWidget* viewer,
    const QPointF& viewportPosition,
    MeasurementPoint& point) const
{
    if (viewer == 0)
        return false;

    const Camera* camera =
        viewer->cameraManager().activeCamera();

    if (camera == 0 ||
        viewer->width() <= 0 ||
        viewer->height() <= 0)
    {
        return false;
    }

    if (m_planeNormal.lengthSquared() <= 1.0e-12f)
        return false;

    QVector3D rayOrigin;
    QVector3D rayDirection;

    if (!camera->screenPointToRay(
            viewportPosition.x(),
            viewportPosition.y(),
            viewer->width(),
            viewer->height(),
            rayOrigin,
            rayDirection))
    {
        return false;
    }

    const float denominator =
        QVector3D::dotProduct(
            rayDirection,
            m_planeNormal);

    if (qAbs(denominator) <= 1.0e-8f)
        return false;

    const float distance =
        QVector3D::dotProduct(
            m_planeOrigin - rayOrigin,
            m_planeNormal) /
        denominator;

    if (distance < 0.0f)
        return false;

    point.viewportPosition = viewportPosition;
    point.worldPosition =
        rayOrigin +
        rayDirection * distance;

    point.valid = true;

    return true;
}

bool Length2DMeasurement::ensureResultMaterial(OpenGLViewerWidget* viewer)
{
    if (viewer == 0)
        return false;

    if (m_resultMaterial != 0)
        return true;

    m_resultMaterial =
        viewer->materialManager().createMaterial(
            "MeasurementLength2DMaterial");

    if (m_resultMaterial == 0)
        return false;

    if (!m_resultMaterial->setSurfaceMode(
            SurfaceMode::VertexColor))
    {
        return false;
    }

    m_resultMaterial->setLightingEnabled(false);

    return true;
}

bool Length2DMeasurement::commitResult(OpenGLViewerWidget* viewer, const MeasurementPoint& start, const MeasurementPoint& end)
{
    if (viewer == 0 || !start.valid || !end.valid)
        return false;

    if (!ensureResultMaterial(viewer))
        return false;

    /// 世界原点在当前视口中的投影就是二维标尺原点。
    QPointF originPosition;

    if (!viewer->worldPointAtScene(m_planeOrigin, originPosition))
        return false;

    /// 测量点相对于二维标尺原点的屏幕 Pixel 偏移。
    const QPointF startOffset = start.viewportPosition - originPosition;
    const QPointF endOffset = end.viewportPosition - originPosition;
    const QPointF middleOffset = (start.viewportPosition + end.viewportPosition) * 0.5 - originPosition;

    /// ------------------------------------------------
    /// Result Item
    /// ------------------------------------------------

    RenderItem* item = viewer->measurementItemManager().createItem("MeasurementLength2DResult");

    if (item == 0)
        return false;

    item->setMaterial(m_resultMaterial);
    item->setDepthTestEnabled(false);

    /// ------------------------------------------------
    /// Line Geometry Label
    /// ------------------------------------------------
    ///
    /// RenderLabel Geometry 使用 OpenGL Screen Pixel：
    /// +X 向右，+Y 向上。
    ///
    /// startOffset / endOffset 是 Qt Viewport：
    /// +X 向右，+Y 向下。
    ///
    /// 因此 Geometry Y 取反。

    BufferGeometry* geometry = new BufferGeometry("MeasurementLength2DLine", BufferUsage::Static, RenderType::Lines);

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

    const float startX = static_cast<float>(startOffset.x());
    const float startY = -static_cast<float>(startOffset.y());
    const float endX = static_cast<float>(endOffset.x());
    const float endY = -static_cast<float>(endOffset.y());

    const std::vector<GLfloat> vertices =
    {
        startX, startY, 0.0f, color.x(), color.y(), color.z(),
        endX,   endY,   0.0f, color.x(), color.y(), color.z()
    };

    const std::vector<GLuint> indices = { 0, 1 };

    geometry->setVertexData(vertices);
    geometry->setIndexData(indices);

    if (viewer->resourceManager().adopt(geometry) == InvalidResourceId)
    {
        delete geometry;
        viewer->measurementItemManager().remove(item->id());
        return false;
    }

    RenderLabel* lineLabel = item->createLabel();

    if (lineLabel == 0)
    {
        viewer->resourceManager().remove(geometry->id());
        viewer->measurementItemManager().remove(item->id());
        return false;
    }

    /// Geometry 坐标已经是相对于标尺原点的 Pixel 坐标。
    lineLabel->setAnchorPosition(m_planeOrigin);
    lineLabel->setPixelOffset(QPointF(0.0, 0.0));
    lineLabel->setGeometry(geometry);
    lineLabel->setMaterial(m_resultMaterial);

    /// ------------------------------------------------
    /// P1 Text Label
    /// ------------------------------------------------

    const QString startText = QStringLiteral("P1=%1").arg(pointText(start));

    if (createPersistentLabel(viewer, item, m_planeOrigin, startOffset + QPointF(5.0, -15.0), startText) == 0)
        qWarning() << "Length2DMeasurement commitResult failed to create P1 Label.";

    /// ------------------------------------------------
    /// P2 Text Label
    /// ------------------------------------------------

    const QString endText = QStringLiteral("P2=%1").arg(pointText(end));

    if (createPersistentLabel(viewer, item, m_planeOrigin, endOffset + QPointF(5.0, 5.0), endText) == 0)
        qWarning() << "Length2DMeasurement commitResult failed to create P2 Label.";

    /// ------------------------------------------------
    /// Length Text Label
    /// ------------------------------------------------

    const double length = static_cast<double>((end.worldPosition - start.worldPosition).length());
    const QString lengthText = QStringLiteral("L=%1").arg(QString::number(length, 'f', 3));

    if (createPersistentLabel(viewer, item, m_planeOrigin, middleOffset + QPointF(5.0, -15.0), lengthText) == 0)
        qWarning() << "Length2DMeasurement commitResult failed to create Length Label.";

    return true;
}
QString Length2DMeasurement::pointText(
    const MeasurementPoint& point) const
{
    if (!point.valid)
        return QStringLiteral("(?, ?)");

    const QVector3D offset =
        point.worldPosition -
        m_planeOrigin;

    const double x =
        QVector3D::dotProduct(
            offset,
            m_planeXAxis);

    const double y =
        QVector3D::dotProduct(
            offset,
            m_planeYAxis);

    return QStringLiteral("(%1, %2)")
        .arg(QString::number(x, 'f', 3))
        .arg(QString::number(y, 'f', 3));
}