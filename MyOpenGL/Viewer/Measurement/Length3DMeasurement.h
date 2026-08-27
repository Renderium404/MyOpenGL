#ifndef LENGTH3DMEASUREMENT_H
#define LENGTH3DMEASUREMENT_H

#include <QString>

#include "MyOpenGL/Viewer/Measurement/MeasurementTool.h"

class Material;

class Length3DMeasurement : public MeasurementTool
{
public:
    Length3DMeasurement();

    MeasurementType type() const override;

    void reset() override;

    bool mousePressEvent(OpenGLViewerWidget* viewer, QMouseEvent* event) override;
    bool mouseMoveEvent(OpenGLViewerWidget* viewer, QMouseEvent* event) override;
    bool mouseReleaseEvent(OpenGLViewerWidget* viewer, QMouseEvent* event) override;

    void drawOverlay(OpenGLViewerWidget* viewer, QPainter& painter) const override;

private:
    bool viewportPointToScene(OpenGLViewerWidget* viewer, const QPointF& viewportPosition, MeasurementPoint& point) const;
    bool ensureResultMaterial(OpenGLViewerWidget* viewer);
    bool commitResult(OpenGLViewerWidget* viewer, const MeasurementPoint& start, const MeasurementPoint& end);
    QString pointText(const MeasurementPoint& point) const;

private:
    MeasurementPoint m_startPoint;
    MeasurementPoint m_endPoint;
    MeasurementPoint m_currentPoint;

    QPointF m_cursorPosition;
    bool m_hasCursorPosition;

    Material* m_resultMaterial;
};

#endif // LENGTH3DMEASUREMENT_H