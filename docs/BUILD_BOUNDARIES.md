# Runtime/editor compilation boundary

This is the **first incremental implementation step** of the broader
[Unity-style architecture roadmap](ENGINE_ARCHITECTURE_ROADMAP.md).

## New CMake targets

| Target | Content | Can link without ImGui? |
| --- | --- | --- |
| `se_runtime` | Scene/ECS, rendering, audio, physics, scripting, asset/scene systems, utilities | Yes |
| `se_editor_ui` | Dear ImGui and GLFW/OpenGL ImGui backends | No |
| `engine_core` | Existing Engine/App and C++ plugin implementation (compatibility facade) | No |
| `runtime_boundary_tests` | Regression test using runtime scene APIs | Yes |

Public namespaced targets: `SimpleEngine::Runtime`, `SimpleEngine::EditorUI`.

Existing executables and samples still link `engine_core`, preserving the
current API and packaging logic. The new runtime test links **only**
`se_runtime`, not the editor library.

## Guardrail

At configure time, CMake scans the `se_runtime` translation units and fails
if a source file directly includes `imgui.h` or `imgui_impl...` headers.
This prevents an accidental direct UI dependency while the architecture is
migrated. It does not yet enforce the whole transitive include graph or remove
the platform-specific Windows audio/window dependencies.

## Build and test

On a supported Windows/MSYS2 UCRT64 development environment:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Explicit new target check:

```sh
cmake --build build --target runtime_boundary_tests
ctest --test-dir build --output-on-failure -R runtime_boundary_tests
```

## Migration checklist

1. **This PR:** Split runtime sources, standalone ImGui, compatibility facade,
   source-level guard, and runtime-only lifecycle test.
2. Next: extract ImGui panel rendering from `src/Engine.cpp` and editor UI
   hooks from `src/PluginSystem.cpp`, leaving pure engine loop/services in
   runtime targets.
3. Next: introduce `SimpleEngineEditor` and `SimpleEnginePlayer` executables
   with shared runtime bootstrap services.
4. Then: project manifest/manager, GUID-based scene documents, separate
   editor/runtime worlds, and a minimal generic player for arbitrary projects.
5. Independently: replace WinMM and Windows-only link flags with portable
   backends and platform-specific link options.

No complete editor/runtime separation or cross-platform compatibility is
claimed at this stage. Those are follow-up milestones.
