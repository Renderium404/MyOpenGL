#ifndef MEASUREMENTTOOL_H
#define MEASUREMENTTOOL_H

#include <QPointF>
#include <QVector3D>

class QKeyEvent;
class QMouseEvent;
class QPainter;
class OpenGLViewerWidget;

/// 测量类型。
enum class MeasurementType
{
    None,
    /// 在当前视口平面内测量长度。
    Length2D,
    /// 使用模型真实三维点测量空间长度。
    Length3D,
    /// 在当前视口平面内测量角度。
    Angle2D,
    /// 使用模型真实三维点测量空间角度。
    Angle3D
};

/// 测量交互状态。
enum class MeasurementState
{
    Idle,       // 未开始测量。
    Collecting, // 正在采集测量点。
    Finished    // 测量完成。
};

/// 一个测量点。
struct MeasurementPoint
{
    QPointF viewportPosition; // 屏幕位置。
    QVector3D worldPosition;  // 世界坐标。
    bool valid;               // 是否命中有效位置。

    MeasurementPoint()
        : valid(false)
    {
    }
};

class MeasurementTool
{
public:
    virtual ~MeasurementTool()
    {
    }

    virtual MeasurementType type() const = 0;
    virtual MeasurementState state() const = 0;

    virtual void reset() = 0;

    virtual bool mousePressEvent(OpenGLViewerWidget* viewer, QMouseEvent* event) = 0;
    virtual bool mouseMoveEvent(OpenGLViewerWidget* viewer, QMouseEvent* event) = 0;
    virtual bool mouseReleaseEvent(OpenGLViewerWidget* viewer, QMouseEvent* event) = 0;
    virtual bool keyPressEvent(OpenGLViewerWidget* viewer, QKeyEvent* event) = 0;

    virtual void drawOverlay(OpenGLViewerWidget* viewer, QPainter& painter) const = 0;
protected:
    static void drawOverlayLabel(QPainter& painter, const QPointF& position, const QString& text);
};

#endif // MEASUREMENTTOOL_H