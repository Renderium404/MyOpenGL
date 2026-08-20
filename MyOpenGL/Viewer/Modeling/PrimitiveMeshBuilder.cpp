#include "PrimitiveMeshBuilder.h"

#include <QDebug>

#include <cmath>

namespace
{
const float Pi = 3.14159265358979323846f;
const float TwoPi = Pi * 2.0f;
const float Epsilon = 1.0e-8f;

bool buildFrame(const QVector3D& start, const QVector3D& end,
                QVector3D& axis, QVector3D& u, QVector3D& v, float& length)
{
    const QVector3D direction = end - start;
    const float lengthSquared = direction.lengthSquared();

    if (lengthSquared <= Epsilon * Epsilon)
        return false;

    length = std::sqrt(lengthSquared);
    axis = direction / length;

    // 选择一个不与 Axis 平行的参考方向。
    const QVector3D reference = std::fabs(axis.y()) < 0.9f
        ? QVector3D(0.0f, 1.0f, 0.0f)
        : QVector3D(1.0f, 0.0f, 0.0f);

    u = QVector3D::crossProduct(reference, axis);

    if (u.lengthSquared() <= Epsilon * Epsilon)
        return false;

    u.normalize();
    v = QVector3D::crossProduct(axis, u).normalized();

    return true;
}

QVector3D radialDirection(const QVector3D& u, const QVector3D& v, float angle)
{
    return u * std::cos(angle) + v * std::sin(angle);
}

bool validRadius(float radius)
{
    return radius > 0.0f;
}

bool validSegments(int segments)
{
    return segments >= 3;
}
}

PrimitiveMeshBuilder::PrimitiveMeshBuilder()
{
}

/// 数据

void PrimitiveMeshBuilder::clear()
{
    m_vertices.clear();
    m_indices.clear();
}

bool PrimitiveMeshBuilder::isEmpty() const
{
    return m_vertices.empty();
}

int PrimitiveMeshBuilder::vertexCount() const
{
    return static_cast<int>(m_vertices.size() / VertexStride);
}

int PrimitiveMeshBuilder::indexCount() const
{
    return static_cast<int>(m_indices.size());
}

const std::vector<GLfloat>& PrimitiveMeshBuilder::vertexData() const
{
    return m_vertices;
}

const std::vector<GLuint>& PrimitiveMeshBuilder::indexData() const
{
    return m_indices;
}

/// 基础写入

GLuint PrimitiveMeshBuilder::appendVertex(const QVector3D& position, const QVector3D& normal, const QVector3D& color)
{
    const GLuint index = static_cast<GLuint>(vertexCount());

    m_vertices.push_back(position.x());
    m_vertices.push_back(position.y());
    m_vertices.push_back(position.z());

    m_vertices.push_back(normal.x());
    m_vertices.push_back(normal.y());
    m_vertices.push_back(normal.z());

    m_vertices.push_back(color.x());
    m_vertices.push_back(color.y());
    m_vertices.push_back(color.z());

    return index;
}

void PrimitiveMeshBuilder::appendTriangle(GLuint a, GLuint b, GLuint c)
{
    m_indices.push_back(a);
    m_indices.push_back(b);
    m_indices.push_back(c);
}

void PrimitiveMeshBuilder::appendCap(const QVector3D& center, const QVector3D& axis,
                                     const QVector3D& u, const QVector3D& v,
                                     float radius, int segments, bool positiveAxis,
                                     const QVector3D& color)
{
    const QVector3D normal = positiveAxis ? axis : -axis;
    const GLuint centerIndex = appendVertex(center, normal, color);
    const GLuint ringStart = static_cast<GLuint>(vertexCount());

    for (int i = 0; i < segments; ++i)
    {
        const float angle = TwoPi * static_cast<float>(i) / static_cast<float>(segments);
        const QVector3D radial = radialDirection(u, v, angle);
        appendVertex(center + radial * radius, normal, color);
    }

    for (int i = 0; i < segments; ++i)
    {
        const GLuint current = ringStart + static_cast<GLuint>(i);
        const GLuint next = ringStart + static_cast<GLuint>((i + 1) % segments);

        if (positiveAxis)
            appendTriangle(centerIndex, current, next);
        else
            appendTriangle(centerIndex, next, current);
    }
}

