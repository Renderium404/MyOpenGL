#include "Angle2DMeasurement.h"

#include <QDebug>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QtMath>

#include <cmath>
#include <vector>
#include <QVector2D>


#include "MyOpenGL/Camera/Camera.h"
#include "MyOpenGL/Item/RenderItem.h"
#include "MyOpenGL/Item/RenderPart.h"
#include "MyOpenGL/Material/Material.h"
#include "MyOpenGL/Resource/BufferGeometry.h"
#include "MyOpenGL/Viewer/OpenGLViewerWidget.h"
#include "MyOpenGL/Item/RenderLabel.h"
Angle2DMeasurement::Angle2DMeasurement()
    : m_state(MeasurementState::Idle)
    , m_pointCount(0)
    , m_hasCursorPosition(false)
    , m_planeOrigin(0.0f, 0.0f, 0.0f)
    , m_resultMaterial(0)
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
    m_currentPoint = MeasurementPoint();

    m_cursorPosition = QPointF();
    m_hasCursorPosition = false;

    m_planeOrigin = QVector3D(0.0f, 0.0f, 0.0f);
    m_planeNormal = QVector3D();
    m_planeXAxis = QVector3D();
    m_planeYAxis = QVector3D();
}

bool Angle2DMeasurement::mousePressEvent(OpenGLViewerWidget* viewer, QMouseEvent* event)
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

    if (m_state == MeasurementState::Idle || m_state == MeasurementState::Finished)
    {
        reset();

        m_cursorPosition = event->localPos();
        m_hasCursorPosition = true;

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

        m_firstPoint = point;
        m_currentPoint = point;
        m_pointCount = 1;
        m_state = MeasurementState::Collecting;

        viewer->update();
        return true;
    }

    if (m_pointCount == 1)
    {
        if (!viewportPointToPlane(viewer, event->localPos(), point))
        {
            m_currentPoint = MeasurementPoint();
            viewer->update();
            return true;
        }

        m_vertexPoint = point;
        m_currentPoint = point;
        m_pointCount = 2;

        viewer->update();
        return true;
    }

    if (m_pointCount == 2)
    {
        if (!viewportPointToPlane(viewer, event->localPos(), point))
        {
            m_currentPoint = MeasurementPoint();
            viewer->update();
            return true;
        }

        if (!commitResult(viewer, m_firstPoint, m_vertexPoint, point))
        {
            qWarning() << "Angle2DMeasurement mousePressEvent failed to commit result.";
            viewer->update();
            return true;
        }

        m_endPoint = point;
        m_currentPoint = point;
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

    m_cursorPosition = event->localPos();
    m_hasCursorPosition = true;

    const Camera* camera = viewer->cameraManager().activeCamera();

    if (camera == 0)
        return true;

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

    if (m_state != MeasurementState::Collecting || !m_firstPoint.valid)
        return;

    QPointF firstPosition;

    if (!viewer->worldPointAtScene(m_firstPoint.worldPosition, firstPosition))
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
    painter.drawEllipse(firstPosition, 4.0, 4.0);

    drawOverlayLabel(painter, firstPosition + QPointF(10.0, -30.0), QStringLiteral("P1=%1").arg(pointText(m_firstPoint)));

    if (m_pointCount == 1)
    {
        QPen previewPen(pointColor);
        previewPen.setWidth(2);
        previewPen.setStyle(Qt::DashLine);

        painter.setPen(previewPen);

        if (m_currentPoint.valid)
            painter.drawLine(firstPosition, currentPosition);

        painter.setBrush(pointColor);
        painter.drawEllipse(currentPosition, 4.0, 4.0);

        drawOverlayLabel(painter, currentPosition + QPointF(10.0, 10.0), QStringLiteral("P2=%1").arg(pointText(m_currentPoint)));
        drawOverlayLabel(painter, currentPosition + QPointF(10.0, 40.0), QStringLiteral("A=?"));
        return;
    }

    if (m_pointCount != 2 || !m_vertexPoint.valid)
        return;

    QPointF vertexPosition;

    if (!viewer->worldPointAtScene(m_vertexPoint.worldPosition, vertexPosition))
        return;

    QPen fixedPen(pointColor);
    fixedPen.setWidth(2);

    painter.setPen(fixedPen);
    painter.drawLine(vertexPosition, firstPosition);

    QPen previewPen(pointColor);
    previewPen.setWidth(2);
    previewPen.setStyle(Qt::DashLine);

    painter.setPen(previewPen);

    if (m_currentPoint.valid)
        painter.drawLine(vertexPosition, currentPosition);

    painter.setBrush(pointColor);
    painter.drawEllipse(vertexPosition, 4.0, 4.0);
    painter.drawEllipse(currentPosition, 4.0, 4.0);

    drawOverlayLabel(painter, vertexPosition + QPointF(10.0, 10.0), QStringLiteral("P2=%1").arg(pointText(m_vertexPoint)));
    drawOverlayLabel(painter, currentPosition + QPointF(10.0, 10.0), QStringLiteral("P3=%1").arg(pointText(m_currentPoint)));

    QString angleText = QStringLiteral("A=?");

    if (m_currentPoint.valid)
    {
        double angle = 0.0;

        if (angleValue(m_firstPoint.worldPosition, m_vertexPoint.worldPosition, m_currentPoint.worldPosition, angle))
            angleText = QStringLiteral("A=%1 %2").arg(QString::number(angle, 'f', 2)).arg(QChar(0x00B0));
    }

    drawOverlayLabel(painter, vertexPosition + QPointF(10.0, -30.0), angleText);
}

