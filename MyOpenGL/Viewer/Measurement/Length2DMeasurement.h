#ifndef LENGTH2DMEASUREMENT_H
#define LENGTH2DMEASUREMENT_H

#include "MeasurementTool.h"

class Length2DMeasurement : public MeasurementTool
{
public:
    Length2DMeasurement();

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
    bool worldPointToViewport(OpenGLViewerWidget* viewer, const QVector3D& worldPosition, QPointF& viewportPosition) const;

private:
    MeasurementState m_state;

    MeasurementPoint m_startPoint;
    MeasurementPoint m_endPoint;
    MeasurementPoint m_previewPoint;

    QVector3D m_planeOrigin;
    QVector3D m_planeNormal;
};

#endif // LENGTH2DMEASUREMENT_H