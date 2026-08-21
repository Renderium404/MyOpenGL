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

    QMatrix4x4 model;           //模型坐标转世界坐标的变换矩阵
    QMatrix4x4 view;            //世界坐标转窗口坐标的变换矩阵
    QMatrix4x4 projection;      //窗口坐标转窗口投影的变换矩阵

    RenderViewport viewport;    //窗口信息

    bool depthTestEnabled;      //是否启用深度测试
    bool depthWriteEnabled;
};

#endif // RENDERSTATE_H