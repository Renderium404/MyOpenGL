#include "Angle3DMeasurement.h"

#include <QColor>
#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QtMath>
#include <QVector2D>

#include <cmath>
#include <vector>

#include "MyOpenGL/Item/RenderItem.h"
#include "MyOpenGL/Item/RenderPart.h"
#include "MyOpenGL/Material/Material.h"
#include "MyOpenGL/Resource/BufferGeometry.h"
#include "MyOpenGL/Viewer/Modeling/SimpleModeling.h"
#include "MyOpenGL/Viewer/OpenGLViewerWidget.h"

Angle3DMeasurement::Angle3DMeasurement()
    : m_pointCount(0)
    , m_hasCursorPosition(false)
    , m_resultMaterial(0)
{
}

/// 基本信息

MeasurementType Angle3DMeasurement::type() const
{
    return MeasurementType::Angle3D;
}

/// 交互

void Angle3DMeasurement::reset()
{
    setState(MeasurementState::Idle);
    m_pointCount = 0;

    m_vertexPoint = MeasurementPoint();
    m_firstPoint = MeasurementPoint();
    m_endPoint = MeasurementPoint();
    m_currentPoint = MeasurementPoint();

    m_cursorPosition = QPointF();
    m_hasCursorPosition = false;
}

bool Angle3DMeasurement::mousePressEvent(OpenGLViewerWidget* viewer, QMouseEvent* event)
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

    /// 第一点 P1：角度顶点。
    if (state() == MeasurementState::Idle || state() == MeasurementState::Finished)
    {
        reset();

        m_cursorPosition = event->localPos();
        m_hasCursorPosition = true;

        m_vertexPoint = point;
        m_currentPoint = point;
        m_pointCount = 1;
        setState(MeasurementState::Collecting);

        viewer->update();
        return true;
    }

    /// 第二点 P2：第一条边端点。
    if (m_pointCount == 1)
    {
        m_firstPoint = point;
        m_pointCount = 2;

        viewer->update();
        return true;
    }

    /// 第三点 P3：第二条边端点。
    if (m_pointCount == 2)
    {
        if (!commitResult(viewer, m_vertexPoint, m_firstPoint, point))
        {
            qWarning() << "Angle3DMeasurement mousePressEvent failed to commit result.";
            viewer->update();
            return true;
        }

        m_endPoint = point;
        m_pointCount = 3;
        setState(MeasurementState::Finished);

        viewer->update();
        return true;
    }

    return true;
}

bool Angle3DMeasurement::mouseMoveEvent(OpenGLViewerWidget* viewer, QMouseEvent* event)
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

bool Angle3DMeasurement::mouseReleaseEvent(OpenGLViewerWidget* viewer, QMouseEvent* event)
{
    Q_UNUSED(viewer);
    Q_UNUSED(event);

    return true;
}

/// Overlay

void Angle3DMeasurement::drawOverlay(OpenGLViewerWidget* viewer, QPainter& painter) const
{
    if (viewer == 0)
        return;

    const QVector4D& measurementColor = lineColor();
    const QColor pointColor = QColor::fromRgbF(qBound(0.0f, measurementColor.x(), 1.0f), qBound(0.0f, measurementColor.y(), 1.0f), qBound(0.0f, measurementColor.z(), 1.0f));

    /// 尚未开始时显示即将选择的 P1。
    if (state() == MeasurementState::Idle || state() == MeasurementState::Finished)
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

    if (state() != MeasurementState::Collecting || !m_vertexPoint.valid)
        return;

    QPointF vertexPosition;

    if (!viewer->worldPointAtScene(m_vertexPoint.worldPosition, vertexPosition))
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
    painter.drawEllipse(vertexPosition, 4.0, 4.0);

    drawOverlayLabel(painter, vertexPosition + QPointF(10.0, 10.0), QStringLiteral("P1=%1").arg(pointText(m_vertexPoint)));

    /// 正在确定 P2。
    if (m_pointCount == 1)
    {
        QPen previewPen(pointColor);
        previewPen.setWidthF(lineWidth());
        previewPen.setStyle(Qt::DashLine);

        painter.setPen(previewPen);

        if (m_currentPoint.valid)
            painter.drawLine(vertexPosition, currentPosition);

        painter.setBrush(pointColor);
        painter.drawEllipse(currentPosition, 4.0, 4.0);

        drawOverlayLabel(painter, currentPosition + QPointF(10.0, 10.0), QStringLiteral("P2=%1").arg(pointText(m_currentPoint)));
        return;
    }

    if (m_pointCount != 2 || !m_firstPoint.valid)
        return;

    QPointF firstPosition;

    if (!viewer->worldPointAtScene(m_firstPoint.worldPosition, firstPosition))
        return;

    /// 已确定的第一条边。
    QPen fixedPen(pointColor);
    fixedPen.setWidthF(lineWidth());

    painter.setPen(fixedPen);
    painter.drawLine(vertexPosition, firstPosition);

    /// 当前正在确定的第二条边。
    QPen previewPen(pointColor);
    previewPen.setWidthF(lineWidth());
    previewPen.setStyle(Qt::DashLine);

    painter.setPen(previewPen);

    if (m_currentPoint.valid)
        painter.drawLine(vertexPosition, currentPosition);

    painter.setBrush(pointColor);
    painter.drawEllipse(firstPosition, 4.0, 4.0);
    painter.drawEllipse(currentPosition, 4.0, 4.0);

    drawOverlayLabel(painter, firstPosition + QPointF(10.0, 10.0), QStringLiteral("P2=%1").arg(pointText(m_firstPoint)));
    drawOverlayLabel(painter, currentPosition + QPointF(10.0, 10.0), QStringLiteral("P3=%1").arg(pointText(m_currentPoint)));

    QString angleText = QStringLiteral("A=?");

    if (m_currentPoint.valid)
    {
        double angle = 0.0;

        if (angleValue(m_vertexPoint.worldPosition, m_firstPoint.worldPosition, m_currentPoint.worldPosition, angle))
            angleText = QStringLiteral("A=%1 %2").arg(QString::number(angle, 'f', 2)).arg(QChar(0x00B0));
    }

    drawOverlayLabel(painter, vertexPosition + QPointF(10.0, -30.0), angleText);
}