/// 球

bool PrimitiveMeshBuilder::appendSphere(const QVector3D& center, float radius,
                                        const QVector3D& color, int segments, int rings)
{
    if (!validRadius(radius) || segments < 3 || rings < 2)
    {
        qWarning() << "PrimitiveMeshBuilder appendSphere failed: invalid parameters.";
        return false;
    }

    const QVector3D up(0.0f, 1.0f, 0.0f);

    const GLuint topIndex = appendVertex(center + up * radius, up, color);

    std::vector<GLuint> ringStarts;
    ringStarts.reserve(rings - 1);

    // 不在极点重复生成整圈顶点。
    for (int ring = 1; ring < rings; ++ring)
    {
        const float phi = Pi * static_cast<float>(ring) / static_cast<float>(rings);
        const float y = std::cos(phi);
        const float ringRadius = std::sin(phi);

        ringStarts.push_back(static_cast<GLuint>(vertexCount()));

        for (int segment = 0; segment < segments; ++segment)
        {
            const float theta = TwoPi * static_cast<float>(segment) / static_cast<float>(segments);

            QVector3D normal(
                ringRadius * std::cos(theta),
                y,
                ringRadius * std::sin(theta));

            normal.normalize();
            appendVertex(center + normal * radius, normal, color);
        }
    }

    const GLuint bottomIndex = appendVertex(center - up * radius, -up, color);

    /// 顶部

    const GLuint firstRing = ringStarts.front();

    for (int segment = 0; segment < segments; ++segment)
    {
        const GLuint current = firstRing + static_cast<GLuint>(segment);
        const GLuint next = firstRing + static_cast<GLuint>((segment + 1) % segments);
        appendTriangle(topIndex, next, current);
    }

    /// 中间

    for (int ring = 0; ring < static_cast<int>(ringStarts.size()) - 1; ++ring)
    {
        const GLuint upper = ringStarts[ring];
        const GLuint lower = ringStarts[ring + 1];

        for (int segment = 0; segment < segments; ++segment)
        {
            const GLuint nextSegment = static_cast<GLuint>((segment + 1) % segments);

            const GLuint a = upper + static_cast<GLuint>(segment);
            const GLuint b = upper + nextSegment;
            const GLuint c = lower + static_cast<GLuint>(segment);
            const GLuint d = lower + nextSegment;

            appendTriangle(a, b, c);
            appendTriangle(b, d, c);
        }
    }

    /// 底部

    const GLuint lastRing = ringStarts.back();

    for (int segment = 0; segment < segments; ++segment)
    {
        const GLuint current = lastRing + static_cast<GLuint>(segment);
        const GLuint next = lastRing + static_cast<GLuint>((segment + 1) % segments);
        appendTriangle(current, next, bottomIndex);
    }

    return true;
}

/// 圆柱

bool PrimitiveMeshBuilder::appendCylinder(const QVector3D& start, const QVector3D& end,
                                          float radius, const QVector3D& color, int segments)
{
    if (!validRadius(radius) || !validSegments(segments))
    {
        qWarning() << "PrimitiveMeshBuilder appendCylinder failed: invalid parameters.";
        return false;
    }

    QVector3D axis;
    QVector3D u;
    QVector3D v;
    float length = 0.0f;

    if (!buildFrame(start, end, axis, u, v, length))
    {
        qWarning() << "PrimitiveMeshBuilder appendCylinder failed: start and end are too close.";
        return false;
    }

    Q_UNUSED(length);

    const GLuint sideStart = static_cast<GLuint>(vertexCount());

    for (int i = 0; i < segments; ++i)
    {
        const float angle = TwoPi * static_cast<float>(i) / static_cast<float>(segments);
        const QVector3D normal = radialDirection(u, v, angle).normalized();

        appendVertex(start + normal * radius, normal, color);
        appendVertex(end + normal * radius, normal, color);
    }

    for (int i = 0; i < segments; ++i)
    {
        const int next = (i + 1) % segments;

        const GLuint bottom0 = sideStart + static_cast<GLuint>(i * 2);
        const GLuint top0 = bottom0 + 1;
        const GLuint bottom1 = sideStart + static_cast<GLuint>(next * 2);
        const GLuint top1 = bottom1 + 1;

        appendTriangle(bottom0, bottom1, top0);
        appendTriangle(top0, bottom1, top1);
    }

    appendCap(start, axis, u, v, radius, segments, false, color);
    appendCap(end, axis, u, v, radius, segments, true, color);

    return true;
}

