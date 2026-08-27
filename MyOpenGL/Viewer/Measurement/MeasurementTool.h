#ifndef MEASUREMENTTOOL_H
#define MEASUREMENTTOOL_H

#include <QPointF>
#include <QString>
#include <QVector3D>
#include <QVector4D>

class QKeyEvent;
class QMouseEvent;
class QPainter;

class OpenGLViewerWidget;
class RenderItem;
class RenderLabel;

/// 测量类型。
enum class MeasurementType
{
    None,
    Length2D, // 当前视口平面长度测量。
    Length3D, // 模型真实三维长度测量。
    Angle2D,  // 当前视口平面角度测量。
    Angle3D   // 模型真实三维角度测量。
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
    bool valid;               // 是否为有效测量点。

    MeasurementPoint()
        : valid(false)
    {
    }
};

/// 测量工具基类。
/// 统一管理测量状态、结果线颜色和结果线宽度。
class MeasurementTool
{
public:
    MeasurementTool();
    virtual ~MeasurementTool();

    /// 基本信息
    /// 返回当前测量类型。
    virtual MeasurementType type() const = 0;
    /// 返回当前测量交互状态。
    MeasurementState state() const;
    /// 设置当前测量交互状态。
    void setState(MeasurementState state);

    /// 结果样式
    /// 返回测量结果线颜色。
    const QVector4D& lineColor() const;
    /// 设置测量结果线颜色。
    void setLineColor(const QVector4D& color);
    /// 返回测量结果线宽度，单位为屏幕 Pixel。
    float lineWidth() const;
    /// 设置测量结果线宽度，必须大于 0。
    bool setLineWidth(float width);

    /// 交互

    /// 重置当前测量。
    virtual void reset() = 0;
    virtual bool mousePressEvent(OpenGLViewerWidget* viewer, QMouseEvent* event) = 0;
    virtual bool mouseMoveEvent(OpenGLViewerWidget* viewer, QMouseEvent* event) = 0;
    virtual bool mouseReleaseEvent(OpenGLViewerWidget* viewer, QMouseEvent* event) = 0;
    virtual bool keyPressEvent(OpenGLViewerWidget* viewer, QKeyEvent* event);

    /// 绘制当前测量过程中的临时 Overlay。
    virtual void drawOverlay(OpenGLViewerWidget* viewer, QPainter& painter) const = 0;

protected:
    /// 创建持久化文本 Label。
    static RenderLabel* createPersistentLabel(OpenGLViewerWidget* viewer, 
                                                RenderItem* item, 
                                                const QVector3D& anchorWorld, 
                                                const QVector2D& anchorSence, 
                                                const QPointF& pixelOffset,
                                                const QString& text);
    /// 绘制临时 QPainter 文本 Label。
    static void drawOverlayLabel(QPainter& painter, const QPointF& position, const QString& text);

private:
    MeasurementState m_state; // 当前测量交互状态。
    QVector4D m_lineColor;     // 测量结果线 RGBA 颜色。
    float m_lineWidth;         // 测量结果线屏幕 Pixel 宽度。
};

#endif // MEASUREMENTTOOL_H