#include "PrimitivePicking.h"

#include <QDebug>
#include <QtMath>

#include <cfloat>
#include <cstring>

namespace
{

std::size_t componentTypeSize(GLenum type)
{
    switch (type)
    {
    case GL_FLOAT:
        return sizeof(GLfloat);
    case GL_DOUBLE:
        return sizeof(GLdouble);
    }

    return 0;
}

std::size_t indexTypeSize(GLenum type)
{
    switch (type)
    {
    case GL_UNSIGNED_BYTE:
        return sizeof(GLubyte);
    case GL_UNSIGNED_SHORT:
        return sizeof(GLushort);
    case GL_UNSIGNED_INT:
        return sizeof(GLuint);
    }

    return 0;
}

bool readPosition(const TriangleMeshPickView& view, int vertexIndex, QVector3D& position)
{
    if (vertexIndex < 0 || vertexIndex >= view.vertexCount)
        return false;

    const std::size_t componentSize = componentTypeSize(view.positionType);

    if (componentSize == 0)
        return false;

    const std::size_t byteOffset = static_cast<std::size_t>(vertexIndex) * view.vertexStride + view.positionByteOffset;
    const char* bytes = static_cast<const char*>(view.vertexData) + byteOffset;

    if (view.positionType == GL_FLOAT)
    {
        GLfloat values[3] = { 0.0f, 0.0f, 0.0f };
        std::memcpy(values, bytes, sizeof(values));
        position = QVector3D(values[0], values[1], values[2]);
        return true;
    }

    GLdouble values[3] = { 0.0, 0.0, 0.0 };
    std::memcpy(values, bytes, sizeof(values));
    position = QVector3D(static_cast<float>(values[0]), static_cast<float>(values[1]), static_cast<float>(values[2]));
    return true;
}

bool readIndex(const TriangleMeshPickView& view, int indexOffset, unsigned int& index)
{
    if (indexOffset < 0 || indexOffset >= view.indexCount)
        return false;

    const std::size_t valueSize = indexTypeSize(view.indexType);

    if (valueSize == 0)
        return false;

    const char* bytes = static_cast<const char*>(view.indexData) + static_cast<std::size_t>(indexOffset) * valueSize;

    switch (view.indexType)
    {
    case GL_UNSIGNED_BYTE:
    {
        GLubyte value = 0;
        std::memcpy(&value, bytes, sizeof(value));
        index = static_cast<unsigned int>(value);
        return true;
    }
    case GL_UNSIGNED_SHORT:
    {
        GLushort value = 0;
        std::memcpy(&value, bytes, sizeof(value));
        index = static_cast<unsigned int>(value);
        return true;
    }
    case GL_UNSIGNED_INT:
    {
        GLuint value = 0;
        std::memcpy(&value, bytes, sizeof(value));
        index = static_cast<unsigned int>(value);
        return true;
    }
    }

    return false;
}

bool validateTriangleMeshPickView(const TriangleMeshPickView& view)
{
    if (view.vertexData == 0 || view.vertexByteSize == 0 || view.vertexCount <= 0 || view.vertexStride == 0)
    {
        qWarning() << "Triangle Primitive Picking failed: vertex view is invalid.";
        return false;
    }

    const std::size_t positionComponentSize = componentTypeSize(view.positionType);

    if (positionComponentSize == 0)
    {
        qWarning() << "Triangle Primitive Picking failed: unsupported position component type:" << view.positionType;
        return false;
    }

    const std::size_t positionByteSize = positionComponentSize * 3;

    if (view.positionByteOffset > view.vertexStride || positionByteSize > view.vertexStride - view.positionByteOffset)
    {
        qWarning() << "Triangle Primitive Picking failed: position attribute exceeds vertex stride.";
        return false;
    }

    const std::size_t requiredVertexBytes =
        static_cast<std::size_t>(view.vertexCount - 1) * view.vertexStride +
        view.positionByteOffset +
        positionByteSize;

    if (requiredVertexBytes > view.vertexByteSize)
    {
        qWarning() << "Triangle Primitive Picking failed: vertex buffer is smaller than declared vertex count.";
        return false;
    }

    if (view.indexData == 0 || view.indexByteSize == 0 || view.indexCount <= 0 || view.indexCount % 3 != 0)
    {
        qWarning() << "Triangle Primitive Picking failed: triangle index view is invalid.";
        return false;
    }

    const std::size_t indexSize = indexTypeSize(view.indexType);

    if (indexSize == 0)
    {
        qWarning() << "Triangle Primitive Picking failed: unsupported index type:" << view.indexType;
        return false;
    }

    if (static_cast<std::size_t>(view.indexCount) * indexSize > view.indexByteSize)
    {
        qWarning() << "Triangle Primitive Picking failed: index buffer is smaller than declared index count.";
        return false;
    }

    return true;
}

}

