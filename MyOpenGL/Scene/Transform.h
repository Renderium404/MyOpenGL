#ifndef TRANSFORM_H
#define TRANSFORM_H

#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>

/// RenderItem 的局部变换。
/// 第一版只保存独立的 Position / Rotation / Scale，不包含 Parent / Child 层级关系。
class Transform
{
public:
    Transform();

    /// 变换状态
    const QVector3D& position() const;
    const QQuaternion& rotation() const;
    const QVector3D& scale() const;

    void setPosition(const QVector3D& position);
    void setRotation(const QQuaternion& rotation);
    void setScale(const QVector3D& scale);
    void setUniformScale(float scale); // 同时设置三个轴的缩放比例；允许负值用于镜像。
    void reset();                      // 恢复 Position=(0,0,0)、Identity Rotation、Scale=(1,1,1)。

    /// Matrix
    QMatrix4x4 matrix() const; // 按 Translation * Rotation * Scale 生成当前 Model Matrix。

private:
    QVector3D m_position;   // 当前局部平移。
    QQuaternion m_rotation; // 当前局部旋转。
    QVector3D m_scale;      // 当前局部缩放。
};

#endif // TRANSFORM_H