add_library(MyOpenGLCore STATIC
    Core/Resource.cpp
    Core/Resource.h
    Core/ResourceManager.cpp
    Core/ResourceManager.h

    Resource/Geometry.cpp
    Resource/Geometry.h
    Resource/BufferGeometry.cpp
    Resource/BufferGeometry.h
    Resource/Curve.cpp
    Resource/Curve.h
    Resource/ExternalGeometryData.h
    Resource/ExternalGeometryDataSource.h
    Resource/ExternalGeometry.cpp
    Resource/ExternalGeometry.h
    Resource/ExternalGpuGeometryData.h
    Resource/ExternalGpuGeometryDataSource.h
    Resource/ExternalGpuGeometry.cpp
    Resource/ExternalGpuGeometry.h
    Resource/Texture.cpp
    Resource/Texture.h

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
    Scene/Transform.cpp
    Scene/Transform.h
    Scene/RenderPart.cpp
    Scene/RenderPart.h
    Scene/RenderPartUpdate.cpp
    Scene/RenderPartUpdate.h
    Scene/RenderItem.cpp
    Scene/RenderItem.h
    Scene/Scene.cpp
    Scene/Scene.h

    Render/MyOpenGLContext.cpp
    Render/MyOpenGLContext.h
    Render/RenderContext.cpp
    Render/RenderContext.h
    Render/RenderState.cpp
    Render/RenderState.h
    Render/ShaderProgram.cpp
    Render/ShaderProgram.h
    Render/Renderer.cpp
    Render/Renderer.h
)

target_include_directories(MyOpenGLCore
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(MyOpenGLCore
    PUBLIC
    Qt5::Core
    Qt5::Gui
)

# ============================================================
# Qt Widgets Viewer
# ============================================================

# Core 保持不依赖 Qt Widgets。
# Viewer 负责窗口、输入、Scene 渲染编排以及后续 Grid、Axis、
# ViewNavigation 等 Viewer 级系统显示对象。
add_library(MyOpenGLViewer STATIC
    Viewer/OpenGLViewerWidget.cpp
    Viewer/OpenGLViewerWidget.h

    Viewer/Modeling/SimpleModeling.cpp
    Viewer/Modeling/SimpleModeling.h
    Viewer/Modeling/PrimitiveMeshBuilder.cpp
    Viewer/Modeling/PrimitiveMeshBuilder.h

    Viewer/Geometry/CoordinateSystemGeometry.cpp
    Viewer/Geometry/CoordinateSystemGeometry.h
    Viewer/Geometry/GridPlaneGeometry.cpp
    Viewer/Geometry/GridPlaneGeometry.h
    Viewer/Geometry/ViewNavigationGeometry.cpp
    Viewer/Geometry/ViewNavigationGeometry.h

    Viewer/System/CoordinateSystem.cpp
    Viewer/System/CoordinateSystem.h
    Viewer/System/ViewNavigation.cpp
    Viewer/System/ViewNavigation.h
)

target_include_directories(MyOpenGLViewer
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(MyOpenGLViewer
    PUBLIC
    MyOpenGLCore
    Qt5::Widgets
)