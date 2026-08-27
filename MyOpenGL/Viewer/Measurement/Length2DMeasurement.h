#ifndef LENGTH2DMEASUREMENT_H
#define LENGTH2DMEASUREMENT_H

#include <QPointF>
#include <QString>

#include "MeasurementTool.h"

class Material;

class Length2DMeasurement : public MeasurementTool
{
public:
    Length2DMeasurement();

    MeasurementType type() const override;

    void reset() override;

    bool mousePressEvent(OpenGLViewerWidget* viewer, QMouseEvent* event) override;
    bool mouseMoveEvent(OpenGLViewerWidget* viewer, QMouseEvent* event) override;
    bool mouseReleaseEvent(OpenGLViewerWidget* viewer, QMouseEvent* event) override;
    void drawOverlay(OpenGLViewerWidget* viewer, QPainter& painter) const override;

private:
    bool viewportPointToPlane(OpenGLViewerWidget* viewer, const QPointF& viewportPosition, MeasurementPoint& point) const;

    bool ensureResultMaterial(OpenGLViewerWidget* viewer);
    bool commitResult(OpenGLViewerWidget* viewer, const MeasurementPoint& start, const MeasurementPoint& end);

    QString pointText(const MeasurementPoint& point) const;

private:
    MeasurementPoint m_startPoint;
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

#endif // LENGTH2DMEASUREMENT_H
