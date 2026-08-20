# ============================================================
# SandBox
#
# 当前目录下每一个 .cpp 都作为一个独立测试程序。
# 每个 .cpp 必须包含自己的 main()。
# ============================================================

file(GLOB SANDBOX_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

foreach(SANDBOX_SOURCE ${SANDBOX_SOURCES})

    # 例如：
    # TestCamera.cpp -> TestCamera
    get_filename_component(SANDBOX_NAME
        "${SANDBOX_SOURCE}"
        NAME_WE
    )

    # CMake Target 使用前缀，避免和项目中的其他 Target 重名。
    set(SANDBOX_TARGET "SandBox_${SANDBOX_NAME}")

    add_executable(${SANDBOX_TARGET}
        "${SANDBOX_SOURCE}"
    )
    
    target_link_libraries(${SANDBOX_TARGET}
        PRIVATE
        MyOpenGLViewer
        Qt5::Widgets
    )

    # 最终生成的 exe 仍然叫 TestCamera.exe，
    # 而不是 SandBox_TestCamera.exe。
    set_target_properties(${SANDBOX_TARGET}
        PROPERTIES
        OUTPUT_NAME "${SANDBOX_NAME}"
    )

endforeach()