/// 圆锥

bool PrimitiveMeshBuilder::appendCone(const QVector3D& baseCenter, const QVector3D& tip,
                                      float radius, const QVector3D& color, int segments)
{
    if (!validRadius(radius) || !validSegments(segments))
    {
        qWarning() << "PrimitiveMeshBuilder appendCone failed: invalid parameters.";
        return false;
    }

    QVector3D axis;
    QVector3D u;
    QVector3D v;
    float length = 0.0f;

    if (!buildFrame(baseCenter, tip, axis, u, v, length))
    {
        qWarning() << "PrimitiveMeshBuilder appendCone failed: baseCenter and tip are too close.";
        return false;
    }

    const float normalSlope = radius / length;

    /// 侧面

    for (int i = 0; i < segments; ++i)
    {
        const int next = (i + 1) % segments;

        const float angle0 = TwoPi * static_cast<float>(i) / static_cast<float>(segments);
        const float angle1 = TwoPi * static_cast<float>(next) / static_cast<float>(segments);
        const float middleAngle = (angle0 + angle1) * 0.5f;

        const QVector3D radial0 = radialDirection(u, v, angle0);
        const QVector3D radial1 = radialDirection(u, v, angle1);

        QVector3D normal0 = (radial0 + axis * normalSlope).normalized();
        QVector3D normal1 = (radial1 + axis * normalSlope).normalized();

        // 最后一段跨越 2π，不能直接取 angle0 / angle1 的普通平均。
        const float apexAngle = i == segments - 1
            ? angle0 + (TwoPi - angle0) * 0.5f
            : middleAngle;

        const QVector3D apexRadial = radialDirection(u, v, apexAngle);
        const QVector3D apexNormal = (apexRadial + axis * normalSlope).normalized();

        const GLuint base0 = appendVertex(baseCenter + radial0 * radius, normal0, color);
        const GLuint base1 = appendVertex(baseCenter + radial1 * radius, normal1, color);
        const GLuint apex = appendVertex(tip, apexNormal, color);

        appendTriangle(base0, base1, apex);
    }

    /// 底面

    appendCap(baseCenter, axis, u, v, radius, segments, false, color);

    return true;
}

/// 圆台

