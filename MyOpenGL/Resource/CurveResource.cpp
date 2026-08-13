#include "CurveResource.h"

#include <QDebug>

CurveResource::CurveResource(const QString& name)
    : MeshResource(name, ResourceTypeCurve, ResourceUpdateDynamic, MeshPrimitiveLineStrip)
    , m_color(1.0f, 1.0f, 1.0f)
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

    // Curve Vertex 固定使用 position.xyz + color.rgb，共 6 个 GLfloat。
    setVertexLayout(6, attributes);
}

/// 曲线基本信息

int CurveResource::pointCount() const
{
    return static_cast<int>(m_points.size());
}

const std::vector<QVector3D>& CurveResource::points() const
{
    return m_points;
}

const QVector3D& CurveResource::color() const
{
    return m_color;
}

/// 曲线完整数据

bool CurveResource::setPoints(const std::vector<QVector3D>& points)
{
    // GL_LINE_STRIP 至少需要两个控制点才能形成一条有效线段。
    if (points.size() < 2)
    {
        qWarning() << "CurveResource setPoints failed: curve requires at least 2 points:" << name();
        return false;
    }

    m_points = points;
    rebuildGeometry();
    return true;
}

bool CurveResource::setColor(const QVector3D& color)
{
    if (color.x() < 0.0f || color.y() < 0.0f || color.z() < 0.0f)
    {
        qWarning() << "CurveResource setColor failed: color components cannot be negative:" << name();
        return false;
    }

    if (m_color == color)
        return true;

    m_color = color;

    // 每个 Vertex 都保存一份颜色，修改统一颜色会影响全部 Vertex，因此当前直接执行全量更新。
    if (!m_points.empty())
        rebuildGeometry();

    return true;
}

/// 曲线动态编辑

bool CurveResource::updatePoint(int pointIndex, const QVector3D& point)
{
    if (pointIndex < 0 || pointIndex >= static_cast<int>(m_points.size()))
    {
        qWarning() << "CurveResource updatePoint failed: point index is out of range:" << pointIndex << name();
        return false;
    }

    if (m_points[pointIndex] == point)
        return true;

    m_points[pointIndex] = point;

    // 每个 Curve Vertex 为 6 个 GLfloat，position.xyz 位于 Vertex 起始位置的前 3 个值。
    const int valuesPerCurveVertex = 6;
    const int positionValueOffset = pointIndex * valuesPerCurveVertex;

    const GLfloat positionValues[] =
    {
        point.x(),
        point.y(),
        point.z()
    };

    return updateVertexData(positionValueOffset, positionValues, 3);
}

bool CurveResource::appendPoint(const QVector3D& point)
{
    if (m_points.size() < 2)
    {
        qWarning() << "CurveResource appendPoint failed: initialize the curve with setPoints() first:" << name();
        return false;
    }

    m_points.push_back(point);

    // Vertex / Index 数量均发生变化，现有 GPU Buffer Storage 大小可能不足，因此执行全量更新。
    rebuildGeometry();
    return true;
}

bool CurveResource::removePoint(int pointIndex)
{
    if (pointIndex < 0 || pointIndex >= static_cast<int>(m_points.size()))
    {
        qWarning() << "CurveResource removePoint failed: point index is out of range:" << pointIndex << name();
        return false;
    }

    // 删除后仍必须保留至少两个点，否则无法形成 GL_LINE_STRIP。
    if (m_points.size() <= 2)
    {
        qWarning() << "CurveResource removePoint failed: curve must keep at least 2 points:" << name();
        return false;
    }

    m_points.erase(m_points.begin() + pointIndex);

    // 删除 Vertex 会改变后续 Vertex 和 Index 的排列及 Buffer 大小，因此执行全量更新。
    rebuildGeometry();
    return true;
}

/// 几何生成

void CurveResource::rebuildGeometry()
{
    std::vector<GLfloat> vertices;
    std::vector<GLuint> indices;

    const int valuesPerCurveVertex = 6; // position.xyz + color.rgb。
    vertices.reserve(m_points.size() * valuesPerCurveVertex);
    indices.reserve(m_points.size());

    for (std::size_t i = 0; i < m_points.size(); ++i)
    {
        const QVector3D& point = m_points[i];

        vertices.push_back(point.x());
        vertices.push_back(point.y());
        vertices.push_back(point.z());

        vertices.push_back(m_color.x());
        vertices.push_back(m_color.y());
        vertices.push_back(m_color.z());

        // GL_LINE_STRIP 按控制点顺序连续连接，因此索引直接使用 0、1、2……。
        indices.push_back(static_cast<GLuint>(i));
    }

    setVertexData(vertices);
    setIndexData(indices);
}