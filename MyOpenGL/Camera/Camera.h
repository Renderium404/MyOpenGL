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
enum class ProjectionType
{
    Perspective, // 透视投影：具有近大远小的透视效果。
    Parallel     // 平行投影：物体显示尺寸不随观察距离变化。
};

/// 单个相机状态。
/// 保存 Camera 在世界坐标中的观察姿态，以及透视和平行两套独立投影参数。
class Camera
{
public:
    /// 相机基本信息
    CameraId id() const { return m_id; }
    const QString& name() const { return m_name; }

    QString type() const;
    ProjectionType projectionType() const { return m_type; }

    /// 相机状态

    // 设置相机矩阵：Camera Local -> World。
    bool setCamera(const QMatrix4x4& matrix);
    bool setCamera(const QVector3D& position, const QQuaternion& orientation);

    //相机平面/你的视角为X_Y平面 ，你站在相机坐标系原点，头顶方向为Y轴，右手方向为X轴，则你的正前方为-Z轴
    QVector3D forward() const;
    QVector3D right() const;
    QVector3D up() const;

    const QVector3D& position() const { return m_position; }
    const QQuaternion& orientation() const { return m_orientation; }

    /// 投影

    // 设置透视投影参数，并切换到透视投影。
    bool setPerspective(float fieldOfView, float nearPlane, float farPlane);

    // 设置平行投影参数，并切换到平行投影。
    bool setParallel(float height, float nearPlane, float farPlane);

    float perspectiveFieldOfView() const { return m_perspectiveFieldOfView; }
    float parallelHeight() const { return m_parallelHeight; }

    float nearPlane() const
    {
        return m_type == ProjectionType::Perspective ? m_perspectiveNearPlane : m_parallelNearPlane;
    }

    float farPlane() const
    {
        return m_type == ProjectionType::Perspective ? m_perspectiveFarPlane : m_parallelFarPlane;
    }

    /// Matrix

    // 相机矩阵：把相机局部坐标转换到世界坐标。
    QMatrix4x4 cameraMatrix() const;

    // 视图矩阵：把世界坐标转换到相机局部坐标。
    QMatrix4x4 viewMatrix() const;

    //投影矩阵，将基于相机坐标系的点转换到屏幕上
    QMatrix4x4 projectionMatrix(float aspect) const;

    /// 坐标转换

    // 将屏幕像素坐标和深度值逆投影到相机坐标系。
    // depth 范围为 [0,1]：0 表示 Near Plane，1 表示 Far Plane。
    bool screenToCamera(float screenPointX, float screenPointY, float depth, int viewportWidth, int viewportHeight, QVector3D& cameraPoint) const;

    // 将相机坐标系中的点投影到屏幕像素坐标。
    // depth 范围为 [0,1]：0 表示 Near Plane，1 表示 Far Plane。
    bool cameraToScreen(const QVector3D& cameraPoint, int viewportWidth, int viewportHeight, float& screenPointX, float& screenPointY, float& depth) const;

    // 将屏幕像素位置转换为世界坐标系 Picking Ray。
    bool screenPointToRay(float screenPointX, float screenPointY, int viewportWidth, int viewportHeight, QVector3D& rayOrigin, QVector3D& rayDirection) const;

private:
    friend class CameraManager;

    /// CameraManager 内部接口
    explicit Camera(const QString& name);
    ~Camera();

    void setId(CameraId id) { m_id = id; }

private:
    ///相机基本信息
    CameraId m_id;
    QString m_name;

    ///用于确定唯一的相机坐标系
    QVector3D m_position;       //位置
    QQuaternion m_orientation;  //姿态

    /// 投影类型
    ProjectionType m_type;

    /// 透视信息，用于计算透视投影矩阵
    float m_perspectiveFieldOfView;
    float m_perspectiveNearPlane;
    float m_perspectiveFarPlane;

    /// 平行信息，用于计算平行投影矩阵
    float m_parallelHeight;
    float m_parallelNearPlane;
    float m_parallelFarPlane;
};

#endif // CAMERA_H