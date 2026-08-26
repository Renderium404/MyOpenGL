#include "Length2DMeasurement.h"

#include <QDebug>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>

#include <vector>
#include <QVector2D>

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

    const QVector3D startOffset = start.worldPosition - m_planeOrigin;
    const QVector3D endOffset = end.worldPosition - m_planeOrigin;

    const QVector2D startScene(QVector3D::dotProduct(startOffset, m_planeXAxis), QVector3D::dotProduct(startOffset, m_planeYAxis));
    const QVector2D endScene(QVector3D::dotProduct(endOffset, m_planeXAxis), QVector3D::dotProduct(endOffset, m_planeYAxis));
    const QVector2D middleScene = (startScene + endScene) * 0.5f;

    RenderItem* item = viewer->measurementItemManager().createItem("MeasurementLength2DResult");

    if (item == 0)
        return false;

    /// 保存共享线材质，删除测量 Item 时不能将该 Material 当作独占 Label Material 删除。
    item->setMaterial(m_resultMaterial);
    item->setDepthTestEnabled(false);

    /// 测量线 Geometry 使用二维标尺坐标，不使用 Pixel。
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

    const QVector3D color(1.0f, 0.82f, 0.16f); // 测量结果统一使用黄色。

    const std::vector<GLfloat> vertices =
    {
        startScene.x(), startScene.y(), 0.0f, color.x(), color.y(), color.z(),
        endScene.x(),   endScene.y(),   0.0f, color.x(), color.y(), color.z()
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

    lineLabel->setAnchorWorld(m_planeOrigin);
    lineLabel->setAnchorSence(QVector2D(0.0f, 0.0f));
    lineLabel->setPixelOffset(QPointF(0.0, 0.0));
    lineLabel->setGeometry(geometry);
    lineLabel->setMaterial(m_resultMaterial);
    lineLabel->setVisible(true);

    const int textPixelSize = 16; // 测量结果统一文本字号。

    RenderLabel* startLabel = item->createTextLabel(viewer->resourceManager(), viewer->materialManager(), QStringLiteral("P1=%1").arg(pointText(start)), textPixelSize);

    if (startLabel != 0)
    {
        startLabel->setAnchorWorld(m_planeOrigin);
        startLabel->setAnchorSence(startScene);
        startLabel->setPixelOffset(QPointF(5.0, -15.0));
    }
    else
    {
        qWarning() << "Length2DMeasurement commitResult failed to create P1 Label.";
    }

    RenderLabel* endLabel = item->createTextLabel(viewer->resourceManager(), viewer->materialManager(), QStringLiteral("P2=%1").arg(pointText(end)), textPixelSize);

    if (endLabel != 0)
    {
        endLabel->setAnchorWorld(m_planeOrigin);
        endLabel->setAnchorSence(endScene);
        endLabel->setPixelOffset(QPointF(5.0, 5.0));
    }
    else
    {
        qWarning() << "Length2DMeasurement commitResult failed to create P2 Label.";
    }

    const double length = static_cast<double>((end.worldPosition - start.worldPosition).length());
    const QString lengthText = QStringLiteral("L=%1").arg(QString::number(length, 'f', 3));

    RenderLabel* lengthLabel = item->createTextLabel(viewer->resourceManager(), viewer->materialManager(), lengthText, textPixelSize);

    if (lengthLabel != 0)
    {
        lengthLabel->setAnchorWorld(m_planeOrigin);
        lengthLabel->setAnchorSence(middleScene);
        lengthLabel->setPixelOffset(QPointF(5.0, -15.0));
    }
    else
    {
        qWarning() << "Length2DMeasurement commitResult failed to create Length Label.";
    }

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