add_executable(SandBox
    main.cpp
    ModelingMesh.cpp
    ModelingMesh.h
    ModelingMeshAdapter.cpp
    ModelingMeshAdapter.h
    OpenGLSandboxWidget.cpp
    OpenGLSandboxWidget.h
)

target_link_libraries(SandBox
    PRIVATE
    MyOpenGLCore
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
)