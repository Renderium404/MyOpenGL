#ifndef CAMERA_H
#define CAMERA_H

#include <QMatrix4x4>
#include <QQuaternion>
#include <QString>
#include <QVector3D>

class CameraManager;

/// 相机唯一标识类型，由 CameraManager 统一分配。
typedef unsigned int CameraId;

/// 无效相机 ID。
const CameraId InvalidCameraId = 0;

/// 相机投影类型。
/// 描述当前 Camera 使用透视投影还是正交投影。
enum CameraProjectionType
{
    CameraProjectionPerspective,   // 透视投影，物体随距离增加而缩小。
    CameraProjectionOrthographic   // 正交投影，物体显示尺寸不随距离变化。
};

/// 获取相机投影类型的调试名称。
const char* cameraProjectionTypeName(CameraProjectionType type);

/// 单个相机状态。
/// 使用四元数保存完整观察姿态，同时保存 Position / Target 和投影参数，并负责生成 View / Projection Matrix。
class Camera
{
public:
    explicit Camera(const QString& name = "Camera");

    /// 相机基本信息
    CameraId id() const;
    const QString& name() const;
    CameraProjectionType projectionType() const;

    /// 观察状态
    const QVector3D& position() const;
    const QVector3D& target() const;
    const QVector3D& up() const;       // 获取当前实际视图 Up 方向，保持旧接口兼容。
    const QQuaternion& orientation() const; // 获取 Camera Local -> World 的完整四元数姿态。
    QVector3D forward() const;          // 获取当前四元数姿态对应的单位 Forward；Camera Local Forward = -Z。
    QVector3D right() const;            // 获取当前四元数姿态对应的单位 Right；Camera Local Right = +X。
    QVector3D viewUp() const;           // 获取当前四元数姿态对应的单位 Up；Camera Local Up = +Y。
    float distanceToTarget() const;
    bool setView(const QVector3D& position, const QVector3D& target, const QVector3D& up); // 同时设置观察状态并检查方向是否合法。

    /// Picking Ray
    bool screenPointToRay(int screenX, int screenY, int viewportWidth, int viewportHeight, QVector3D& rayOrigin, QVector3D& rayDirection) const; // 将 Widget 像素坐标转换为当前 Camera 的世界空间 Picking Ray。

    /// 透视投影
    float fieldOfView() const;
    bool setPerspective(float fieldOfView, float nearPlane, float farPlane); // 设置垂直 FOV、Near 和 Far，并切换到透视投影。

    /// 正交投影
    float orthographicHeight() const;
    bool setOrthographic(float height, float nearPlane, float farPlane); // 设置可视区域高度、Near 和 Far，并切换到正交投影。

    /// 公共投影参数
    float nearPlane() const;
    float farPlane() const;

    /// 矩阵
    QMatrix4x4 viewMatrix() const;
    QMatrix4x4 projectionMatrix(float aspect) const;

private:
    friend class CameraManager;

    void setId(CameraId id); // 仅允许 CameraManager 设置相机 ID。

private:
    CameraId m_id;                           // 当前相机唯一标识，未注册时为 InvalidCameraId。
    QString m_name;                          // 当前相机调试名称。
    CameraProjectionType m_projectionType;   // 当前相机投影类型。
    QVector3D m_position;                    // 当前相机在世界坐标中的位置。
    QVector3D m_target;                      // 当前相机观察目标点。
    QVector3D m_up;                          // 当前四元数姿态对应的实际 View Up，供旧引用接口返回。
    QQuaternion m_orientation;                // Camera Local -> World 完整旋转姿态，避免欧拉角和极点锁定。
    float m_fieldOfView;                     // 透视投影垂直视场角，单位为度。
    float m_orthographicHeight;              // 正交投影视口在世界坐标中的可视高度。
    float m_nearPlane;                       // 当前投影 Near Plane 距离。
    float m_farPlane;                        // 当前投影 Far Plane 距离。
};

#endif // CAMERA_H