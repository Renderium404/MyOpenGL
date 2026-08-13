add_library(MyOpenGLCore STATIC
    Core/Resource.cpp
    Core/Resource.h
    Core/ResourceManager.cpp
    Core/ResourceManager.h

    Resource/RenderableMesh.cpp
    Resource/RenderableMesh.h
    Resource/ExternalMeshData.h
    Resource/ExternalMeshDataSource.h
    Resource/MeshResource.cpp
    Resource/MeshResource.h
    Resource/ExternalMeshResource.cpp
    Resource/ExternalMeshResource.h
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