PrimitivePickHit::PrimitivePickHit()
    : primitiveIndex(-1)
    , position(0.0f, 0.0f, 0.0f)
    , barycentric(0.0f, 0.0f, 0.0f)
{
    vertices[0] = QVector3D(0.0f, 0.0f, 0.0f);
    vertices[1] = QVector3D(0.0f, 0.0f, 0.0f);
    vertices[2] = QVector3D(0.0f, 0.0f, 0.0f);
}

TriangleMeshPickView::TriangleMeshPickView()
    : vertexData(0)
    , vertexByteSize(0)
    , vertexCount(0)
    , vertexStride(0)
    , positionByteOffset(0)
    , positionType(GL_FLOAT)
    , indexData(0)
    , indexByteSize(0)
    , indexCount(0)
    , indexType(GL_UNSIGNED_INT)
{
}

/// Triangle Picking

bool raycastTriangleMesh(const TriangleMeshPickView& view, const QVector3D& rayOrigin, const QVector3D& rayDirection, PrimitivePickHit& hit)
{
    hit = PrimitivePickHit();

    if (!validateTriangleMeshPickView(view))
        return false;

    // Ray Direction 必须可 Normalize；1e-12 只用于防止零向量进入 Triangle Test。
    const float directionEpsilon = 1.0e-12f;

    if (rayDirection.lengthSquared() <= directionEpsilon)
    {
        qWarning() << "Triangle Primitive Picking failed: ray direction is zero.";
        return false;
    }

    const QVector3D direction = rayDirection.normalized();
    float nearestDistance = FLT_MAX;
    const float intersectionEpsilon = 1.0e-7f;

    for (int primitiveIndex = 0; primitiveIndex < view.indexCount / 3; ++primitiveIndex)
    {
        unsigned int indices[3] = { 0, 0, 0 };

        if (!readIndex(view, primitiveIndex * 3 + 0, indices[0]) ||
            !readIndex(view, primitiveIndex * 3 + 1, indices[1]) ||
            !readIndex(view, primitiveIndex * 3 + 2, indices[2]))
        {
            qWarning() << "Triangle Primitive Picking failed: unable to read triangle indices:" << primitiveIndex;
            return false;
        }

        if (indices[0] >= static_cast<unsigned int>(view.vertexCount) ||
            indices[1] >= static_cast<unsigned int>(view.vertexCount) ||
            indices[2] >= static_cast<unsigned int>(view.vertexCount))
        {
            qWarning() << "Triangle Primitive Picking failed: triangle index exceeds vertex count:" << primitiveIndex;
            return false;
        }

        QVector3D vertices[3];

        if (!readPosition(view, static_cast<int>(indices[0]), vertices[0]) ||
            !readPosition(view, static_cast<int>(indices[1]), vertices[1]) ||
            !readPosition(view, static_cast<int>(indices[2]), vertices[2]))
        {
            qWarning() << "Triangle Primitive Picking failed: unable to read triangle vertices:" << primitiveIndex;
            return false;
        }

        const QVector3D edge1 = vertices[1] - vertices[0];
        const QVector3D edge2 = vertices[2] - vertices[0];
        const QVector3D pVector = QVector3D::crossProduct(direction, edge2);
        const float determinant = QVector3D::dotProduct(edge1, pVector);

        // 不执行 Back-face Culling；Determinant 正负只表示 Ray 从三角形哪一侧进入。
        if (qAbs(determinant) <= intersectionEpsilon)
            continue;

        const float inverseDeterminant = 1.0f / determinant;
        const QVector3D tVector = rayOrigin - vertices[0];
        const float barycentric1 = QVector3D::dotProduct(tVector, pVector) * inverseDeterminant;

        if (barycentric1 < -intersectionEpsilon || barycentric1 > 1.0f + intersectionEpsilon)
            continue;

        const QVector3D qVector = QVector3D::crossProduct(tVector, edge1);
        const float barycentric2 = QVector3D::dotProduct(direction, qVector) * inverseDeterminant;

        if (barycentric2 < -intersectionEpsilon || barycentric1 + barycentric2 > 1.0f + intersectionEpsilon)
            continue;

        const float distance = QVector3D::dotProduct(edge2, qVector) * inverseDeterminant;

        if (distance <= intersectionEpsilon || distance >= nearestDistance)
            continue;

        nearestDistance = distance;

        // Moller-Trumbore 中 u / v 分别对应 Vertex1 / Vertex2，Vertex0 权重为 1-u-v。
        hit.primitiveIndex = primitiveIndex;
        hit.position = rayOrigin + direction * distance;
        hit.barycentric = QVector3D(1.0f - barycentric1 - barycentric2, barycentric1, barycentric2);
        hit.vertices[0] = vertices[0];
        hit.vertices[1] = vertices[1];
        hit.vertices[2] = vertices[2];
    }

    return hit.primitiveIndex >= 0;
}