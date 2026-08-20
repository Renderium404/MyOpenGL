#ifndef RENDERCONTEXT_H
#define RENDERCONTEXT_H

#include <QMatrix4x4>
#include <QVector3D>

/// 当前 Frame 的共享渲染环境。
/// 由 Camera 和当前主 Viewport 生成，为用户模型和系统显示对象提供统一的帧级输入。
struct RenderContext
{
    RenderContext();

    QMatrix4x4 view;           // 当前主 Camera World -> Camera Matrix。
    QMatrix4x4 projection;     // 当前主 Camera Projection Matrix。

    QVector3D cameraPosition;  // 当前 Camera 世界坐标位置。
    QVector3D cameraForward;   // 当前 Camera 世界空间 Forward。
    QVector3D cameraUp;        // 当前 Camera 世界空间 Up。

    int viewportWidth;         // 当前主 Viewport Pixel 宽度。
    int viewportHeight;        // 当前主 Viewport Pixel 高度。

    bool isValid() const;
};

#endif // RENDERCONTEXT_H