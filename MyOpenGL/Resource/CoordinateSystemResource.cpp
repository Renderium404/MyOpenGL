#include "CoordinateSystemResource.h"

#include <QDebug>

CoordinateSystemResource::CoordinateSystemResource(const QString& name)
    : MeshResource(name, ResourceTypeCoordinateSystem, ResourceUpdateStatic, MeshPrimitiveLines)
    , m_axisLength(1.0f) // 默认单位坐标系，每个轴从原点延伸 1 个局部坐标单位。
{
    rebuildGeometry();
}

/// 几何参数

float CoordinateSystemResource::axisLength() const
{
    return m_axisLength;
}

bool CoordinateSystemResource::setAxisLength(float length)
{
    if (length <= 0.0f)
    {
        qWarning() << "CoordinateSystemResource setAxisLength failed: length must be greater than zero:" << name();
        return false;
    }

    if (m_axisLength == length)
        return true;

    m_axisLength = length;
    rebuildGeometry();
    return true;
}

/// 几何生成

void CoordinateSystemResource::rebuildGeometry()
{
    std::vector<MeshVertexAttribute> attributes;

    MeshVertexAttribute positionAttribute;
    positionAttribute.location = 0;
    positionAttribute.componentCount = 3;
    positionAttribute.valueOffset = 0;
    attributes.push_back(positionAttribute);

    MeshVertexAttribute colorAttribute;
    colorAttribute.location = 1;
    colorAttribute.componentCount = 3;
    colorAttribute.valueOffset = 3;
    attributes.push_back(colorAttribute);

    // 每个坐标轴使用独立的原点 Vertex，使三个轴能够分别保存自己的 RGB 颜色。
    const GLfloat vertexArray[] =
    {
        // position                     color
         0.0f, 0.0f, 0.0f,             1.0f, 0.0f, 0.0f, // X origin
         m_axisLength, 0.0f, 0.0f,      1.0f, 0.0f, 0.0f, // +X

         0.0f, 0.0f, 0.0f,             0.0f, 1.0f, 0.0f, // Y origin
         0.0f, m_axisLength, 0.0f,      0.0f, 1.0f, 0.0f, // +Y

         0.0f, 0.0f, 0.0f,             0.0f, 0.0f, 1.0f, // Z origin
         0.0f, 0.0f, m_axisLength,      0.0f, 0.0f, 1.0f  // +Z
    };

    const GLuint indexArray[] =
    {
        0, 1,
        2, 3,
        4, 5
    };

    const int vertexValueCount = sizeof(vertexArray) / sizeof(vertexArray[0]);
    const int indexCount = sizeof(indexArray) / sizeof(indexArray[0]);

    setVertexLayout(6, attributes); // position.xyz + color.rgb，共 6 个 GLfloat。
    setVertexData(std::vector<GLfloat>(vertexArray, vertexArray + vertexValueCount));
    setIndexData(std::vector<GLuint>(indexArray, indexArray + indexCount));
}