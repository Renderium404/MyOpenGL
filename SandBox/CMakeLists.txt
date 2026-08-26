# ============================================================
# SandBox
#
# 每一个测试程序显式定义自己的 Target。
# 测试程序可以包含一个 main.cpp 和若干辅助源文件。
# ============================================================

function(add_sandbox_test SANDBOX_NAME)

    set(SANDBOX_TARGET "SandBox_${SANDBOX_NAME}")

    add_executable(${SANDBOX_TARGET}
        ${ARGN}
    )

    target_link_libraries(${SANDBOX_TARGET}
        PRIVATE
        MyOpenGLViewer
        Qt5::Widgets
    )

    # 最终生成的 exe 保持测试名称。
    set_target_properties(${SANDBOX_TARGET}
        PROPERTIES
        OUTPUT_NAME "${SANDBOX_NAME}"
    )

endfunction()

# Camera / Viewer 测试。
add_sandbox_test(TestCameraViewer
    TestCameraViewer.cpp
)

# Renderer 独立测试。
add_sandbox_test(TestRenderer
    TestRenderer.cpp
    RendererSandboxWidget.cpp
    RendererSandboxWidget.h
)