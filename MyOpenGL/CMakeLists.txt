add_library(MyOpenGLCore STATIC
    Core/Resource.cpp
    Core/Resource.h
    Core/ResourceManager.cpp
    Core/ResourceManager.h

    Resource/RenderableObject.cpp
    Resource/RenderableObject.h
    Resource/ExternalMeshData.h
    Resource/ExternalMeshDataSource.h
    Resource/ExternalGpuMeshData.h
    Resource/ExternalGpuMeshDataSource.h
    Resource/MeshResource.cpp
    Resource/MeshResource.h
    Resource/ExternalMeshResource.cpp
    Resource/ExternalMeshResource.h
    Resource/ExternalGpuMeshResource.cpp
    Resource/ExternalGpuMeshResource.h
    Resource/TextureResource.cpp
    Resource/TextureResource.h
    Resource/CurveResource.cpp
    Resource/CurveResource.h
    Resource/CoordinateSystemResource.cpp
    Resource/CoordinateSystemResource.h
    Resource/ViewNavigationResource.cpp
    Resource/ViewNavigationResource.h
    Resource/GridPlaneResource.cpp
    Resource/GridPlaneResource.h

    Material/Material.cpp
    Material/Material.h
    Material/MaterialManager.cpp
    Material/MaterialManager.h

    Camera/Camera.cpp
    Camera/Camera.h
    Camera/CameraManager.cpp
    Camera/CameraManager.h

    Light/Light.cpp
    Light/Light.h
    Light/LightManager.cpp
    Light/LightManager.h

    Scene/AxisAlignedBoundingBox.cpp
    Scene/AxisAlignedBoundingBox.h
    Scene/PrimitivePicking.cpp
    Scene/PrimitivePicking.h
    Scene/MeshResourcePrimitivePickSource.cpp
    Scene/MeshResourcePrimitivePickSource.h
    Scene/Transform.cpp
    Scene/Transform.h
    Scene/RenderItem.cpp
    Scene/RenderItem.h
    Scene/Scene.cpp
    Scene/Scene.h

    Render/RenderContext.cpp
    Render/RenderContext.h
    Render/ShaderProgram.cpp
    Render/ShaderProgram.h
    Render/Renderer.cpp
    Render/Renderer.h

)

target_include_directories(MyOpenGLCore
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(MyOpenGLCore
    PUBLIC
    Qt5::Core
    Qt5::Gui
)

# ============================================================
# Qt Widgets Viewer
# ============================================================

# Core 保持不依赖 Qt Widgets；可复用 QOpenGLWidget 单独放在 Viewer Target。
add_library(MyOpenGLViewer STATIC
    Viewer/OpenGLViewerWidget.cpp
    Viewer/OpenGLViewerWidget.h
)

target_include_directories(MyOpenGLViewer
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(MyOpenGLViewer
    PUBLIC
    MyOpenGLCore
    Qt5::Widgets
)
