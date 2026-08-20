#include "CoordinateSystemGeometry.h"

#include <QDebug>
#include <QVector3D>

#include "MyOpenGL/Viewer/Modeling/PrimitiveMeshBuilder.h"

CoordinateSystemGeometry::CoordinateSystemGeometry(const QString& name)
    : BufferGeometry(name, ResourceUpdateStatic, Triangles)
    , m_axisLength(1.0f)
{
    rebuildGeometry();
}

/// 几何参数

float CoordinateSystemGeometry::axisLength() const
{
    return m_axisLength;
}

bool CoordinateSystemGeometry::setAxisLength(float length)
{
    if (length <= 0.0f)
    {
        qWarning() << "CoordinateSystemGeometry setAxisLength failed: length must be greater than zero:" << name();
        return false;
    }

    if (m_axisLength == length)
        return true;

    m_axisLength = length;
    rebuildGeometry();
    return true;
}

/// 几何生成

void CoordinateSystemGeometry::rebuildGeometry()
{
    PrimitiveMeshBuilder builder;

    const QVector3D origin(0.0f, 0.0f, 0.0f);

    const QVector3D originColor(0.75f, 0.75f, 0.75f);
    const QVector3D xColor(1.0f, 0.15f, 0.15f);
    const QVector3D yColor(0.15f, 1.0f, 0.15f);
    const QVector3D zColor(0.15f, 0.35f, 1.0f);

    const float arrowLength = m_axisLength * 0.18f;
    const float bodyLength = m_axisLength - arrowLength;

    const float originRadius = m_axisLength * 0.055f;
    const float bodyRadius = m_axisLength * 0.025f;
    const float arrowRadius = m_axisLength * 0.060f;

    const QVector3D xBodyEnd(bodyLength, 0.0f, 0.0f);
    const QVector3D yBodyEnd(0.0f, bodyLength, 0.0f);
    const QVector3D zBodyEnd(0.0f, 0.0f, bodyLength);

    const QVector3D xEnd(m_axisLength, 0.0f, 0.0f);
    const QVector3D yEnd(0.0f, m_axisLength, 0.0f);
    const QVector3D zEnd(0.0f, 0.0f, m_axisLength);

    /// 原点
    builder.appendSphere(origin, originRadius, originColor, 16, 8);

    /// X
    builder.appendCylinder(origin, xBodyEnd, bodyRadius, xColor, 16);
    builder.appendCone(xBodyEnd, xEnd, arrowRadius, xColor, 16);

    /// Y
    builder.appendCylinder(origin, yBodyEnd, bodyRadius, yColor, 16);
    builder.appendCone(yBodyEnd, yEnd, arrowRadius, yColor, 16);

    /// Z
    builder.appendCylinder(origin, zBodyEnd, bodyRadius, zColor, 16);
    builder.appendCone(zBodyEnd, zEnd, arrowRadius, zColor, 16);

    /// PrimitiveMeshBuilder 数据格式：
    /// Position.xyz + Normal.xyz + Color.rgb
    ///
    /// 当前系统 VertexColor Shader 只需要 Position 和 Color，
    /// 因此 Normal 保存在 Buffer 中，但暂时不绑定到 Shader Attribute。
    std::vector<GeometryVertexAttribute> attributes;

    GeometryVertexAttribute positionAttribute;
    positionAttribute.location = 0;
    positionAttribute.componentCount = 3;
    positionAttribute.valueOffset = PrimitiveMeshBuilder::PositionOffset;
    attributes.push_back(positionAttribute);

    GeometryVertexAttribute colorAttribute;
    colorAttribute.location = 1;
    colorAttribute.componentCount = 3;
    colorAttribute.valueOffset = PrimitiveMeshBuilder::ColorOffset;
    attributes.push_back(colorAttribute);

    setVertexLayout(PrimitiveMeshBuilder::VertexStride, attributes);
    setVertexData(builder.vertexData());
    setIndexData(builder.indexData());
}


