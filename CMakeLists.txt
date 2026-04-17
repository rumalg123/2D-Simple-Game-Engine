cmake_minimum_required(VERSION 3.16)

project(SimpleEngine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

enable_testing()

find_package(Threads REQUIRED)

add_library(engine_core STATIC
    src/App.cpp
    src/AssetManifest.cpp
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

add_executable(flappy_bird_sample
    samples/flappy_bird/src/FlappyBirdGame.cpp
)

target_link_libraries(flappy_bird_sample PRIVATE engine_core)

add_executable(basic_game_template
    templates/basic_game/src/BasicGame.cpp
)

target_link_libraries(basic_game_template PRIVATE engine_core)

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
        -PackageName SimpleEngine
        -ExecutableName engine.exe
        -GameConfig ${CMAKE_SOURCE_DIR}/demo.game.json
        -AssetsPath ${CMAKE_SOURCE_DIR}/assets
        -ReadmePath ${CMAKE_SOURCE_DIR}/README.md
        -SkipBuild
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS engine
    COMMENT "Packaging demo executable, config, runtime DLLs, and assets"
    VERBATIM
)

add_custom_target(package_snake_sample
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
        -File ${CMAKE_SOURCE_DIR}/tools/package.ps1
        -BuildDir ${CMAKE_BINARY_DIR}
        -OutputDir ${CMAKE_SOURCE_DIR}/dist
        -PackageName SnakeSample
        -ExecutableName snake_sample.exe
        -GameConfig ${CMAKE_SOURCE_DIR}/samples/snake/snake.game.json
        -AssetsPath ${CMAKE_SOURCE_DIR}/samples/snake/assets
        -ReadmePath ${CMAKE_SOURCE_DIR}/samples/snake/README.md
        -AllowMissingAssets
        -SkipBuild
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS snake_sample
    COMMENT "Packaging Snake sample executable, config, runtime DLLs, and assets"
    VERBATIM
)

add_custom_target(package_flappy_bird_sample
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
        -File ${CMAKE_SOURCE_DIR}/tools/package.ps1
        -BuildDir ${CMAKE_BINARY_DIR}
        -OutputDir ${CMAKE_SOURCE_DIR}/dist
        -PackageName FlappyBirdSample
        -ExecutableName flappy_bird_sample.exe
        -GameConfig ${CMAKE_SOURCE_DIR}/samples/flappy_bird/flappy_bird.game.json
        -AssetsPath ${CMAKE_SOURCE_DIR}/samples/flappy_bird/assets
        -ReadmePath ${CMAKE_SOURCE_DIR}/samples/flappy_bird/README.md
        -AllowMissingAssets
        -SkipBuild
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS flappy_bird_sample
    COMMENT "Packaging Flappy Bird sample executable, config, runtime DLLs, and assets"
    VERBATIM
)

add_custom_target(package_basic_game_template
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
        -File ${CMAKE_SOURCE_DIR}/tools/package.ps1
        -BuildDir ${CMAKE_BINARY_DIR}
        -OutputDir ${CMAKE_SOURCE_DIR}/dist
        -PackageName BasicGameTemplate
        -ExecutableName basic_game_template.exe
        -GameConfig ${CMAKE_SOURCE_DIR}/templates/basic_game/basic_game.game.json
        -AssetsPath ${CMAKE_SOURCE_DIR}/templates/basic_game/assets
        -ReadmePath ${CMAKE_SOURCE_DIR}/templates/basic_game/README.md
        -AllowMissingAssets
        -SkipBuild
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS basic_game_template
    COMMENT "Packaging basic game template executable, config, runtime DLLs, and assets"
    VERBATIM
)

add_test(
    NAME package_basic_game_template_smoke
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
        -File ${CMAKE_SOURCE_DIR}/tests/PackageSmokeTests.ps1
        -RepoRoot ${CMAKE_SOURCE_DIR}
        -BuildDir ${CMAKE_BINARY_DIR}
        -OutputDir ${CMAKE_BINARY_DIR}/package_test_dist
        -Target basic_game_template
        -PackageName BasicGameTemplateSmoke
        -ExecutableName basic_game_template.exe
        -GameConfig ${CMAKE_SOURCE_DIR}/templates/basic_game/basic_game.game.json
        -AssetsPath ${CMAKE_SOURCE_DIR}/templates/basic_game/assets
        -ReadmePath ${CMAKE_SOURCE_DIR}/templates/basic_game/README.md
        -AllowMissingAssets
)
