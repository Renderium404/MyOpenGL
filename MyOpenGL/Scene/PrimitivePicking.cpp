#include "PrimitivePicking.h"

#include <QDebug>
#include <QtMath>
#include <QVector4D>

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

bool readPosition(const void* vertexData, std::size_t vertexByteSize, int vertexCount, std::size_t vertexStride,
                  std::size_t positionByteOffset, GLenum positionType, int vertexIndex, QVector3D& position)
{
    if (vertexData == 0 || vertexIndex < 0 || vertexIndex >= vertexCount)
        return false;

    const std::size_t componentSize = componentTypeSize(positionType);

    if (componentSize == 0)
        return false;

    const std::size_t positionByteSize = componentSize * 3;
    const std::size_t byteOffset = static_cast<std::size_t>(vertexIndex) * vertexStride + positionByteOffset;

    if (byteOffset > vertexByteSize || positionByteSize > vertexByteSize - byteOffset)
        return false;

    const char* bytes = static_cast<const char*>(vertexData) + byteOffset;

    if (positionType == GL_FLOAT)
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

bool readIndex(const void* indexData, std::size_t indexByteSize, int indexCount, GLenum indexType, int indexOffset, unsigned int& index)
{
    if (indexData == 0 || indexOffset < 0 || indexOffset >= indexCount)
        return false;

    const std::size_t valueSize = indexTypeSize(indexType);

    if (valueSize == 0)
        return false;

    const std::size_t byteOffset = static_cast<std::size_t>(indexOffset) * valueSize;

    if (byteOffset > indexByteSize || valueSize > indexByteSize - byteOffset)
        return false;

    const char* bytes = static_cast<const char*>(indexData) + byteOffset;

    switch (indexType)
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

bool validateIndexedPositionView(const void* vertexData, std::size_t vertexByteSize, int vertexCount, std::size_t vertexStride,
                                 std::size_t positionByteOffset, GLenum positionType, const void* indexData,
                                 std::size_t indexByteSize, int indexCount, GLenum indexType)
{
    if (vertexData == 0 || vertexByteSize == 0 || vertexCount <= 0 || vertexStride == 0)
    {
        qWarning() << "Primitive Picking failed: vertex view is invalid.";
        return false;
    }

    const std::size_t positionComponentSize = componentTypeSize(positionType);

    if (positionComponentSize == 0)
    {
        qWarning() << "Primitive Picking failed: unsupported position component type:" << positionType;
        return false;
    }

    const std::size_t positionByteSize = positionComponentSize * 3;

    if (positionByteOffset > vertexStride || positionByteSize > vertexStride - positionByteOffset)
    {
        qWarning() << "Primitive Picking failed: position attribute exceeds vertex stride.";
        return false;
    }

    const std::size_t requiredVertexBytes =
        static_cast<std::size_t>(vertexCount - 1) * vertexStride +
        positionByteOffset +
        positionByteSize;

    if (requiredVertexBytes > vertexByteSize)
    {
        qWarning() << "Primitive Picking failed: vertex buffer is smaller than declared vertex count.";
        return false;
    }

    if (indexData == 0 || indexByteSize == 0 || indexCount <= 0)
    {
        qWarning() << "Primitive Picking failed: index view is invalid.";
        return false;
    }

    const std::size_t indexSize = indexTypeSize(indexType);

    if (indexSize == 0)
    {
        qWarning() << "Primitive Picking failed: unsupported index type:" << indexType;
        return false;
    }

    if (static_cast<std::size_t>(indexCount) * indexSize > indexByteSize)
    {
        qWarning() << "Primitive Picking failed: index buffer is smaller than declared index count.";
        return false;
    }

    return true;
}

bool projectToScreen(const QVector3D& localPosition, const PrimitivePickContext& context, QPointF& screenPosition, float& clipW, float& ndcDepth)
{
    const QVector4D clipPosition = context.localToClip * QVector4D(localPosition, 1.0f);
    const float clipEpsilon = 1.0e-7f;

    // 当前第一版只处理两个端点都位于 Camera 前方的 Segment。
    // Near Plane 穿越线段的 Clip 后续可作为边界增强补充。
    if (clipPosition.w() <= clipEpsilon)
        return false;

    clipW = clipPosition.w();

    const float inverseW = 1.0f / clipW;
    const float ndcX = clipPosition.x() * inverseW;
    const float ndcY = clipPosition.y() * inverseW;
    ndcDepth = clipPosition.z() * inverseW;

    screenPosition.setX((ndcX * 0.5f + 0.5f) * static_cast<float>(context.viewportWidth));
    screenPosition.setY((1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(context.viewportHeight));
    return true;
}

float pointSegmentDistanceSquared(const QPointF& point, const QPointF& start, const QPointF& end, float& segmentParameter)
{
    const float dx = static_cast<float>(end.x() - start.x());
    const float dy = static_cast<float>(end.y() - start.y());
    const float lengthSquared = dx * dx + dy * dy;

    if (lengthSquared <= 1.0e-12f)
    {
        segmentParameter = 0.0f;
        const float px = static_cast<float>(point.x() - start.x());
        const float py = static_cast<float>(point.y() - start.y());
        return px * px + py * py;
    }

    const float px = static_cast<float>(point.x() - start.x());
    const float py = static_cast<float>(point.y() - start.y());
    segmentParameter = (px * dx + py * dy) / lengthSquared;
    segmentParameter = qMax(0.0f, qMin(1.0f, segmentParameter));

    const float closestX = static_cast<float>(start.x()) + dx * segmentParameter;
    const float closestY = static_cast<float>(start.y()) + dy * segmentParameter;
    const float distanceX = static_cast<float>(point.x()) - closestX;
    const float distanceY = static_cast<float>(point.y()) - closestY;

    return distanceX * distanceX + distanceY * distanceY;
}

float perspectiveSegmentParameter(float screenParameter, float startW, float endW)
{
    const float denominator = (1.0f - screenParameter) * endW + screenParameter * startW;

    if (qAbs(denominator) <= 1.0e-12f)
        return screenParameter;

    const float geometryParameter = screenParameter * startW / denominator;
    return qMax(0.0f, qMin(1.0f, geometryParameter));
}

}

const char* primitivePickTypeName(PrimitivePickType type)
{
    switch (type)
    {
    case PrimitivePickTriangle:
        return "Triangle";
    case PrimitivePickLine:
        return "Line";
    }

    return "Unknown";
}

PrimitivePickContext::PrimitivePickContext()
    : rayOrigin(0.0f, 0.0f, 0.0f)
    , rayDirection(0.0f, 0.0f, -1.0f)
    , screenPosition(0.0, 0.0)
    , viewportWidth(0)
    , viewportHeight(0)
    , pixelTolerance(5.0f)
{
}

PointPickHit::PointPickHit()
    : vertexIndex(-1)
    , position(0.0f, 0.0f, 0.0f)
    , screenDistance(0.0f)
    , ndcDepth(0.0f)
{
}

PrimitivePickHit::PrimitivePickHit()
    : type(PrimitivePickTriangle)
    , primitiveIndex(-1)
    , vertexCount(0)
    , position(0.0f, 0.0f, 0.0f)
    , barycentric(0.0f, 0.0f, 0.0f)
{
    vertices[0] = QVector3D(0.0f, 0.0f, 0.0f);
    vertices[1] = QVector3D(0.0f, 0.0f, 0.0f);
    vertices[2] = QVector3D(0.0f, 0.0f, 0.0f);
}

PointPickView::PointPickView()
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

TrianglePickView::TrianglePickView()
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

LinePickView::LinePickView()
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

bool raycastTriangles(const TrianglePickView& view, const QVector3D& rayOrigin, const QVector3D& rayDirection, PrimitivePickHit& hit)
{
    hit = PrimitivePickHit();

    if (!validateIndexedPositionView(view.vertexData, view.vertexByteSize, view.vertexCount, view.vertexStride,
                                     view.positionByteOffset, view.positionType, view.indexData,
                                     view.indexByteSize, view.indexCount, view.indexType))
        return false;

    if (view.indexCount % 3 != 0)
    {
        qWarning() << "Triangle Primitive Picking failed: triangle index count is not divisible by 3.";
        return false;
    }

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

        if (!readIndex(view.indexData, view.indexByteSize, view.indexCount, view.indexType, primitiveIndex * 3 + 0, indices[0]) ||
            !readIndex(view.indexData, view.indexByteSize, view.indexCount, view.indexType, primitiveIndex * 3 + 1, indices[1]) ||
            !readIndex(view.indexData, view.indexByteSize, view.indexCount, view.indexType, primitiveIndex * 3 + 2, indices[2]))
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

        if (!readPosition(view.vertexData, view.vertexByteSize, view.vertexCount, view.vertexStride, view.positionByteOffset, view.positionType, static_cast<int>(indices[0]), vertices[0]) ||
            !readPosition(view.vertexData, view.vertexByteSize, view.vertexCount, view.vertexStride, view.positionByteOffset, view.positionType, static_cast<int>(indices[1]), vertices[1]) ||
            !readPosition(view.vertexData, view.vertexByteSize, view.vertexCount, view.vertexStride, view.positionByteOffset, view.positionType, static_cast<int>(indices[2]), vertices[2]))
        {
            qWarning() << "Triangle Primitive Picking failed: unable to read triangle vertices:" << primitiveIndex;
            return false;
        }

        const QVector3D edge1 = vertices[1] - vertices[0];
        const QVector3D edge2 = vertices[2] - vertices[0];
        const QVector3D pVector = QVector3D::crossProduct(direction, edge2);
        const float determinant = QVector3D::dotProduct(edge1, pVector);

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

        hit.type = PrimitivePickTriangle;
        hit.primitiveIndex = primitiveIndex;
        hit.vertexCount = 3;
        hit.position = rayOrigin + direction * distance;
        hit.barycentric = QVector3D(1.0f - barycentric1 - barycentric2, barycentric1, barycentric2);
        hit.vertices[0] = vertices[0];
        hit.vertices[1] = vertices[1];
        hit.vertices[2] = vertices[2];
    }

    return hit.primitiveIndex >= 0;
}

/// Point Picking

bool pickPoints(const PointPickView& view, const PrimitivePickContext& context, PointPickHit& hit)
{
    hit = PointPickHit();

    if (!validateIndexedPositionView(view.vertexData, view.vertexByteSize, view.vertexCount, view.vertexStride,
                                     view.positionByteOffset, view.positionType, view.indexData,
                                     view.indexByteSize, view.indexCount, view.indexType))
        return false;

    if (context.viewportWidth <= 0 || context.viewportHeight <= 0 || context.pixelTolerance <= 0.0f)
    {
        qWarning() << "Point Picking failed: screen picking context is invalid.";
        return false;
    }

    const float toleranceSquared = context.pixelTolerance * context.pixelTolerance;
    float bestScreenDistanceSquared = FLT_MAX;
    float bestNdcDepth = FLT_MAX;

    // 同一 Vertex 可能被多个 Triangle / Line Index 重复引用。
    // Point Picking 只测试每个实际被引用 Vertex 一次。
    std::vector<bool> visited(static_cast<std::size_t>(view.vertexCount), false);

    for (int indexOffset = 0; indexOffset < view.indexCount; ++indexOffset)
    {
        unsigned int vertexIndex = 0;

        if (!readIndex(view.indexData, view.indexByteSize, view.indexCount, view.indexType, indexOffset, vertexIndex))
        {
            qWarning() << "Point Picking failed: unable to read vertex index:" << indexOffset;
            return false;
        }

        if (vertexIndex >= static_cast<unsigned int>(view.vertexCount))
        {
            qWarning() << "Point Picking failed: vertex index exceeds vertex count:" << vertexIndex;
            return false;
        }

        if (visited[vertexIndex])
            continue;

        visited[vertexIndex] = true;

        QVector3D localPosition;

        if (!readPosition(view.vertexData, view.vertexByteSize, view.vertexCount, view.vertexStride,
                          view.positionByteOffset, view.positionType, static_cast<int>(vertexIndex), localPosition))
        {
            qWarning() << "Point Picking failed: unable to read vertex position:" << vertexIndex;
            return false;
        }

        QPointF screenPosition;
        float clipW = 0.0f;
        float ndcDepth = 0.0f;

        if (!projectToScreen(localPosition, context, screenPosition, clipW, ndcDepth))
            continue;

        if (ndcDepth < -1.0f || ndcDepth > 1.0f)
            continue;

        const float dx = static_cast<float>(context.screenPosition.x() - screenPosition.x());
        const float dy = static_cast<float>(context.screenPosition.y() - screenPosition.y());
        const float screenDistanceSquared = dx * dx + dy * dy;

        if (screenDistanceSquared > toleranceSquared)
            continue;

        const float screenDistanceEpsilon = 1.0e-4f;
        const bool betterScreenDistance = screenDistanceSquared + screenDistanceEpsilon < bestScreenDistanceSquared;
        const bool sameScreenDistance = qAbs(screenDistanceSquared - bestScreenDistanceSquared) <= screenDistanceEpsilon;

        if (!betterScreenDistance && !(sameScreenDistance && ndcDepth < bestNdcDepth))
            continue;

        bestScreenDistanceSquared = screenDistanceSquared;
        bestNdcDepth = ndcDepth;

        hit.vertexIndex = static_cast<int>(vertexIndex);
        hit.position = localPosition;
        hit.screenDistance = qSqrt(screenDistanceSquared);
        hit.ndcDepth = ndcDepth;
    }

    return hit.vertexIndex >= 0;
}

/// Line Picking

bool pickLines(const LinePickView& view, LinePickTopology topology, const PrimitivePickContext& context, PrimitivePickHit& hit)
{
    hit = PrimitivePickHit();

    if (!validateIndexedPositionView(view.vertexData, view.vertexByteSize, view.vertexCount, view.vertexStride,
                                     view.positionByteOffset, view.positionType, view.indexData,
                                     view.indexByteSize, view.indexCount, view.indexType))
        return false;

    if (context.viewportWidth <= 0 || context.viewportHeight <= 0 || context.pixelTolerance <= 0.0f)
    {
        qWarning() << "Line Primitive Picking failed: screen picking context is invalid.";
        return false;
    }

    int primitiveCount = 0;

    switch (topology)
    {
    case LinePickSegments:
        if (view.indexCount < 2 || view.indexCount % 2 != 0)
        {
            qWarning() << "Line Primitive Picking failed: GL_LINES index count must be a positive multiple of 2.";
            return false;
        }

        primitiveCount = view.indexCount / 2;
        break;

    case LinePickStrip:
        if (view.indexCount < 2)
        {
            qWarning() << "Line Primitive Picking failed: GL_LINE_STRIP requires at least 2 indices.";
            return false;
        }

        primitiveCount = view.indexCount - 1;
        break;
    }

    const float toleranceSquared = context.pixelTolerance * context.pixelTolerance;
    float bestScreenDistanceSquared = FLT_MAX;
    float bestNdcDepth = FLT_MAX;

    for (int primitiveIndex = 0; primitiveIndex < primitiveCount; ++primitiveIndex)
    {
        const int firstIndexOffset = topology == LinePickSegments ? primitiveIndex * 2 : primitiveIndex;
        const int secondIndexOffset = firstIndexOffset + 1;

        unsigned int firstIndex = 0;
        unsigned int secondIndex = 0;

        if (!readIndex(view.indexData, view.indexByteSize, view.indexCount, view.indexType, firstIndexOffset, firstIndex) ||
            !readIndex(view.indexData, view.indexByteSize, view.indexCount, view.indexType, secondIndexOffset, secondIndex))
        {
            qWarning() << "Line Primitive Picking failed: unable to read line indices:" << primitiveIndex;
            return false;
        }

        if (firstIndex >= static_cast<unsigned int>(view.vertexCount) ||
            secondIndex >= static_cast<unsigned int>(view.vertexCount))
        {
            qWarning() << "Line Primitive Picking failed: line index exceeds vertex count:" << primitiveIndex;
            return false;
        }

        QVector3D vertices[2];

        if (!readPosition(view.vertexData, view.vertexByteSize, view.vertexCount, view.vertexStride, view.positionByteOffset, view.positionType, static_cast<int>(firstIndex), vertices[0]) ||
            !readPosition(view.vertexData, view.vertexByteSize, view.vertexCount, view.vertexStride, view.positionByteOffset, view.positionType, static_cast<int>(secondIndex), vertices[1]))
        {
            qWarning() << "Line Primitive Picking failed: unable to read line vertices:" << primitiveIndex;
            return false;
        }

        QPointF screenVertices[2];
        float clipW[2] = { 0.0f, 0.0f };
        float ndcDepth[2] = { 0.0f, 0.0f };

        if (!projectToScreen(vertices[0], context, screenVertices[0], clipW[0], ndcDepth[0]) ||
            !projectToScreen(vertices[1], context, screenVertices[1], clipW[1], ndcDepth[1]))
            continue;

        // 两个端点都完全位于同一 Depth Clip Plane 外时不参与 Picking。
        if ((ndcDepth[0] < -1.0f && ndcDepth[1] < -1.0f) ||
            (ndcDepth[0] > 1.0f && ndcDepth[1] > 1.0f))
            continue;

        float screenParameter = 0.0f;
        const float screenDistanceSquared = pointSegmentDistanceSquared(context.screenPosition, screenVertices[0], screenVertices[1], screenParameter);

        if (screenDistanceSquared > toleranceSquared)
            continue;

        const float geometryParameter = perspectiveSegmentParameter(screenParameter, clipW[0], clipW[1]);
        const QVector3D localPosition = vertices[0] + (vertices[1] - vertices[0]) * geometryParameter;
        const QVector4D hitClipPosition = context.localToClip * QVector4D(localPosition, 1.0f);

        if (hitClipPosition.w() <= 1.0e-7f)
            continue;

        const float hitNdcDepth = hitClipPosition.z() / hitClipPosition.w();

        // 同一 Geometry 内优先选择鼠标屏幕距离更近的 Segment；
        // 距离近似相同时再选择更靠近 Camera 的 Segment。
        const float screenDistanceEpsilon = 1.0e-4f;
        const bool betterScreenDistance = screenDistanceSquared + screenDistanceEpsilon < bestScreenDistanceSquared;
        const bool sameScreenDistance = qAbs(screenDistanceSquared - bestScreenDistanceSquared) <= screenDistanceEpsilon;

        if (!betterScreenDistance && !(sameScreenDistance && hitNdcDepth < bestNdcDepth))
            continue;

        bestScreenDistanceSquared = screenDistanceSquared;
        bestNdcDepth = hitNdcDepth;

        hit.type = PrimitivePickLine;
        hit.primitiveIndex = primitiveIndex;
        hit.vertexCount = 2;
        hit.position = localPosition;
        hit.barycentric = QVector3D(0.0f, 0.0f, 0.0f);
        hit.vertices[0] = vertices[0];
        hit.vertices[1] = vertices[1];
        hit.vertices[2] = vertices[1];
    }

    return hit.primitiveIndex >= 0;
}
