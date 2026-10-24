# Runtime and editor compilation boundaries

Part of the [SimpleEngine architecture roadmap](ENGINE_ARCHITECTURE_ROADMAP.md).
The separation is **in progress**, not finished. This describes the new
compilation targets after the editor-panel extraction phase.

## Compile and link targets

| Target | Sources and responsibilities | Direct ImGui dependency |
| --- | --- | --- |
| `se_runtime` | ECS/scenes, core runtime systems, and generic `PluginManager` | No |
| `se_editor_ui` | Vendored Dear ImGui core and GLFW/OpenGL backends | Yes |
| `se_editor` | Scene Hierarchy, Object Inspector, Asset Browser, built-in prefab plugin UI | Yes |
| `engine_core` | Legacy `Engine.cpp` and `App.cpp` application shell | Yes (not runtime-only yet) |
| `runtime_boundary_tests` | Links **only** `se_runtime`; tests scene and plugin lifecycle | No |

Namespaced aliases: `SimpleEngine::Runtime`, `SimpleEngine::EditorUI`,
`SimpleEngine::Editor`.

Existing targets (`engine`, `snake_sample`, `flappy_bird_sample`,
`basic_game_template` and `engine_tests`) still link `engine_core`.
All their public APIs and output names remain unchanged.

## Where the editor code lives now

- `src/editor/SceneHierarchyPanel.cpp`: `Engine::renderSceneHierarchyPanel()`
- `src/editor/InspectorPanel.cpp`: `Engine::renderInspectorPanel()` and local `editStringField()`
- `src/editor/AssetBrowserPanel.cpp`: `Engine::renderAssetBrowserPanel()`
- `src/editor/PluginEditorUI.cpp`: built-in prefab editor plugin,
  `PluginManager::loadDefaultPlugins()` and `PluginManager::renderUi()`
- `src/PluginSystem.cpp`: generic, ImGui-independent plugin registration,
  `onUpdate`, teardown, count and names.

The panels remain `Engine` methods with access to its private data to preserve
existing behavior for this stage. They do **not** yet use a standalone editor
service/context API, and `Engine.cpp` still owns editor bootstrap, gizmos,
play/pause UI and several document-editing operations.

## Guardrails and tests

CMake scans the source files assigned to `se_runtime` for direct ImGui header
includes and rejects them at configure time. The runtime-only test now
exercises `PluginManager` registration, update and unload in addition to
scene/component operations. This only establishes the first boundary: it is
not a full transitive dependency verifier, and runtime is still Windows-focused.

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

## Follow-up milestones

1. Extract editor bootstrap and play/gizmo controls from `Engine.cpp`.
2. Replace direct `Engine` private-state access in editor panels with
   `EditorContext` / `EditorCommands` and explicit callbacks.
3. Move `onRenderUi` out of runtime-facing `IPlugin` to a separate editor
   extension interface; make default editor plugins registered by the editor.
4. Add separate `SimpleEngineEditor` and `SimpleEnginePlayer` entry points.
5. Implement independent projects, persistent entity identifiers and
   authored-vs-runtime scene isolation.
6. Replace Windows-only audio/link dependencies and add native platform CI.

This remains a **behavior-preserving refactor**, not yet a new project authoring
feature or a cross-platform engine release.
