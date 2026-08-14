add_executable(SandBox
    main.cpp

    ModelingMesh.cpp
    ModelingMesh.h
    ModelingMeshAdapter.cpp
    ModelingMeshAdapter.h

    ModelingGpuMesh.cpp
    ModelingGpuMesh.h
    ModelingGpuMeshSync.cpp
    ModelingGpuMeshSync.h
    ModelingGpuMeshWorker.cpp
    ModelingGpuMeshWorker.h
    ModelingGpuMeshAdapter.cpp
    ModelingGpuMeshAdapter.h

    OpenGLSandboxWidget.cpp
    OpenGLSandboxWidget.h
)

target_link_libraries(SandBox
    PRIVATE
    MyOpenGLViewer
)
