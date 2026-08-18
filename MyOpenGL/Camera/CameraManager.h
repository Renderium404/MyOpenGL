#ifndef CAMERAMANAGER_H
#define CAMERAMANAGER_H

#include "Camera.h"
#include "MyOpenGL/Scene/AxisAlignedBoundingBox.h"

#include <cstddef>
#include <map>

/// 相机管理器。
/// 负责 CameraId、相机所有权、Active Camera、View Bounds 以及基于 View Bounds 的视图导航。
/// CameraManager 始终维护一个有效 View Bounds；没有业务 Bounds 时使用原点单位盒作为默认范围。
/// Camera Target 是 View Bounds Center 的派生结果，不作为 CameraManager 的独立视图中心状态。
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
    void clear();                                 // 删除全部相机、清除 Active Camera，并恢复默认 View Bounds。

    /// Active Camera
    bool setActiveCamera(CameraId id);
    CameraId activeCameraId() const;
    Camera* activeCamera();
    const Camera* activeCamera() const;

    /// View Bounds
    bool setViewBounds(const AxisAlignedBoundingBox& bounds); // 只替换 View Bounds，不立即改变 Camera。
    void clearViewBounds();                             // 恢复默认原点单位盒，并同步当前 Active Camera 到默认 Bounds Center。
    bool hasViewBounds() const;                        // 正常生命周期始终为 true；保留用于状态检查。
    const AxisAlignedBoundingBox& viewBounds() const;
    bool focusBounds(const AxisAlignedBoundingBox& bounds); // 替换 View Bounds，并将 Camera 平移到新 Bounds Center，保持当前方向和距离。
    bool focusPoint(const QVector3D& worldPoint);            // 平移当前 View Bounds，使其 Center 移到指定世界点，并同步平移 Camera。
    bool fitViewBounds(int viewportWidth, int viewportHeight, float margin = 1.15f); // 按当前方向使 View Bounds 完整进入视野。
    bool fitBounds(const AxisAlignedBoundingBox& bounds, int viewportWidth, int viewportHeight, float margin = 1.15f); // 替换 View Bounds 后立即 Fit。

    /// 视图导航
    bool setViewDirection(const QVector3D& forward, const QVector3D& up); // 围绕 View Bounds Center 设置观察方向，保持当前距离。
    bool orbit(float yawDegrees, float pitchDegrees);                     // 使用当前屏幕 Up / Right 构造增量四元数，全角度围绕 View Bounds Center 旋转。
    bool pan(float rightDistance, float upDistance);                      // 沿当前视图 Right / Up 平移整个 View Bounds 和 Camera。
    bool zoom(float factor);                                              // factor > 1 放大，0 < factor < 1 缩小；View Bounds 保持不变。

    /// 标准方向
    bool viewFront();     // 从 +Z 看向 View Bounds Center。
    bool viewBack();      // 从 -Z 看向 View Bounds Center。
    bool viewLeft();      // 从 -X 看向 View Bounds Center。
    bool viewRight();     // 从 +X 看向 View Bounds Center。
    bool viewTop();       // 从 +Y 看向 View Bounds Center，屏幕上方对应 -Z。
    bool viewBottom();    // 从 -Y 看向 View Bounds Center，屏幕上方对应 +Z。
    bool viewIsometric(); // 从 +X/+Y/+Z 方向观察 View Bounds Center。

private:
    typedef std::map<CameraId, Camera*> CameraMap;

    static AxisAlignedBoundingBox createDefaultViewBounds(); // 默认范围：[-1, -1, -1] ~ [1, 1, 1]。
    bool validateViewBounds(const char* operation) const; // 检查 View Bounds 是否有效；失败表示内部状态异常。
    bool applyView(const AxisAlignedBoundingBox& bounds, const QVector3D& forward, const QVector3D& up, float distance); // 以指定 Bounds Center 为 Target 应用 Camera View。
    bool translatedViewBounds(const QVector3D& translation, AxisAlignedBoundingBox& translatedBounds) const; // 计算平移后的 View Bounds。
    bool fitBoundsInternal(const AxisAlignedBoundingBox& bounds, int viewportWidth, int viewportHeight, float margin); // 对明确 Bounds 计算并应用 Fit，不修改缓存。

private:
    CameraMap m_cameras;                 // 当前管理的全部 Camera，CameraManager 拥有这些对象。
    AxisAlignedBoundingBox m_viewBounds; // 当前视图控制包围盒；始终有效，默认是原点单位盒，Camera Target 由其 Center 派生。
    CameraId m_nextId;                   // 下一个可分配 CameraId，0 保留为 InvalidCameraId。
    CameraId m_activeCameraId;           // 当前 Active Camera，没有 Active Camera 时为 InvalidCameraId。
};

#endif // CAMERAMANAGER_H