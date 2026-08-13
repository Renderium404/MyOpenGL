#include "RenderableMesh.h"

const char* meshPrimitiveTypeName(MeshPrimitiveType type)
{
    switch (type)
    {
    case MeshPrimitiveTriangles:
        return "Triangles";
    case MeshPrimitiveLines:
        return "Lines";
    case MeshPrimitiveLineStrip:
        return "LineStrip";
    }

    return "Unknown";
}