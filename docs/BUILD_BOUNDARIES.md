# Runtime and editor compilation boundaries

Part of the [SimpleEngine architecture roadmap](ENGINE_ARCHITECTURE_ROADMAP.md).
The separation is **in progress**, not finished. This describes the new
compilation targets after the editor UI-frame and gizmo extraction phase.

## Compile and link targets

| Target | Sources and responsibilities | Direct ImGui dependency |
| --- | --- | --- |
| `se_runtime` | ECS/scenes, core runtime systems, and generic `PluginManager` | No |
| `se_editor_ui` | Vendored Dear ImGui core and GLFW/OpenGL backends | Yes |
| `se_editor` | Scene Hierarchy, Inspector, Asset Browser, prefab plugin UI, frame lifecycle, simulation panel, gizmo | Yes |
| `engine_core` | Legacy `Engine.cpp` and `App.cpp` application shell; delegates UI to `se_editor` | No direct ImGui references; still links the editor |
| `runtime_boundary_tests` | Links **only** `se_runtime`; tests scene and plugin lifecycle | No |

Namespaced aliases: `SimpleEngine::Runtime`, `SimpleEngine::EditorUI`,
`SimpleEngine::Editor`.

Existing targets (`engine`, `snake_sample`, `flappy_bird_sample`,
`basic_game_template` and `engine_tests`) still link `engine_core`.
All their public APIs and output names remain unchanged.

## Where the editor code lives now

- `src/editor/EditorFrame.cpp`: `Engine::initImGui()`, frame lifecycle,
  UI panel dispatch, draw submission and editor keyboard-capture query
- `src/editor/Gizmo.cpp`: transform gizmo drawing, hit-testing and world/screen conversions
- `src/editor/EnginePanel.cpp`: simulation control, statistics, rendering settings
- `src/editor/SceneHierarchyPanel.cpp`: `Engine::renderSceneHierarchyPanel()`
- `src/editor/InspectorPanel.cpp`: `Engine::renderInspectorPanel()` and local `editStringField()`
- `src/editor/AssetBrowserPanel.cpp`: `Engine::renderAssetBrowserPanel()`
- `src/editor/PluginEditorUI.cpp`: built-in prefab editor plugin,
  `PluginManager::loadDefaultPlugins()` and `PluginManager::renderUi()`
- `src/PluginSystem.cpp`: generic, ImGui-independent plugin registration,
  `onUpdate`, teardown, count and names.

The editor implementations remain `Engine` methods with access to its private
data to preserve behavior. `Engine.cpp` now has no direct ImGui includes or
`ImGui::` references; it calls methods implemented in `se_editor`.
The boundary is still **not** a runtime-only application: `engine_core` links
editor code, and `Engine` owns both editor and gameplay state. An explicit
`EditorContext` and independently linked player remain future work.

## Guardrails and tests

CMake scans all `se_runtime` source files for direct ImGui includes and now
also rejects direct ImGui includes/calls in `src/Engine.cpp`. This does not
fully validate transitive dependencies. The runtime-only CTest continues to
cover scene/component and generic plugin lifecycle. Windows-specific linking
and WinMM audio remain in place.

`.github/workflows/validate-pr.yml` now builds all Windows executables and
runs CTest for pull requests targeting `main`. It is separate from the
existing publish workflow, which still runs only on pushes to `main`.

Windows + MSYS2 UCRT64 prerequisites are in the repository release workflow.
To build and run the full regression suite:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

To verify runtime-only compilation specifically:

```sh
cmake --build build --target runtime_boundary_tests
ctest --test-dir build --output-on-failure -R runtime_boundary_tests
```

The extraction should be checked with a native Windows build before merging.
Moving methods between translation units alone does not prove final linker and
GUI functionality.

## Standalone editor/player application milestone

Two dedicated executable targets are now available: `SimpleEngineEditor`
links the editor facade, while `SimpleEnginePlayer` compiles the same
`Engine.cpp` with `SIMPLEENGINE_PLAYER_ONLY` and links only `se_runtime`.
The player uses a serialized startup scene, and PR CI checks that no Dear
ImGui symbols enter its executable. The original engine/game targets remain.

See [Standalone Editor and Player](EDITOR_PLAYER.md) for commands and
limitations. This is a transitional compile-time split; fully independent
editor context/commands and project-based gameplay are still future phases.

## Follow-up milestones

1. Replace direct `Engine` private-state access in editor components
   with an `EditorContext` and undoable editor command API.
2. Separate UI-facing plugin extension callbacks from runtime `IPlugin` and
   register built-in editor plugins only when the editor is active.
3. Add independent `SimpleEngineEditor` and `SimpleEnginePlayer` entry
   points so the player can link without editor symbols.
4. Implement independent projects, persistent entity identifiers and
   authored-vs-runtime scene isolation.
5. Replace Windows-only audio/link dependencies and add native platform CI.

This remains a **behavior-preserving refactor**, not yet a new project authoring
feature or a cross-platform engine release.
