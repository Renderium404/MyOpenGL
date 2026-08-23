#include "SimpleModeling.h"

#include "MyOpenGL/Resource/BufferGeometry.h"

#include <QDebug>
#include <QVector3D>

#include <cmath>
#include <vector>

namespace
{
const float Pi = 3.14159265358979323846f;
const float TwoPi = Pi * 2.0f;

void appendVertex(std::vector<GLfloat>& vertices, const QVector3D& position, const QVector3D& normal)
{
    vertices.push_back(position.x());
    vertices.push_back(position.y());
    vertices.push_back(position.z());
    vertices.push_back(normal.x());
    vertices.push_back(normal.y());
    vertices.push_back(normal.z());
}

BufferGeometry* buildGeometry(const QString& name, const std::vector<GLfloat>& vertices, const std::vector<GLuint>& indices)
{
    BufferGeometry* geometry = new BufferGeometry(name, BufferUsage::Static, RenderType::Triangles);

    std::vector<GeometryVertexAttribute> attributes;

    GeometryVertexAttribute position;
    position.location = GeometryAttribute::Position;
    position.componentCount = 3;
    position.valueOffset = 0;
    attributes.push_back(position);

    GeometryVertexAttribute normal;
    normal.location = GeometryAttribute::Normal;
    normal.componentCount = 3;
    normal.valueOffset = 3;
    attributes.push_back(normal);

    geometry->setVertexLayout(6, attributes);
    geometry->setVertexData(vertices);
    geometry->setIndexData(indices);

    return geometry;
}

void appendCap(std::vector<GLfloat>& vertices,
               std::vector<GLuint>& indices,
               float radius,
               float y,
               int segments,
               bool top)
{
    const QVector3D normal = top ? QVector3D(0.0f, 1.0f, 0.0f) : QVector3D(0.0f, -1.0f, 0.0f);

    const GLuint centerIndex = static_cast<GLuint>(vertices.size() / 6);
    appendVertex(vertices, QVector3D(0.0f, y, 0.0f), normal);

    const GLuint ringStart = static_cast<GLuint>(vertices.size() / 6);

    for (int i = 0; i <= segments; ++i)
    {
        const float angle = TwoPi * static_cast<float>(i) / static_cast<float>(segments);
        const float x = radius * std::cos(angle);
        const float z = radius * std::sin(angle);
        appendVertex(vertices, QVector3D(x, y, z), normal);
    }

    for (int i = 0; i < segments; ++i)
    {
        const GLuint current = ringStart + static_cast<GLuint>(i);
        const GLuint next = current + 1;

        if (top)
        {
            indices.push_back(centerIndex);
            indices.push_back(next);
            indices.push_back(current);
        }
        else
        {
            indices.push_back(centerIndex);
            indices.push_back(current);
            indices.push_back(next);
        }
    }
}

bool validRadius(float radius)
{
    return radius > 0.0f;
}

bool validHeight(float height)
{
    return height > 0.0f;
}

bool validSegments(int segments)
{
    return segments >= 3;
}

bool validSideCount(int sideCount)
{
    return sideCount >= 3;
}
}

/// Sphere

BufferGeometry* SimpleModeling::createSphere(const QString& name, float radius, int segments, int rings)
{
    if (!validRadius(radius) || segments < 3 || rings < 2)
    {
        qWarning() << "SimpleModeling createSphere failed: invalid parameters.";
        return 0;
    }

    std::vector<GLfloat> vertices;
    std::vector<GLuint> indices;

    for (int ring = 0; ring <= rings; ++ring)
    {
        const float v = static_cast<float>(ring) / static_cast<float>(rings);
        const float phi = Pi * v;

        const float y = std::cos(phi);
        const float ringRadius = std::sin(phi);

        for (int segment = 0; segment <= segments; ++segment)
        {
            const float u = static_cast<float>(segment) / static_cast<float>(segments);
            const float theta = TwoPi * u;

            QVector3D normal(
                ringRadius * std::cos(theta),
                y,
                ringRadius * std::sin(theta));

            normal.normalize();

            appendVertex(vertices, normal * radius, normal);
        }
    }

    const int rowSize = segments + 1;

    for (int ring = 0; ring < rings; ++ring)
    {
        for (int segment = 0; segment < segments; ++segment)
        {
            const GLuint a = static_cast<GLuint>(ring * rowSize + segment);
            const GLuint b = static_cast<GLuint>((ring + 1) * rowSize + segment);
            const GLuint c = b + 1;
            const GLuint d = a + 1;

            if (ring != 0)
            {
                indices.push_back(a);
                indices.push_back(b);
                indices.push_back(d);
            }

            if (ring != rings - 1)
            {
                indices.push_back(d);
                indices.push_back(b);
                indices.push_back(c);
            }
        }
    }

    return buildGeometry(name, vertices, indices);
}

