#ifndef ANGLE3DMEASUREMENT_H
#define ANGLE3DMEASUREMENT_H

#include <QString>

#include "MyOpenGL/Viewer/Measurement/MeasurementTool.h"

class Material;

class Angle3DMeasurement : public MeasurementTool
{
public:
    Angle3DMeasurement();

    MeasurementType type() const override;
    MeasurementState state() const override;

    void reset() override;

    bool mousePressEvent(OpenGLViewerWidget* viewer, QMouseEvent* event) override;
    bool mouseMoveEvent(OpenGLViewerWidget* viewer, QMouseEvent* event) override;
    bool mouseReleaseEvent(OpenGLViewerWidget* viewer, QMouseEvent* event) override;
    bool keyPressEvent(OpenGLViewerWidget* viewer, QKeyEvent* event) override;

    void drawOverlay(OpenGLViewerWidget* viewer, QPainter& painter) const override;

private:
    bool viewportPointToScene(OpenGLViewerWidget* viewer, const QPointF& viewportPosition, MeasurementPoint& point) const;
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

    Material* m_resultMaterial;
};

#endif // ANGLE3DMEASUREMENT_H