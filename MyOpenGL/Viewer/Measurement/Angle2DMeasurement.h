#ifndef ANGLE2DMEASUREMENT_H
#define ANGLE2DMEASUREMENT_H

#include "MyOpenGL/Viewer/Measurement/MeasurementTool.h"

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
    bool viewportPointToPlane(OpenGLViewerWidget* viewer, const QPoint& viewportPosition, MeasurementPoint& point) const;
    bool angleValue(const QVector3D& first, const QVector3D& vertex, const QVector3D& end, double& angle) const;

private:
    MeasurementState m_state;
    int m_pointCount;

    MeasurementPoint m_firstPoint;
    MeasurementPoint m_vertexPoint;
    MeasurementPoint m_endPoint;
    MeasurementPoint m_previewPoint;

    QVector3D m_planeOrigin;
    QVector3D m_planeNormal;
};

#endif // ANGLE2DMEASUREMENT_H