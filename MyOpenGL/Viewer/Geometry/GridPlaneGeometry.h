#ifndef GRIDPLANEGEOMETRY_H
#define GRIDPLANEGEOMETRY_H

#include "MyOpenGL/Resource/BufferGeometry.h"

/// 网格平面方向。
enum GridPlaneOrientation
{
    GridPlaneXY, // Z = 0，在 XY 平面生成参考网格。
    GridPlaneXZ, // Y = 0，在 XZ 平面生成参考网格，默认作为场景地面。
    GridPlaneYZ  // X = 0，在 YZ 平面生成参考网格。
};

/// Viewer 场景参考网格几何。
/// 根据平面方向、半尺寸和间距生成规则 GL_LINES 网格。
class GridPlaneGeometry : public BufferGeometry
{
public:
    explicit GridPlaneGeometry(const QString& name = "GridPlane");

    /// 网格参数
    GridPlaneOrientation orientation() const;
    float halfSize() const;
    float spacing() const;
    int divisionCount() const; // 获取从原点到 halfSize 范围内可容纳的完整间隔数量。

    bool setOrientation(GridPlaneOrientation orientation); // 修改网格主平面，并重新生成完整 CPU 几何数据。
    bool setHalfSize(float halfSize);                       // 修改网格从原点向正负方向延伸的最大距离。
    bool setSpacing(float spacing);                         // 修改相邻平行网格线之间的距离。
    bool setGrid(float halfSize, float spacing);            // 同时修改尺寸和间距，只执行一次几何重建。

private:
    void rebuildGeometry(); // 根据当前参数重新生成 position + color 顶点数据和索引数据。

private:
    GridPlaneOrientation m_orientation; // 当前网格所在的局部主平面。
    float m_halfSize;                    // 网格从局部原点向正负两个方向延伸的最大距离。
    float m_spacing;                     // 相邻平行网格线之间的局部坐标距离。
};

#endif // GRIDPLANEGEOMETRY_H