bool PrimitiveMeshBuilder::appendFrustum(const QVector3D& bottomCenter, const QVector3D& topCenter,
                                         float bottomRadius, float topRadius,
                                         const QVector3D& color, int segments)
{
    if (!validRadius(bottomRadius) || !validRadius(topRadius) || !validSegments(segments))
    {
        qWarning() << "PrimitiveMeshBuilder appendFrustum failed: invalid parameters.";
        return false;
    }

    QVector3D axis;
    QVector3D u;
    QVector3D v;
    float length = 0.0f;

    if (!buildFrame(bottomCenter, topCenter, axis, u, v, length))
    {
        qWarning() << "PrimitiveMeshBuilder appendFrustum failed: centers are too close.";
        return false;
    }

    const float normalSlope = (bottomRadius - topRadius) / length;
    const GLuint sideStart = static_cast<GLuint>(vertexCount());

    /// 侧面

    for (int i = 0; i < segments; ++i)
    {
        const float angle = TwoPi * static_cast<float>(i) / static_cast<float>(segments);
        const QVector3D radial = radialDirection(u, v, angle);
        const QVector3D normal = (radial + axis * normalSlope).normalized();

        appendVertex(bottomCenter + radial * bottomRadius, normal, color);
        appendVertex(topCenter + radial * topRadius, normal, color);
    }

    for (int i = 0; i < segments; ++i)
    {
        const int next = (i + 1) % segments;

        const GLuint bottom0 = sideStart + static_cast<GLuint>(i * 2);
        const GLuint top0 = bottom0 + 1;
        const GLuint bottom1 = sideStart + static_cast<GLuint>(next * 2);
        const GLuint top1 = bottom1 + 1;

        appendTriangle(bottom0, bottom1, top0);
        appendTriangle(top0, bottom1, top1);
    }

    /// 上下面

    appendCap(bottomCenter, axis, u, v, bottomRadius, segments, false, color);
    appendCap(topCenter, axis, u, v, topRadius, segments, true, color);

    return true;
}

/// 长方体

bool PrimitiveMeshBuilder::appendBox(const QVector3D& center, const QVector3D& size, const QVector3D& color)
{
    if (size.x() <= 0.0f || size.y() <= 0.0f || size.z() <= 0.0f)
    {
        qWarning() << "PrimitiveMeshBuilder appendBox failed: size must be positive.";
        return false;
    }

    const float x = size.x() * 0.5f;
    const float y = size.y() * 0.5f;
    const float z = size.z() * 0.5f;

    const QVector3D p000 = center + QVector3D(-x, -y, -z);
    const QVector3D p001 = center + QVector3D(-x, -y,  z);
    const QVector3D p010 = center + QVector3D(-x,  y, -z);
    const QVector3D p011 = center + QVector3D(-x,  y,  z);
    const QVector3D p100 = center + QVector3D( x, -y, -z);
    const QVector3D p101 = center + QVector3D( x, -y,  z);
    const QVector3D p110 = center + QVector3D( x,  y, -z);
    const QVector3D p111 = center + QVector3D( x,  y,  z);

    struct Face
    {
        QVector3D p0;
        QVector3D p1;
        QVector3D p2;
        QVector3D p3;
        QVector3D normal;
    };

    const Face faces[] =
    {
        { p100, p110, p111, p101, QVector3D( 1.0f,  0.0f,  0.0f) }, // +X
        { p001, p011, p010, p000, QVector3D(-1.0f,  0.0f,  0.0f) }, // -X
        { p010, p011, p111, p110, QVector3D( 0.0f,  1.0f,  0.0f) }, // +Y
        { p001, p000, p100, p101, QVector3D( 0.0f, -1.0f,  0.0f) }, // -Y
        { p001, p101, p111, p011, QVector3D( 0.0f,  0.0f,  1.0f) }, // +Z
        { p100, p000, p010, p110, QVector3D( 0.0f,  0.0f, -1.0f) }  // -Z
    };

    for (int i = 0; i < 6; ++i)
    {
        const GLuint a = appendVertex(faces[i].p0, faces[i].normal, color);
        const GLuint b = appendVertex(faces[i].p1, faces[i].normal, color);
        const GLuint c = appendVertex(faces[i].p2, faces[i].normal, color);
        const GLuint d = appendVertex(faces[i].p3, faces[i].normal, color);

        appendTriangle(a, b, c);
        appendTriangle(a, c, d);
    }

    return true;
}

/// 多棱柱

bool PrimitiveMeshBuilder::appendPrism(const QVector3D& bottomCenter, const QVector3D& topCenter,
                                       float radius, int sideCount, const QVector3D& color)
{
    return appendPolyFrustum(bottomCenter, topCenter, radius, radius, sideCount, color);
}

/// 多棱锥

