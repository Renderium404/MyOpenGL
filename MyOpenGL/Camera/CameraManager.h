#ifndef CAMERAMANAGER_H
#define CAMERAMANAGER_H

#include "Camera.h"
#include "MyOpenGL/Scene/AxisAlignedBoundingBox.h"

#include <cstddef>
#include <map>

class CameraManager
{
public:
    CameraManager();
    ~CameraManager();

    /// Camera 管理
    CameraId add(Camera* camera);
    Camera* get(CameraId id);
    const Camera* get(CameraId id) const;
    bool contains(CameraId id) const;
    std::size_t count() const;
    bool remove(CameraId id);
    void clear();

    /// Active Camera
    bool setActiveCamera(CameraId id);
    CameraId activeCameraId() const;
    Camera* activeCamera();
    const Camera* activeCamera() const;
    /// Camera Navigation
    // 围绕锚点旋转相机，并保持锚点在屏幕上的投影位置不变
    bool orbitAround(const QVector3D& anchor, float yaw, float pitch);
    // 以锚点所在深度换算屏幕位移并平移相机，使锚点投影随鼠标拖动
    bool panAt(const QVector3D& anchor, float deltaX, float deltaY,int viewportWidth, int viewportHeight);
    // 以锚点为缩放参考，并保持锚点在屏幕上的投影位置不变
    bool zoomAt(const QVector3D& anchor, float factor,int viewportWidth, int viewportHeight);
    // 将相机切换到指定观察方向，并保持与观察锚点的距离不变。
    bool setViewDirection(const QVector3D& anchor, const QVector3D& forward, const QVector3D& up);
    /// View Bounds
    bool setViewBounds(const AxisAlignedBoundingBox& bounds);
    void clearViewBounds();
    bool hasViewBounds() const;
    const AxisAlignedBoundingBox& viewBounds() const;

    // 将 Bounds Center 移到视图中心，保持 Camera Orientation 和当前观察尺度。
    bool focusBounds(const AxisAlignedBoundingBox& bounds);
    // 将 Bounds Center 移到视图中心，并调整观察尺度使整个 Bounds 可见。
    bool fitBounds(const AxisAlignedBoundingBox& bounds, int viewportWidth, int viewportHeight, float margin = 1.15f);


private:
    typedef std::map<CameraId, Camera*> CameraMap;

private:
    CameraMap m_cameras;        //相机组
    CameraId m_nextId;          //可分配ID
    CameraId m_activeCameraId;  //当前活跃的相机

    AxisAlignedBoundingBox m_viewBounds;    //额外关注的空间
};

#endif // CAMERAMANAGER_H