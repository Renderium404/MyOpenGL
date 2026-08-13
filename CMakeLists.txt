# CMake 最低版本要求
cmake_minimum_required(VERSION 3.5)

# 定义项目，PROJECT_NAME 的值将为 MyOpenGL
project(MyOpenGL)

# 查找 Qt5：
# 要求版本 >= 5.8
# REQUIRED：找不到时直接终止 CMake 配置
# COMPONENTS Widgets：需要 Qt Widgets 模块
find_package(Qt5 5.8 REQUIRED COMPONENTS Widgets)


set(CMAKE_AUTOMOC ON)       #是否自动运行 Qt 的 moc 工具
set(CMAKE_AUTOUIC ON)       #是否自动使用 Qt 的 uic 处理 .ui 文件
set(CMAKE_AUTORCC ON)       #是否自动使用 Qt 的 rcc 处理 .qrc 资源文件


# 创建名为 MyOpenGL 的可执行程序 Target
# 当前 Target 包含 main.cpp
add_executable(${PROJECT_NAME}
    main.cpp
    OpenGLWidget.cpp
    OpenGLWidget.h
    resources/resources.qrc
)

# 为 MyOpenGL Target 添加链接依赖
# PRIVATE：Qt5::Widgets 仅作为当前 Target 的依赖
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    Qt5::Widgets
)

# ============================================================
# 新框架
# ============================================================

add_subdirectory(MyOpenGL)

# ============================================================
# 测试沙盒
# ============================================================

add_subdirectory(SandBox)