bool PrimitiveMeshBuilder::appendPyramid(const QVector3D& baseCenter, const QVector3D& tip,
                                         float radius, int sideCount, const QVector3D& color)
{
    if (!validRadius(radius) || !validSegments(sideCount))
    {
        qWarning() << "PrimitiveMeshBuilder appendPyramid failed: invalid parameters.";
        return false;
    }

    QVector3D axis;
    QVector3D u;
    QVector3D v;
    float length = 0.0f;

    if (!buildFrame(baseCenter, tip, axis, u, v, length))
    {
        qWarning() << "PrimitiveMeshBuilder appendPyramid failed: baseCenter and tip are too close.";
        return false;
    }

    Q_UNUSED(length);

    /// 侧面

    for (int i = 0; i < sideCount; ++i)
    {
        const int next = (i + 1) % sideCount;

        const float angle0 = TwoPi * static_cast<float>(i) / static_cast<float>(sideCount);
        const float angle1 = TwoPi * static_cast<float>(next) / static_cast<float>(sideCount);

        const QVector3D p0 = baseCenter + radialDirection(u, v, angle0) * radius;
        const QVector3D p1 = baseCenter + radialDirection(u, v, angle1) * radius;

        QVector3D normal = QVector3D::crossProduct(p1 - p0, tip - p0);

        if (normal.lengthSquared() <= Epsilon * Epsilon)
            return false;

        normal.normalize();

        const GLuint a = appendVertex(p0, normal, color);
        const GLuint b = appendVertex(p1, normal, color);
        const GLuint c = appendVertex(tip, normal, color);

        appendTriangle(a, b, c);
    }

    /// 底面

    appendCap(baseCenter, axis, u, v, radius, sideCount, false, color);

    return true;
}

/// 多棱台

bool PrimitiveMeshBuilder::appendPolyFrustum(const QVector3D& bottomCenter, const QVector3D& topCenter,
                                             float bottomRadius, float topRadius, int sideCount,
                                             const QVector3D& color)
{
    if (!validRadius(bottomRadius) || !validRadius(topRadius) || !validSegments(sideCount))
    {
        qWarning() << "PrimitiveMeshBuilder appendPolyFrustum failed: invalid parameters.";
        return false;
    }

    QVector3D axis;
    QVector3D u;
    QVector3D v;
    float length = 0.0f;

    if (!buildFrame(bottomCenter, topCenter, axis, u, v, length))
    {
        qWarning() << "PrimitiveMeshBuilder appendPolyFrustum failed: centers are too close.";
        return false;
    }

    Q_UNUSED(length);

    /// 侧面
    /// 多棱体每个面使用独立顶点，保留平面法线。

    for (int i = 0; i < sideCount; ++i)
    {
        const int next = (i + 1) % sideCount;

        const float angle0 = TwoPi * static_cast<float>(i) / static_cast<float>(sideCount);
        const float angle1 = TwoPi * static_cast<float>(next) / static_cast<float>(sideCount);

        const QVector3D radial0 = radialDirection(u, v, angle0);
        const QVector3D radial1 = radialDirection(u, v, angle1);

        const QVector3D bottom0 = bottomCenter + radial0 * bottomRadius;
        const QVector3D bottom1 = bottomCenter + radial1 * bottomRadius;
        const QVector3D top0 = topCenter + radial0 * topRadius;
        const QVector3D top1 = topCenter + radial1 * topRadius;

        QVector3D normal = QVector3D::crossProduct(bottom1 - bottom0, top0 - bottom0);

        if (normal.lengthSquared() <= Epsilon * Epsilon)
            return false;

        normal.normalize();

        const GLuint a = appendVertex(bottom0, normal, color);
        const GLuint b = appendVertex(bottom1, normal, color);
        const GLuint c = appendVertex(top1, normal, color);
        const GLuint d = appendVertex(top0, normal, color);

        appendTriangle(a, b, d);
        appendTriangle(d, b, c);
    }

    /// 上下面

    appendCap(bottomCenter, axis, u, v, bottomRadius, sideCount, false, color);
    appendCap(topCenter, axis, u, v, topRadius, sideCount, true, color);

    return true;
}