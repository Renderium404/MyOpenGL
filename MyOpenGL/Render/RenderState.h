#ifndef RENDERSTATE_H
#define RENDERSTATE_H

#include <QMatrix4x4>

/// 单次 Draw 使用的最终 OpenGL Viewport。
/// x / y 使用 OpenGL 左下角坐标系。
struct RenderViewport
{
    RenderViewport();
    RenderViewport(int x, int y, int width, int height);

    bool isValid() const;

    int x;
    int y;
    int width;
    int height;
};

/// 单次 Geometry Draw 的最终渲染状态。
/// 这里保存 Renderer 可以直接执行的数据，不保存 TopRight、FollowCamera、RelativeSize 等高级显示规则。
struct RenderState
{
    RenderState();

    QMatrix4x4 model;
    QMatrix4x4 view;
    QMatrix4x4 projection;

    RenderViewport viewport;

    bool depthTestEnabled;
    bool depthWriteEnabled;
};

#endif // RENDERSTATE_H