/// Cylinder

BufferGeometry* SimpleModeling::createCylinder(const QString& name, float radius, float height, int segments)
{
    return createFrustum(name, radius, radius, height, segments);
}

/// Cone

BufferGeometry* SimpleModeling::createCone(const QString& name, float radius, float height, int segments)
{
    if (!validRadius(radius) || !validHeight(height) || !validSegments(segments))
    {
        qWarning() << "SimpleModeling createCone failed: invalid parameters.";
        return 0;
    }

    std::vector<GLfloat> vertices;
    std::vector<GLuint> indices;

    const float bottomY = -height * 0.5f;
    const float topY = height * 0.5f;
    const float normalY = radius / height;

    /// 侧面

    for (int i = 0; i <= segments; ++i)
    {
        const float angle = TwoPi * static_cast<float>(i) / static_cast<float>(segments);
        const float x = std::cos(angle);
        const float z = std::sin(angle);

        QVector3D normal(x, normalY, z);
        normal.normalize();

        appendVertex(vertices, QVector3D(x * radius, bottomY, z * radius), normal);
        appendVertex(vertices, QVector3D(0.0f, topY, 0.0f), normal);
    }

    for (int i = 0; i < segments; ++i)
    {
        const GLuint bottom0 = static_cast<GLuint>(i * 2);
        const GLuint apex0 = bottom0 + 1;
        const GLuint bottom1 = bottom0 + 2;

        indices.push_back(bottom0);
        indices.push_back(bottom1);
        indices.push_back(apex0);
    }

    /// 底面

    appendCap(vertices, indices, radius, bottomY, segments, false);

    return buildGeometry(name, vertices, indices);
}

/// Frustum

BufferGeometry* SimpleModeling::createFrustum(const QString& name,
                                              float bottomRadius,
                                              float topRadius,
                                              float height,
                                              int segments)
{
    if (!validRadius(bottomRadius) || !validRadius(topRadius) || !validHeight(height) || !validSegments(segments))
    {
        qWarning() << "SimpleModeling createFrustum failed: invalid parameters.";
        return 0;
    }

    std::vector<GLfloat> vertices;
    std::vector<GLuint> indices;

    const float bottomY = -height * 0.5f;
    const float topY = height * 0.5f;
    const float normalY = (bottomRadius - topRadius) / height;

    /// 侧面

    for (int i = 0; i <= segments; ++i)
    {
        const float angle = TwoPi * static_cast<float>(i) / static_cast<float>(segments);
        const float x = std::cos(angle);
        const float z = std::sin(angle);

        QVector3D normal(x, normalY, z);
        normal.normalize();

        appendVertex(vertices, QVector3D(x * bottomRadius, bottomY, z * bottomRadius), normal);
        appendVertex(vertices, QVector3D(x * topRadius, topY, z * topRadius), normal);
    }

    for (int i = 0; i < segments; ++i)
    {
        const GLuint bottom0 = static_cast<GLuint>(i * 2);
        const GLuint top0 = bottom0 + 1;
        const GLuint bottom1 = bottom0 + 2;
        const GLuint top1 = bottom0 + 3;

        indices.push_back(bottom0);
        indices.push_back(bottom1);
        indices.push_back(top0);

        indices.push_back(top0);
        indices.push_back(bottom1);
        indices.push_back(top1);
    }

    /// 上下面

    appendCap(vertices, indices, bottomRadius, bottomY, segments, false);
    appendCap(vertices, indices, topRadius, topY, segments, true);

    return buildGeometry(name, vertices, indices);
}

/// Box

BufferGeometry* SimpleModeling::createBox(const QString& name, float width, float height, float depth)
{
    if (width <= 0.0f || height <= 0.0f || depth <= 0.0f)
    {
        qWarning() << "SimpleModeling createBox failed: invalid parameters.";
        return 0;
    }

    const float x = width * 0.5f;
    const float y = height * 0.5f;
    const float z = depth * 0.5f;

    std::vector<GLfloat> vertices;
    std::vector<GLuint> indices;

    const QVector3D positions[] =
    {
        // +X
        QVector3D( x, -y, -z), QVector3D( x,  y, -z),
        QVector3D( x,  y,  z), QVector3D( x, -y,  z),

        // -X
        QVector3D(-x, -y,  z), QVector3D(-x,  y,  z),
        QVector3D(-x,  y, -z), QVector3D(-x, -y, -z),

        // +Y
        QVector3D(-x,  y, -z), QVector3D(-x,  y,  z),
        QVector3D( x,  y,  z), QVector3D( x,  y, -z),

        // -Y
        QVector3D(-x, -y,  z), QVector3D(-x, -y, -z),
        QVector3D( x, -y, -z), QVector3D( x, -y,  z),

        // +Z
        QVector3D( x, -y,  z), QVector3D( x,  y,  z),
        QVector3D(-x,  y,  z), QVector3D(-x, -y,  z),

        // -Z
        QVector3D(-x, -y, -z), QVector3D(-x,  y, -z),
        QVector3D( x,  y, -z), QVector3D( x, -y, -z)
    };

    const QVector3D normals[] =
    {
        QVector3D( 1.0f,  0.0f,  0.0f),
        QVector3D(-1.0f,  0.0f,  0.0f),
        QVector3D( 0.0f,  1.0f,  0.0f),
        QVector3D( 0.0f, -1.0f,  0.0f),
        QVector3D( 0.0f,  0.0f,  1.0f),
        QVector3D( 0.0f,  0.0f, -1.0f)
    };

    for (int face = 0; face < 6; ++face)
    {
        for (int vertex = 0; vertex < 4; ++vertex)
            appendVertex(vertices, positions[face * 4 + vertex], normals[face]);

        const GLuint base = static_cast<GLuint>(face * 4);

        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);

        indices.push_back(base);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }

    return buildGeometry(name, vertices, indices);
}