bool Angle2DMeasurement::viewportPointToPlane(OpenGLViewerWidget* viewer, const QPointF& viewportPosition, MeasurementPoint& point) const
{
    if (viewer == 0)
        return false;

    const Camera* camera = viewer->cameraManager().activeCamera();

    if (camera == 0 || viewer->width() <= 0 || viewer->height() <= 0)
        return false;

    if (m_planeNormal.lengthSquared() <= 1.0e-12f)
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

bool Angle2DMeasurement::ensureResultMaterial(OpenGLViewerWidget* viewer)
{
    if (viewer == 0)
        return false;

    if (m_resultMaterial != 0)
        return true;

    Material* material = viewer->materialManager().createMaterial("MeasurementAngle2DMaterial");

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

bool Angle2DMeasurement::commitResult(OpenGLViewerWidget* viewer, const MeasurementPoint& first, const MeasurementPoint& vertex, const MeasurementPoint& end)
{
    if (viewer == 0 || !first.valid || !vertex.valid || !end.valid)
        return false;

    if (!ensureResultMaterial(viewer))
        return false;

    const QVector3D firstOffset = first.worldPosition - m_planeOrigin;
    const QVector3D vertexOffset = vertex.worldPosition - m_planeOrigin;
    const QVector3D endOffset = end.worldPosition - m_planeOrigin;

    const QVector2D firstScene(QVector3D::dotProduct(firstOffset, m_planeXAxis), QVector3D::dotProduct(firstOffset, m_planeYAxis));
    const QVector2D vertexScene(QVector3D::dotProduct(vertexOffset, m_planeXAxis), QVector3D::dotProduct(vertexOffset, m_planeYAxis));
    const QVector2D endScene(QVector3D::dotProduct(endOffset, m_planeXAxis), QVector3D::dotProduct(endOffset, m_planeYAxis));

    RenderItem* item = viewer->measurementItemManager().createItem("MeasurementAngle2DResult");

    if (item == 0)
        return false;

    /// 保存共享线材质，删除测量 Item 时不能将该 Material 当作独占资源删除。
    item->setMaterial(m_resultMaterial);
    item->setDepthTestEnabled(false);

    BufferGeometry* geometry = new BufferGeometry("MeasurementAngle2DLine", BufferUsage::Static, RenderType::Lines);

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
        firstScene.x(),  firstScene.y(),  0.0f, color.x(), color.y(), color.z(),
        vertexScene.x(), vertexScene.y(), 0.0f, color.x(), color.y(), color.z(),
        vertexScene.x(), vertexScene.y(), 0.0f, color.x(), color.y(), color.z(),
        endScene.x(),    endScene.y(),    0.0f, color.x(), color.y(), color.z()
    };

    const std::vector<GLuint> indices = { 0, 1, 2, 3 };

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

    RenderLabel* firstLabel = item->createTextLabel(viewer->resourceManager(), viewer->materialManager(), QStringLiteral("P1=%1").arg(pointText(first)), textPixelSize);

    if (firstLabel != 0)
    {
        firstLabel->setAnchorWorld(m_planeOrigin);
        firstLabel->setAnchorSence(firstScene);
        firstLabel->setPixelOffset(QPointF(5.0, -15.0));
    }
    else
    {
        qWarning() << "Angle2DMeasurement commitResult failed to create P1 Label.";
    }

    RenderLabel* vertexLabel = item->createTextLabel(viewer->resourceManager(), viewer->materialManager(), QStringLiteral("P2=%1").arg(pointText(vertex)), textPixelSize);

    if (vertexLabel != 0)
    {
        vertexLabel->setAnchorWorld(m_planeOrigin);
        vertexLabel->setAnchorSence(vertexScene);
        vertexLabel->setPixelOffset(QPointF(5.0, 5.0));
    }
    else
    {
        qWarning() << "Angle2DMeasurement commitResult failed to create P2 Label.";
    }

    RenderLabel* endLabel = item->createTextLabel(viewer->resourceManager(), viewer->materialManager(), QStringLiteral("P3=%1").arg(pointText(end)), textPixelSize);

    if (endLabel != 0)
    {
        endLabel->setAnchorWorld(m_planeOrigin);
        endLabel->setAnchorSence(endScene);
        endLabel->setPixelOffset(QPointF(5.0, 5.0));
    }
    else
    {
        qWarning() << "Angle2DMeasurement commitResult failed to create P3 Label.";
    }

    double angle = 0.0;
    QString angleText = QStringLiteral("A=?");

    if (angleValue(first.worldPosition, vertex.worldPosition, end.worldPosition, angle))
        angleText = QStringLiteral("A=%1 %2").arg(QString::number(angle, 'f', 2)).arg(QChar(0x00B0));

    RenderLabel* angleLabel = item->createTextLabel(viewer->resourceManager(), viewer->materialManager(), angleText, textPixelSize);

    if (angleLabel != 0)
    {
        angleLabel->setAnchorWorld(m_planeOrigin);
        angleLabel->setAnchorSence(vertexScene);
        angleLabel->setPixelOffset(QPointF(5.0, -15.0));
    }
    else
    {
        qWarning() << "Angle2DMeasurement commitResult failed to create Angle Label.";
    }

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

QString Angle2DMeasurement::pointText(const MeasurementPoint& point) const
{
    if (!point.valid)
        return QStringLiteral("(?, ?)");

    const QVector3D offset = point.worldPosition - m_planeOrigin;
    const double x = QVector3D::dotProduct(offset, m_planeXAxis);
    const double y = QVector3D::dotProduct(offset, m_planeYAxis);

    return QStringLiteral("(%1, %2)").arg(QString::number(x, 'f', 3)).arg(QString::number(y, 'f', 3));
}