cmake_minimum_required(VERSION 3.16)

project(SimpleEngine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

enable_testing()

find_package(Threads REQUIRED)

add_library(engine_core STATIC
    src/Engine.cpp
    src/AudioSystem.cpp
    src/Renderer.cpp
    src/Scene.cpp
    src/PhysicsSystem.cpp
    src/Memory.cpp
    src/Game.cpp
    src/GameConfig.cpp
    src/JobSystem.cpp
    src/GpuResourceManager.cpp
    src/RenderGraph.cpp
    src/OpenGLContext.cpp
    src/RenderCommandQueue.cpp
    src/ResourceManager.cpp
    src/SceneSerializer.cpp
    src/Prefab.cpp
    src/ScriptSystem.cpp
    src/InputMap.cpp
    src/PluginSystem.cpp
    src/glad.c
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(engine_core PUBLIC
    include
    include/engine
)

target_include_directories(engine_core PRIVATE
    imgui
    imgui/backends
)

target_link_libraries(engine_core PUBLIC
    glfw3
    opengl32
    gdi32
    winmm
    Threads::Threads
)

add_executable(engine
    src/main.cpp
    src/DemoScene.cpp
    src/DemoGame.cpp
)

target_include_directories(engine PRIVATE
    imgui
    imgui/backends
)

target_link_libraries(engine PRIVATE engine_core)

add_executable(engine_tests
    tests/EngineCoreTests.cpp
)

target_link_libraries(engine_tests PRIVATE engine_core)

add_test(NAME engine_core_tests COMMAND engine_tests)

add_executable(snake_sample
    samples/snake/src/SnakeGame.cpp
)

target_link_libraries(snake_sample PRIVATE engine_core)

add_custom_command(TARGET engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:engine>/assets
)

add_custom_target(package_game
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
        -File ${CMAKE_SOURCE_DIR}/tools/package.ps1
        -BuildDir ${CMAKE_BINARY_DIR}
        -OutputDir ${CMAKE_SOURCE_DIR}/dist
        -SkipBuild
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS engine
    COMMENT "Packaging engine executable, runtime DLLs, and assets"
    VERBATIM
)
