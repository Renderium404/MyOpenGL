#ifndef CAMERAMANAGER_H
#define CAMERAMANAGER_H

#include "Camera/Camera.h"

#include <cstddef>
#include <map>

class AxisAlignedBoundingBox;
class Scene;

/// 相机管理器。
/// 负责 CameraId、相机所有权、Active Camera 以及常用视图导航操作。
class CameraManager
{
public:
    CameraManager();
    ~CameraManager();

    /// 相机管理
    CameraId add(Camera* camera);                 // 注册相机并取得所有权，成功后返回 CameraId。
    Camera* get(CameraId id);                     // 获取指定相机，不存在时返回 0。
    const Camera* get(CameraId id) const;         // 获取指定只读相机，不存在时返回 0。
    bool contains(CameraId id) const;
    std::size_t count() const;
    bool remove(CameraId id);                     // 删除指定相机；删除 Active Camera 后 Active Camera 变为无效。
    void clear();                                 // 删除全部相机并清除 Active Camera。

    /// Active Camera
    bool setActiveCamera(CameraId id);
    CameraId activeCameraId() const;
    Camera* activeCamera();
    const Camera* activeCamera() const;

    /// 视图导航
    bool orbit(float yawDegrees, float pitchDegrees); // 围绕 Active Camera 的 Target 旋转 Position，角度单位为度。
    bool pan(float rightDistance, float upDistance);  // 沿当前视图 Right / Up 平移 Position 和 Target。
    bool zoom(float factor);                          // factor > 1 放大，0 < factor < 1 缩小。
    bool focus(const QVector3D& target);              // 将 Orbit Target 移到指定世界坐标，同时保持当前观察方向和距离。
    bool fitBounds(const AxisAlignedBoundingBox& bounds, int viewportWidth, int viewportHeight, float margin = 1.15f); // 保持当前观察方向，使指定 World Bounds 完整进入视野。
    bool fitAll(const Scene& scene, int viewportWidth, int viewportHeight, float margin = 1.15f); // 聚合可见 Scene Bounds，并复用 fitBounds() 完成取景。

    /// 标准视图
    bool viewFront(const Scene& scene, int viewportWidth, int viewportHeight, float margin = 1.15f);     // 从 +Z 看向 -Z，并自动 Fit 当前 Scene。
    bool viewBack(const Scene& scene, int viewportWidth, int viewportHeight, float margin = 1.15f);      // 从 -Z 看向 +Z，并自动 Fit 当前 Scene。
    bool viewLeft(const Scene& scene, int viewportWidth, int viewportHeight, float margin = 1.15f);      // 从 -X 看向 +X，并自动 Fit 当前 Scene。
    bool viewRight(const Scene& scene, int viewportWidth, int viewportHeight, float margin = 1.15f);     // 从 +X 看向 -X，并自动 Fit 当前 Scene。
    bool viewTop(const Scene& scene, int viewportWidth, int viewportHeight, float margin = 1.15f);       // 从 +Y 看向 -Y，屏幕上方对应 -Z，并自动 Fit 当前 Scene。
    bool viewBottom(const Scene& scene, int viewportWidth, int viewportHeight, float margin = 1.15f);    // 从 -Y 看向 +Y，屏幕上方对应 +Z，并自动 Fit 当前 Scene。
    bool viewIsometric(const Scene& scene, int viewportWidth, int viewportHeight, float margin = 1.15f); // 从 +X/+Y/+Z 方向观察，并自动 Fit 当前 Scene。

private:
    bool setStandardView(const Scene& scene, int viewportWidth, int viewportHeight, const QVector3D& forward, const QVector3D& up, float margin);

    typedef std::map<CameraId, Camera*> CameraMap;

    CameraMap m_cameras;       // 当前管理的全部 Camera，CameraManager 拥有这些对象。
    CameraId m_nextId;         // 下一个可分配 CameraId，0 保留为 InvalidCameraId。
    CameraId m_activeCameraId; // 当前 Active Camera，没有 Active Camera 时为 InvalidCameraId。
};

#endif // CAMERAMANAGER_H
