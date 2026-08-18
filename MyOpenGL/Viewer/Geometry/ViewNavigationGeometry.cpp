#include "ViewNavigationGeometry.h"

#include <QDebug>

ViewNavigationGeometry::ViewNavigationGeometry(const QString& name)
    : BufferGeometry(name, ResourceUpdateStatic, Lines)
    , m_axisLength(1.0f) // 导航器使用单位局部几何，最终屏幕尺寸由 Renderer 控制。
{
    rebuildGeometry();
}

/// 几何参数

float ViewNavigationGeometry::axisLength() const
{
    return m_axisLength;
}

bool ViewNavigationGeometry::setAxisLength(float length)
{
    if (length <= 0.0f)
    {
        qWarning() << "ViewNavigationGeometry setAxisLength failed: length must be greater than zero:" << name();
        return false;
    }

    if (m_axisLength == length)
        return true;

    m_axisLength = length;
    rebuildGeometry();
    return true;
}

/// 几何生成

void ViewNavigationGeometry::rebuildGeometry()
{
    std::vector<GeometryVertexAttribute> attributes;

    GeometryVertexAttribute positionAttribute;
    positionAttribute.location = 0;
    positionAttribute.componentCount = 3;
    positionAttribute.valueOffset = 0;
    attributes.push_back(positionAttribute);

    GeometryVertexAttribute colorAttribute;
    colorAttribute.location = 1;
    colorAttribute.componentCount = 3;
    colorAttribute.valueOffset = 3;
    attributes.push_back(colorAttribute);

    // 导航器几何只描述三个局部方向，不保存 Camera 和 Viewport 状态。
    const GLfloat vertexArray[] =
    {
        // position                     color
         0.0f, 0.0f, 0.0f,             1.0f, 0.0f, 0.0f,
         m_axisLength, 0.0f, 0.0f,      1.0f, 0.0f, 0.0f,

         0.0f, 0.0f, 0.0f,             0.0f, 1.0f, 0.0f,
         0.0f, m_axisLength, 0.0f,      0.0f, 1.0f, 0.0f,

         0.0f, 0.0f, 0.0f,             0.0f, 0.0f, 1.0f,
         0.0f, 0.0f, m_axisLength,      0.0f, 0.0f, 1.0f
    };

    const GLuint indexArray[] =
    {
        0, 1,
        2, 3,
        4, 5
    };

    const int vertexValueCount = sizeof(vertexArray) / sizeof(vertexArray[0]);
    const int indexCount = sizeof(indexArray) / sizeof(indexArray[0]);

    setVertexLayout(6, attributes);
    setVertexData(std::vector<GLfloat>(vertexArray, vertexArray + vertexValueCount));
    setIndexData(std::vector<GLuint>(indexArray, indexArray + indexCount));
}
