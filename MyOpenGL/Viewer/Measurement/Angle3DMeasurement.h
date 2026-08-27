#ifndef ANGLE3DMEASUREMENT_H
#define ANGLE3DMEASUREMENT_H

#include <QString>

#include "MyOpenGL/Viewer/Measurement/MeasurementTool.h"

class Material;

/// 三维角度测量。
/// 第一点为角度顶点，第二点和第三点分别确定两条测量边。
class Angle3DMeasurement : public MeasurementTool
{
public:
    Angle3DMeasurement();

    /// 基本信息
    MeasurementType type() const override;

    /// 交互
    void reset() override;
    bool mousePressEvent(OpenGLViewerWidget* viewer, QMouseEvent* event) override;
    bool mouseMoveEvent(OpenGLViewerWidget* viewer, QMouseEvent* event) override;
    bool mouseReleaseEvent(OpenGLViewerWidget* viewer, QMouseEvent* event) override;

    /// 绘制当前测量过程中的临时 Overlay。
    void drawOverlay(OpenGLViewerWidget* viewer, QPainter& painter) const override;

private:
    /// 获取视口位置对应的模型表面世界坐标。
    bool viewportPointToScene(OpenGLViewerWidget* viewer, const QPointF& viewportPosition, MeasurementPoint& point) const;

    /// 确保三维角度测量使用的共享线 Material 已创建。
    bool ensureResultMaterial(OpenGLViewerWidget* viewer);

    /// 提交角度测量结果，vertex 为角度顶点。
    bool commitResult(OpenGLViewerWidget* viewer, const MeasurementPoint& vertex, const MeasurementPoint& first, const MeasurementPoint& end);

    /// 计算 vertex→first 与 vertex→end 的较小夹角，单位为度。
    bool angleValue(const QVector3D& vertex, const QVector3D& first, const QVector3D& end, double& angle) const;

    /// 返回测量点世界坐标文本。
    QString pointText(const MeasurementPoint& point) const;

private:
    int m_pointCount;                 // 当前已经确定的测量点数量。
    MeasurementPoint m_vertexPoint;   // P1，角度顶点。
    MeasurementPoint m_firstPoint;    // P2，第一条边端点。
    MeasurementPoint m_endPoint;      // P3，第二条边端点。
    MeasurementPoint m_currentPoint;  // 当前鼠标对应的临时测量点。
    QPointF m_cursorPosition;         // 当前鼠标视口位置。
    bool m_hasCursorPosition;         // 是否已有有效鼠标位置。
    Material* m_resultMaterial;       // 测量线共享 Material，由 MaterialManager 拥有。
};

#endif // ANGLE3DMEASUREMENT_H