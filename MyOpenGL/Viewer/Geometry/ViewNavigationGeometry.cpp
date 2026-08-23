#include "ViewNavigationGeometry.h"

ViewNavigationGeometry::ViewNavigationGeometry(const QString& name)
    : BufferGeometry(name, BufferUsage::Static, RenderType::Triangles)
    , m_halfSize(0.30f)
{
    rebuildGeometry();
}

/// 几何参数

float ViewNavigationGeometry::halfSize() const
{
    return m_halfSize;
}

/// 几何生成

void ViewNavigationGeometry::rebuildGeometry()
{
    std::vector<GeometryVertexAttribute> attributes;

    GeometryVertexAttribute positionAttribute;
    positionAttribute.location = GeometryAttribute::Position;
    positionAttribute.componentCount = 3;
    positionAttribute.valueOffset = 0;
    attributes.push_back(positionAttribute);

    GeometryVertexAttribute colorAttribute;
    colorAttribute.location = GeometryAttribute::Color;
    colorAttribute.componentCount = 3;
    colorAttribute.valueOffset = 3;
    attributes.push_back(colorAttribute);

    const GLfloat h = m_halfSize;

    // 每个面使用独立的四个顶点。
    // 这样六个导航面能够分别使用自己的颜色，
    // 后续增加 Hover / Active Face 时也可以独立修改对应面的显示数据。
    const GLfloat vertices[] =
    {
        // +X Right
         h, -h, -h,    1.00f, 0.30f, 0.30f,
         h,  h, -h,    1.00f, 0.30f, 0.30f,
         h,  h,  h,    1.00f, 0.30f, 0.30f,
         h, -h,  h,    1.00f, 0.30f, 0.30f,

        // -X Left
        -h, -h,  h,    0.60f, 0.18f, 0.18f,
        -h,  h,  h,    0.60f, 0.18f, 0.18f,
        -h,  h, -h,    0.60f, 0.18f, 0.18f,
        -h, -h, -h,    0.60f, 0.18f, 0.18f,

        // +Y Top
        -h,  h, -h,    0.30f, 1.00f, 0.30f,
        -h,  h,  h,    0.30f, 1.00f, 0.30f,
         h,  h,  h,    0.30f, 1.00f, 0.30f,
         h,  h, -h,    0.30f, 1.00f, 0.30f,

        // -Y Bottom
        -h, -h,  h,    0.18f, 0.60f, 0.18f,
        -h, -h, -h,    0.18f, 0.60f, 0.18f,
         h, -h, -h,    0.18f, 0.60f, 0.18f,
         h, -h,  h,    0.18f, 0.60f, 0.18f,

        // +Z Front
         h, -h,  h,    0.30f, 0.42f, 1.00f,
         h,  h,  h,    0.30f, 0.42f, 1.00f,
        -h,  h,  h,    0.30f, 0.42f, 1.00f,
        -h, -h,  h,    0.30f, 0.42f, 1.00f,

        // -Z Back
        -h, -h, -h,    0.18f, 0.24f, 0.60f,
        -h,  h, -h,    0.18f, 0.24f, 0.60f,
         h,  h, -h,    0.18f, 0.24f, 0.60f,
         h, -h, -h,    0.18f, 0.24f, 0.60f
    };

    const GLuint indices[] =
    {
         0,  1,  2,   0,  2,  3,   // +X Right
         4,  5,  6,   4,  6,  7,   // -X Left
         8,  9, 10,   8, 10, 11,   // +Y Top
        12, 13, 14,  12, 14, 15,   // -Y Bottom
        16, 17, 18,  16, 18, 19,   // +Z Front
        20, 21, 22,  20, 22, 23    // -Z Back
    };

    const int vertexValueCount = sizeof(vertices) / sizeof(vertices[0]);
    const int indexCount = sizeof(indices) / sizeof(indices[0]);

    setVertexLayout(6, attributes); // position.xyz + color.rgb。
    setVertexData(std::vector<GLfloat>(vertices, vertices + vertexValueCount));
    setIndexData(std::vector<GLuint>(indices, indices + indexCount));
}