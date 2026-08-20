#ifndef PRIMITIVEMESHBUILDER_H
#define PRIMITIVEMESHBUILDER_H

#include <QOpenGLFunctions_3_3_Core>
#include <QVector3D>

#include <vector>

/// Viewer 系统组件使用的基础实体网格构造器。
/// 所有实体追加到同一份 Vertex / Index 数据中。
class PrimitiveMeshBuilder
{
public:
    static const int VertexStride = 9;
    static const int PositionOffset = 0;
    static const int NormalOffset = 3;
    static const int ColorOffset = 6;

    PrimitiveMeshBuilder();

    /// 数据
    void clear();
    bool isEmpty() const;
    int vertexCount() const;
    int indexCount() const;

    const std::vector<GLfloat>& vertexData() const;
    const std::vector<GLuint>& indexData() const;

    /// 圆形实体
    bool appendSphere(const QVector3D& center, float radius, const QVector3D& color, int segments = 24, int rings = 12);
    bool appendCylinder(const QVector3D& start, const QVector3D& end, float radius, const QVector3D& color, int segments = 24);
    bool appendCone(const QVector3D& baseCenter, const QVector3D& tip, float radius, const QVector3D& color, int segments = 24);
    bool appendFrustum(const QVector3D& bottomCenter, const QVector3D& topCenter, float bottomRadius, float topRadius,
                       const QVector3D& color, int segments = 24);

    /// 多面实体
    bool appendBox(const QVector3D& center, const QVector3D& size, const QVector3D& color);
    bool appendPrism(const QVector3D& bottomCenter, const QVector3D& topCenter, float radius, int sideCount,
                     const QVector3D& color);
    bool appendPyramid(const QVector3D& baseCenter, const QVector3D& tip, float radius, int sideCount,
                       const QVector3D& color);
    bool appendPolyFrustum(const QVector3D& bottomCenter, const QVector3D& topCenter, float bottomRadius,
                           float topRadius, int sideCount, const QVector3D& color);

private:
    GLuint appendVertex(const QVector3D& position, const QVector3D& normal, const QVector3D& color);
    void appendTriangle(GLuint a, GLuint b, GLuint c);
    void appendCap(const QVector3D& center, const QVector3D& axis, const QVector3D& u, const QVector3D& v,
                   float radius, int segments, bool positiveAxis, const QVector3D& color);

private:
    std::vector<GLfloat> m_vertices; // Position + Normal + Color。
    std::vector<GLuint> m_indices;   // Triangle Index。
};

#endif // PRIMITIVEMESHBUILDER_H