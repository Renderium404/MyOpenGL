#-------------------------------------------------
# MyOpenGL
#
# 作为 Nestube 主工程的一部分直接参与编译。
# TARGET / TEMPLATE / DESTDIR 由主工程统一管理。
#-------------------------------------------------

QT += core gui widgets opengl

CONFIG += c++11

# 不同目录下允许存在同名 cpp，尽量保持 obj 与源码目录对应。
CONFIG += object_parallel_to_source

# MyOpenGL 内部使用 "MyOpenGL/..." 形式包含头文件，
# 因此 Include Root 必须是 MyOpenGL 的上一级目录。
INCLUDEPATH += $$PWD/..

#-------------------------------------------------
# Sources
#-------------------------------------------------

SOURCES += \
    $$PWD/Camera/Camera.cpp \
    $$PWD/Camera/CameraManager.cpp \
    $$PWD/Core/Resource.cpp \
    $$PWD/Core/ResourceManager.cpp \
    $$PWD/Item/AxisAlignedBoundingBox.cpp \
    $$PWD/Item/ItemManager.cpp \
    $$PWD/Item/RenderItem.cpp \
    $$PWD/Item/RenderPart.cpp \
    $$PWD/Item/RenderPartUpdate.cpp \
    $$PWD/Item/Transform.cpp \
    $$PWD/Light/Light.cpp \
    $$PWD/Light/LightManager.cpp \
    $$PWD/Material/Material.cpp \
    $$PWD/Material/MaterialManager.cpp \
    $$PWD/Render/MyOpenGLContext.cpp \
    $$PWD/Render/RenderContext.cpp \
    $$PWD/Render/Renderer.cpp \
    $$PWD/Render/RenderState.cpp \
    $$PWD/Render/ShaderProgram.cpp \
    $$PWD/Resource/BufferGeometry.cpp \
    $$PWD/Resource/CurveResource.cpp \
    $$PWD/Resource/ExternalGeometry.cpp \
    $$PWD/Resource/ExternalGpuGeometry.cpp \
    $$PWD/Resource/Geometry.cpp \
    $$PWD/Resource/GeometryIterator.cpp \
    $$PWD/Resource/Texture.cpp \
    $$PWD/Viewer/Geometry/CoordinateSystemGeometry.cpp \
    $$PWD/Viewer/Geometry/GridPlaneGeometry.cpp \
    $$PWD/Viewer/Geometry/ViewNavigationGeometry.cpp \
    $$PWD/Viewer/Modeling/PrimitiveMeshBuilder.cpp \
    $$PWD/Viewer/Modeling/SimpleModeling.cpp \
    $$PWD/Viewer/System/CoordinateSystem.cpp \
    $$PWD/Viewer/System/ViewNavigation.cpp \
    $$PWD/Viewer/OpenGLViewerWidget.cpp

#-------------------------------------------------
# Headers
#-------------------------------------------------

HEADERS += \
    $$PWD/Camera/Camera.h \
    $$PWD/Camera/CameraManager.h \
    $$PWD/Core/Resource.h \
    $$PWD/Core/ResourceManager.h \
    $$PWD/Item/AxisAlignedBoundingBox.h \
    $$PWD/Item/ItemManager.h \
    $$PWD/Item/RenderItem.h \
    $$PWD/Item/RenderPart.h \
    $$PWD/Item/RenderPartUpdate.h \
    $$PWD/Item/Transform.h \
    $$PWD/Light/Light.h \
    $$PWD/Light/LightManager.h \
    $$PWD/Material/Material.h \
    $$PWD/Material/MaterialManager.h \
    $$PWD/Render/MyOpenGLContext.h \
    $$PWD/Render/RenderContext.h \
    $$PWD/Render/Renderer.h \
    $$PWD/Render/RenderState.h \
    $$PWD/Render/ShaderProgram.h \
    $$PWD/Resource/BufferGeometry.h \
    $$PWD/Resource/CurveResource.h \
    $$PWD/Resource/ExternalGeometry.h \
    $$PWD/Resource/ExternalGeometryData.h \
    $$PWD/Resource/ExternalGeometryDataSource.h \
    $$PWD/Resource/ExternalGpuGeometry.h \
    $$PWD/Resource/ExternalGpuGeometryData.h \
    $$PWD/Resource/ExternalGpuGeometryDataSource.h \
    $$PWD/Resource/Geometry.h \
    $$PWD/Resource/GeometryIterator.h \
    $$PWD/Resource/Texture.h \
    $$PWD/Viewer/Geometry/CoordinateSystemGeometry.h \
    $$PWD/Viewer/Geometry/GridPlaneGeometry.h \
    $$PWD/Viewer/Geometry/ViewNavigationGeometry.h \
    $$PWD/Viewer/Modeling/PrimitiveMeshBuilder.h \
    $$PWD/Viewer/Modeling/SimpleModeling.h \
    $$PWD/Viewer/System/CoordinateSystem.h \
    $$PWD/Viewer/System/ViewNavigation.h \
    $$PWD/Viewer/OpenGLViewerWidget.h