/// 坐标获取

bool Angle3DMeasurement::viewportPointToScene(OpenGLViewerWidget* viewer, const QPointF& viewportPosition, MeasurementPoint& point) const
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

/// 结果资源

bool Angle3DMeasurement::ensureResultMaterial(OpenGLViewerWidget* viewer)
{
    if (viewer == 0)
        return false;

    if (m_resultMaterial != 0)
        return true;

    Material* material = viewer->materialManager().createMaterial("MeasurementAngle3DMaterial");

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

bool Angle3DMeasurement::commitResult(OpenGLViewerWidget* viewer, const MeasurementPoint& vertex, const MeasurementPoint& first, const MeasurementPoint& end)
{
    if (viewer == 0 || !vertex.valid || !first.valid || !end.valid)
        return false;

    if (!ensureResultMaterial(viewer))
        return false;

    const QVector3D& p1 = vertex.worldPosition;
    const QVector3D& p2 = first.worldPosition;
    const QVector3D& p3 = end.worldPosition;

    const QVector3D firstDirection = p2 - p1;
    const QVector3D endDirection = p3 - p1;

    if (firstDirection.lengthSquared() <= 1.0e-12f || endDirection.lengthSquared() <= 1.0e-12f)
        return false;

    RenderItem* item = viewer->measurementItemManager().createItem("MeasurementAngle3DResult");

    if (item == 0)
        return false;

    item->setMaterial(m_resultMaterial);
    item->setDepthTestEnabled(false);
    item->setDepthWriteEnabled(false);

    const QVector4D& measurementColor = lineColor();
    const QVector3D geometryColor(measurementColor.x(), measurementColor.y(), measurementColor.z());

    /// 两条角度边均从 P1 顶点出发。
    const std::vector<QVector3D> linePoints =
    {
        p1, p2,
        p1, p3
    };

    BufferGeometry* lineGeometry = SimpleModeling::createLines("MeasurementAngle3DLine", linePoints, geometryColor, lineWidth());

    if (lineGeometry != 0 && viewer->resourceManager().adopt(lineGeometry) != InvalidResourceId)
    {
        RenderPart* linePart = item->createPart();

        if (linePart != 0)
            linePart->setGeometry(lineGeometry);
        else
            viewer->resourceManager().remove(lineGeometry->id());
    }
    else
    {
        delete lineGeometry;
    }

    /// 圆弧半径使用较短测量边长度的四分之一。
    const float arcRadius = qMin(firstDirection.length(), endDirection.length()) * 0.25f;

    BufferGeometry* arcGeometry = SimpleModeling::createArc("MeasurementAngle3DArc", p1, firstDirection, endDirection, arcRadius, geometryColor, lineWidth());

    if (arcGeometry != 0 && viewer->resourceManager().adopt(arcGeometry) != InvalidResourceId)
    {
        RenderPart* arcPart = item->createPart();

        if (arcPart != 0)
            arcPart->setGeometry(arcGeometry);
        else
            viewer->resourceManager().remove(arcGeometry->id());
    }
    else
    {
        delete arcGeometry;
    }

    const QVector2D sceneAnchor(0.0f, 0.0f);

    createPersistentLabel(viewer, item, p1, sceneAnchor, QPointF(5.0, 5.0), QStringLiteral("P1=%1").arg(pointText(vertex)));
    createPersistentLabel(viewer, item, p2, sceneAnchor, QPointF(5.0, -15.0), QStringLiteral("P2=%1").arg(pointText(first)));
    createPersistentLabel(viewer, item, p3, sceneAnchor, QPointF(5.0, 5.0), QStringLiteral("P3=%1").arg(pointText(end)));

    double angle = 0.0;
    QString angleText = QStringLiteral("A=?");

    if (angleValue(p1, p2, p3, angle))
        angleText = QStringLiteral("A=%1 %2").arg(QString::number(angle, 'f', 2)).arg(QChar(0x00B0));

    createPersistentLabel(viewer, item, p1, sceneAnchor, QPointF(5.0, -15.0), angleText);
    return true;
}

/// 计算

bool Angle3DMeasurement::angleValue(const QVector3D& vertex, const QVector3D& first, const QVector3D& end, double& angle) const
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