cmake_minimum_required(VERSION 3.18)

project(SimpleEngine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

enable_testing()

find_package(Threads REQUIRED)
# Pin to the Lua 5.4 ABI, which MSYS2 distributes separately from Lua 5.5.
find_package(Lua 5.4 EXACT REQUIRED)

# First stage of the editor/runtime split. Runtime components compile without
# Dear ImGui, and can be linked directly by headless applications and tests.
set(SIMPLEENGINE_RUNTIME_SOURCES
    src/AssetManifest.cpp
    src/AudioSystem.cpp
    src/Renderer.cpp
    src/Scene.cpp
    src/TilemapTools.cpp
    src/AnimationTools.cpp
    src/SceneEditSession.cpp
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
    src/LuaScriptRuntime.cpp
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
target_include_directories(se_runtime PRIVATE ${LUA_INCLUDE_DIR})
target_link_libraries(se_runtime PUBLIC
    glfw3
    opengl32
    gdi32
    winmm
    Threads::Threads
    ${LUA_LIBRARIES}
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
    src/editor/TilemapEditorPanel.cpp
    src/editor/AnimationTimelinePanel.cpp
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

# The standalone player compiles Engine.cpp in an editor-free mode.
# Do not link engine_core or se_editor here: the player must not depend on
# Dear ImGui or the editor panels, even indirectly.
add_library(se_player_core STATIC src/Engine.cpp)
target_compile_definitions(se_player_core PRIVATE SIMPLEENGINE_PLAYER_ONLY=1)
target_include_directories(se_player_core PUBLIC
    include
    include/engine
)
target_link_libraries(se_player_core PUBLIC se_runtime)
add_library(SimpleEngine::PlayerCore ALIAS se_player_core)

add_executable(SimpleEnginePlayer src/player/PlayerMain.cpp)
target_link_libraries(SimpleEnginePlayer PRIVATE se_player_core)

# The editor keeps the original game/demo code and links the editor facade.
add_executable(SimpleEngineEditor
    src/editor/EditorMain.cpp
    src/editor/ProjectLauncher.cpp
    src/DemoScene.cpp
    src/DemoGame.cpp
)
target_link_libraries(SimpleEngineEditor PRIVATE engine_core)

add_test(NAME standalone_player_help COMMAND SimpleEnginePlayer --help)
add_test(NAME standalone_editor_help COMMAND SimpleEngineEditor --help)

# Test project/scene loading headlessly using the runtime library only.
add_executable(player_scene_tests tests/PlayerSceneTests.cpp)
target_link_libraries(player_scene_tests PRIVATE se_runtime)
add_test(NAME player_scene_data COMMAND player_scene_tests
    "${CMAKE_SOURCE_DIR}/examples/player-scene/game.config.json"
    "${CMAKE_SOURCE_DIR}/examples/player-scene/assets/scenes/welcome.scene.json"
)

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

# GUID, non-destructive Play Mode, scene history, legacy serializer tests.
add_executable(scene_authoring_tests tests/SceneAuthoringTests.cpp)
target_link_libraries(scene_authoring_tests PRIVATE se_runtime)
add_test(NAME scene_authoring_tests COMMAND scene_authoring_tests)

# Headless tests: Lua script lifecycle, entity isolation, input, errors and
# independent project-generated gameplay without Dear ImGui or a graphics window.
add_executable(lua_gameplay_tests tests/LuaGameplayTests.cpp)
target_link_libraries(lua_gameplay_tests PRIVATE se_runtime)
add_test(NAME lua_gameplay_tests COMMAND lua_gameplay_tests)

# Validate CLI create → Lua project validation → intentional syntax error.
add_test(NAME lua_project_cli_smoke
    COMMAND ${CMAKE_COMMAND}
        -DPROJECT_TOOL=$<TARGET_FILE:simpleengine_project>
        -DTEST_DIR=${CMAKE_BINARY_DIR}/lua_project_cli_smoke
        -P ${CMAKE_SOURCE_DIR}/tests/ProjectCliSmokeTests.cmake
)

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

# Verify a release-style editor package opens the Project Hub rather than
# the legacy demo. This smoke test does not launch a graphical application.
add_test(
    NAME package_editor_project_hub_smoke
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
        -File ${CMAKE_SOURCE_DIR}/tests/EditorPackageSmokeTests.ps1
        -RepoRoot ${CMAKE_SOURCE_DIR}
        -BuildDir ${CMAKE_BINARY_DIR}
        -OutputDir ${CMAKE_BINARY_DIR}/editor_package_test_dist
)
