cmake_minimum_required(VERSION 3.16)

project(SimpleEngine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

enable_testing()

find_package(Threads REQUIRED)

# First stage of the editor/runtime split. Runtime components compile without
# Dear ImGui, and can be linked directly by headless applications and tests.
set(SIMPLEENGINE_RUNTIME_SOURCES
    src/AssetManifest.cpp
    src/AudioSystem.cpp
    src/Renderer.cpp
    src/Scene.cpp
    src/PhysicsSystem.cpp
    src/Memory.cpp
    src/Game.cpp
    src/GameConfig.cpp
    src/ProjectManager.cpp
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
)

# Prevent accidentally introducing editor-only headers into the runtime sources.
# This is an initial source-level guard, not a replacement for full dependency tests.
foreach(runtime_source IN LISTS SIMPLEENGINE_RUNTIME_SOURCES)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${runtime_source}" runtime_contents)
    if(runtime_contents MATCHES "#[ \t]*include[ \t]*[<\"](imgui|imgui_impl)")
        message(FATAL_ERROR
            "Runtime file ${runtime_source} includes Dear ImGui. "
            "Move editor functionality to the editor integration target.")
    endif()
endforeach()

add_library(se_runtime STATIC ${SIMPLEENGINE_RUNTIME_SOURCES})
target_include_directories(se_runtime PUBLIC
    include
    include/engine
)
target_link_libraries(se_runtime PUBLIC
    glfw3
    opengl32
    gdi32
    winmm
    Threads::Threads
)
add_library(SimpleEngine::Runtime ALIAS se_runtime)

# Dear ImGui is now compiled into a dedicated editor UI library.
# The editor application still uses Engine.cpp and PluginSystem.cpp directly;
# extracting their editor paths is a separate follow-up migration.
add_library(se_editor_ui STATIC
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(se_editor_ui PUBLIC
    imgui
    imgui/backends
)
target_link_libraries(se_editor_ui PUBLIC glfw3)
add_library(SimpleEngine::EditorUI ALIAS se_editor_ui)

# Editor-only panels and native editor plugin callbacks live outside the runtime.
# Plugins, scenes, and game logic can link se_runtime without Dear ImGui.
add_library(se_editor STATIC
    src/editor/SceneHierarchyPanel.cpp
    src/editor/InspectorPanel.cpp
    src/editor/AssetBrowserPanel.cpp
    src/editor/PluginEditorUI.cpp
    src/editor/EditorFrame.cpp
    src/editor/Gizmo.cpp
    src/editor/EnginePanel.cpp
)
target_include_directories(se_editor PUBLIC
    include
    include/engine
)
target_link_libraries(se_editor PUBLIC
    se_runtime
    se_editor_ui
)
add_library(SimpleEngine::Editor ALIAS se_editor)

# Compatibility application shell: Engine.cpp no longer directly includes or
# uses Dear ImGui, but it still calls editor-facing functions implemented in
# se_editor. Next stage introduces separate application/player entry points.
# Core application code must not regain direct Dear ImGui references.
# UI lifecycle, gizmos, panels and keyboard capture live in se_editor.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/Engine.cpp" engine_application_source)
if(engine_application_source MATCHES "#[ \\t]*include[ \\t]*[<\\\"](imgui|imgui_impl)"
   OR engine_application_source MATCHES "ImGui::")
    message(FATAL_ERROR
        "src/Engine.cpp must remain independent of Dear ImGui. "
        "Move UI integration to src/editor.")
endif()

add_library(engine_core STATIC
    src/App.cpp
    src/Engine.cpp
)
target_include_directories(engine_core PUBLIC
    include
    include/engine
)
target_link_libraries(engine_core PUBLIC
    se_editor
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

# Runtime-only regression target: does not link engine_core or Dear ImGui.
add_executable(runtime_boundary_tests tests/RuntimeBoundaryTests.cpp)
target_link_libraries(runtime_boundary_tests PRIVATE se_runtime)
add_test(NAME runtime_boundary_tests COMMAND runtime_boundary_tests)

# Independent project creation/inspection CLI: only depends on the runtime
# library and does not link the editor, Engine.cpp or Dear ImGui.
add_executable(simpleengine_project src/ProjectTool.cpp)
target_link_libraries(simpleengine_project PRIVATE se_runtime)

add_executable(project_manager_tests tests/ProjectManagerTests.cpp)
target_link_libraries(project_manager_tests PRIVATE se_runtime)
add_test(NAME project_manager_tests COMMAND project_manager_tests)

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
