#ifndef ANGLE2DMEASUREMENT_H
#define ANGLE2DMEASUREMENT_H

#include <QString>

#include "MyOpenGL/Viewer/Measurement/MeasurementTool.h"

class Material;

class Angle2DMeasurement : public MeasurementTool
{
public:
    Angle2DMeasurement();

    MeasurementType type() const override;
    MeasurementState state() const override;

    void reset() override;

    bool mousePressEvent(OpenGLViewerWidget* viewer, QMouseEvent* event) override;
    bool mouseMoveEvent(OpenGLViewerWidget* viewer, QMouseEvent* event) override;
    bool mouseReleaseEvent(OpenGLViewerWidget* viewer, QMouseEvent* event) override;
    bool keyPressEvent(OpenGLViewerWidget* viewer, QKeyEvent* event) override;

    void drawOverlay(OpenGLViewerWidget* viewer, QPainter& painter) const override;

private:
    bool viewportPointToPlane(OpenGLViewerWidget* viewer, const QPointF& viewportPosition, MeasurementPoint& point) const;
    bool ensureResultMaterial(OpenGLViewerWidget* viewer);
    bool commitResult(OpenGLViewerWidget* viewer, const MeasurementPoint& first, const MeasurementPoint& vertex, const MeasurementPoint& end);
    bool angleValue(const QVector3D& first, const QVector3D& vertex, const QVector3D& end, double& angle) const;
    QString pointText(const MeasurementPoint& point) const;

private:
    MeasurementState m_state;
    int m_pointCount;

    MeasurementPoint m_firstPoint;
    MeasurementPoint m_vertexPoint;
    MeasurementPoint m_endPoint;
    MeasurementPoint m_currentPoint;

    QPointF m_cursorPosition;
    bool m_hasCursorPosition;

    QVector3D m_planeOrigin;
    QVector3D m_planeNormal;
    QVector3D m_planeXAxis;
    QVector3D m_planeYAxis;

    Material* m_resultMaterial;
};

#endif // ANGLE2DMEASUREMENT_H