/// Prism

BufferGeometry* SimpleModeling::createPrism(const QString& name, int sideCount, float radius, float height)
{
    return createPolyFrustum(name, sideCount, radius, radius, height);
}

/// Pyramid

BufferGeometry* SimpleModeling::createPyramid(const QString& name, int sideCount, float radius, float height)
{
    if (!validSideCount(sideCount) || !validRadius(radius) || !validHeight(height))
    {
        qWarning() << "SimpleModeling createPyramid failed: invalid parameters.";
        return 0;
    }

    std::vector<GLfloat> vertices;
    std::vector<GLuint> indices;

    const float bottomY = -height * 0.5f;
    const float topY = height * 0.5f;
    const QVector3D apex(0.0f, topY, 0.0f);

    /// 侧面

    for (int i = 0; i < sideCount; ++i)
    {
        const float angle0 = TwoPi * static_cast<float>(i) / static_cast<float>(sideCount);
        const float angle1 = TwoPi * static_cast<float>(i + 1) / static_cast<float>(sideCount);

        const QVector3D p0(radius * std::cos(angle0), bottomY, radius * std::sin(angle0));
        const QVector3D p1(radius * std::cos(angle1), bottomY, radius * std::sin(angle1));

        QVector3D normal = QVector3D::crossProduct(p1 - p0, apex - p0).normalized();

        const GLuint base = static_cast<GLuint>(vertices.size() / 6);

        appendVertex(vertices, p0, normal);
        appendVertex(vertices, p1, normal);
        appendVertex(vertices, apex, normal);

        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
    }

    /// 底面

    appendCap(vertices, indices, radius, bottomY, sideCount, false);

    return buildGeometry(name, vertices, indices);
}

/// PolyFrustum

BufferGeometry* SimpleModeling::createPolyFrustum(const QString& name,
                                                  int sideCount,
                                                  float bottomRadius,
                                                  float topRadius,
                                                  float height)
{
    if (!validSideCount(sideCount) ||
        !validRadius(bottomRadius) ||
        !validRadius(topRadius) ||
        !validHeight(height))
    {
        qWarning() << "SimpleModeling createPolyFrustum failed: invalid parameters.";
        return 0;
    }

    std::vector<GLfloat> vertices;
    std::vector<GLuint> indices;

    const float bottomY = -height * 0.5f;
    const float topY = height * 0.5f;

    /// 侧面

    for (int i = 0; i < sideCount; ++i)
    {
        const float angle0 = TwoPi * static_cast<float>(i) / static_cast<float>(sideCount);
        const float angle1 = TwoPi * static_cast<float>(i + 1) / static_cast<float>(sideCount);

        const QVector3D bottom0(bottomRadius * std::cos(angle0), bottomY, bottomRadius * std::sin(angle0));
        const QVector3D bottom1(bottomRadius * std::cos(angle1), bottomY, bottomRadius * std::sin(angle1));
        const QVector3D top0(topRadius * std::cos(angle0), topY, topRadius * std::sin(angle0));
        const QVector3D top1(topRadius * std::cos(angle1), topY, topRadius * std::sin(angle1));

        QVector3D normal = QVector3D::crossProduct(bottom1 - bottom0, top0 - bottom0).normalized();

        const GLuint base = static_cast<GLuint>(vertices.size() / 6);

        appendVertex(vertices, bottom0, normal);
        appendVertex(vertices, bottom1, normal);
        appendVertex(vertices, top1, normal);
        appendVertex(vertices, top0, normal);

        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 3);

        indices.push_back(base + 3);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
    }

    /// 上下面

    appendCap(vertices, indices, bottomRadius, bottomY, sideCount, false);
    appendCap(vertices, indices, topRadius, topY, sideCount, true);

    return buildGeometry(name, vertices, indices);
}