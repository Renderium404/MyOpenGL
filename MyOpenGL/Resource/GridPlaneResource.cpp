#include "GridPlaneResource.h"

#include <QDebug>

GridPlaneResource::GridPlaneResource(const QString& name)
    : MeshResource(name, ResourceTypeGridPlane, ResourceUpdateStatic, Lines)
    , m_orientation(GridPlaneXZ)
    , m_halfSize(10.0f) // 默认网格覆盖 -10 到 +10，共 20 个局部坐标单位。
    , m_spacing(1.0f)   // 默认每 1 个局部坐标单位生成一条平行网格线。
{
    rebuildGeometry();
}

/// 网格参数

GridPlaneOrientation GridPlaneResource::orientation() const
{
    return m_orientation;
}

float GridPlaneResource::halfSize() const
{
    return m_halfSize;
}

float GridPlaneResource::spacing() const
{
    return m_spacing;
}

int GridPlaneResource::divisionCount() const
{
    // 参数均为正数，因此整数转换等价于 floor(halfSize / spacing)。
    return static_cast<int>(m_halfSize / m_spacing);
}

bool GridPlaneResource::setOrientation(GridPlaneOrientation orientation)
{
    if (orientation != GridPlaneXY && orientation != GridPlaneXZ && orientation != GridPlaneYZ)
    {
        qWarning() << "GridPlaneResource setOrientation failed: invalid orientation:" << name();
        return false;
    }

    if (m_orientation == orientation)
        return true;

    m_orientation = orientation;
    rebuildGeometry();
    return true;
}

bool GridPlaneResource::setHalfSize(float halfSize)
{
    if (halfSize <= 0.0f)
    {
        qWarning() << "GridPlaneResource setHalfSize failed: halfSize must be greater than zero:" << name();
        return false;
    }

    if (m_halfSize == halfSize)
        return true;

    m_halfSize = halfSize;
    rebuildGeometry();
    return true;
}

bool GridPlaneResource::setSpacing(float spacing)
{
    if (spacing <= 0.0f)
    {
        qWarning() << "GridPlaneResource setSpacing failed: spacing must be greater than zero:" << name();
        return false;
    }

    if (m_spacing == spacing)
        return true;

    m_spacing = spacing;
    rebuildGeometry();
    return true;
}

bool GridPlaneResource::setGrid(float halfSize, float spacing)
{
    if (halfSize <= 0.0f)
    {
        qWarning() << "GridPlaneResource setGrid failed: halfSize must be greater than zero:" << name();
        return false;
    }

    if (spacing <= 0.0f)
    {
        qWarning() << "GridPlaneResource setGrid failed: spacing must be greater than zero:" << name();
        return false;
    }

    if (m_halfSize == halfSize && m_spacing == spacing)
        return true;

    m_halfSize = halfSize;
    m_spacing = spacing;
    rebuildGeometry();
    return true;
}

/// 几何生成

void GridPlaneResource::rebuildGeometry()
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

    std::vector<GLfloat> vertices;
    std::vector<GLuint> indices;

    const int divisions = divisionCount();
    const int linePositionCount = divisions * 2 + 1;

    // 每个刻度位置生成两条互相垂直的网格线，每条线使用两个独立 Vertex。
    const int vertexCount = linePositionCount * 4;
    vertices.reserve(vertexCount * 6);
    indices.reserve(vertexCount);

    // 默认使用中性灰显示参考网格，避免与 RGB 坐标轴颜色产生语义冲突。
    const GLfloat colorR = 0.5f;
    const GLfloat colorG = 0.5f;
    const GLfloat colorB = 0.5f;

    for (int i = -divisions; i <= divisions; ++i)
    {
        const GLfloat offset = static_cast<GLfloat>(i) * m_spacing;

        GLfloat positions[12];

        if (m_orientation == GridPlaneXY)
        {
            // 一条平行 X，一条平行 Y；所有 Vertex 的 Z 均为 0。
            const GLfloat values[] =
            {
                -m_halfSize, offset, 0.0f,
                 m_halfSize, offset, 0.0f,
                 offset, -m_halfSize, 0.0f,
                 offset,  m_halfSize, 0.0f
            };

            for (int j = 0; j < 12; ++j)
                positions[j] = values[j];
        }
        else if (m_orientation == GridPlaneXZ)
        {
            // 一条平行 X，一条平行 Z；所有 Vertex 的 Y 均为 0。
            const GLfloat values[] =
            {
                -m_halfSize, 0.0f, offset,
                 m_halfSize, 0.0f, offset,
                 offset, 0.0f, -m_halfSize,
                 offset, 0.0f,  m_halfSize
            };

            for (int j = 0; j < 12; ++j)
                positions[j] = values[j];
        }
        else
        {
            // 一条平行 Y，一条平行 Z；所有 Vertex 的 X 均为 0。
            const GLfloat values[] =
            {
                0.0f, -m_halfSize, offset,
                0.0f,  m_halfSize, offset,
                0.0f, offset, -m_halfSize,
                0.0f, offset,  m_halfSize
            };

            for (int j = 0; j < 12; ++j)
                positions[j] = values[j];
        }

        for (int vertex = 0; vertex < 4; ++vertex)
        {
            const int positionOffset = vertex * 3;

            vertices.push_back(positions[positionOffset]);
            vertices.push_back(positions[positionOffset + 1]);
            vertices.push_back(positions[positionOffset + 2]);
            vertices.push_back(colorR);
            vertices.push_back(colorG);
            vertices.push_back(colorB);

            // 当前每个 Grid Vertex 只被一条 GL_LINES 线段使用，因此索引按顶点顺序连续生成。
            indices.push_back(static_cast<GLuint>(indices.size()));
        }
    }

    setVertexLayout(6, attributes);
    setVertexData(vertices);
    setIndexData(indices);
}