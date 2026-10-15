# SimpleEngine: Target Architecture and Multi-Year Development Roadmap

Status: proposed architecture (not implemented). Baseline: main branch inspected 2026-10-15.
Maintainers should use this document for the long-term vision and ENGINE_DEVELOPMENT_SOURCE_OF_TRUTH.md for implementation status and historical delivered milestones. When executing a phase, turn each epic into small, reviewable issues and update both documents.

## A. Mission, users, and principles

Build a 2D-first game creation platform with a portable native C++ runtime and a visual, scriptable editor. A user must be able to create a project, assemble scenes from components, import assets, write gameplay code, press Play, debug, and export a redistributable game without modifying or recompiling the engine.

Personas:
- No-code/low-code beginner: prefabs, reusable templates, drag-and-drop, inspector, asset import and starter components.
- Independent game developer: script authoring, tilemap/animation editors, physics, input, UI, reusable scene composition, builds.
- Engine contributor: C++ extension SDK, tests, debugger, modular targets, predictable build graph and documented data formats.
- Package maintainer: extension versioning, permissions and lifecycle, reproducible builds, signed provenance.

Principles:
1. Project-first: editor and player load user projects independent of engine source.
2. Editor/runtime isolation: editor-only dependencies never enter exported player binaries.
3. Stable serialized identity: resource GUIDs and entity GUIDs, versioned schemas, migration tooling.
4. Cross-platform by design: platform-specific dependencies behind thin platform adapters.
5. Inspectable and recoverable content: human-readable project files, deterministic import, transactional saves, undo/redo and autosave.
6. Extension boundaries before extension marketplace: well-specified APIs and compatibility.
7. Incremental migration: existing engine, Snake, Flappy Bird, BasicGameTemplate and tests always remain working.
8. Deliver vertical slices and user-facing workflows, not disconnected feature checklists.
9. Profile before replacing core infrastructure; do not rewrite ECS or renderer without evidence.
10. Engine stays 2D-first. Networking multiplayer, consoles, 3D and full C# parity are deferred.

## B. Verified repository baseline

Existing:
- CMake project (C++17); targets engine_core, engine, engine_tests, snake_sample, flappy_bird_sample, basic_game_template.
- Sparse-set-like ComponentPool, Scene, ECS; JSON serializer, prefab registry, resource manager.
- OpenGL renderer, sprite and tilemap rendering, basic lighting, render packet/graph and job system.
- AABB-based physics, contact events and gravity; sprite animation; block text.
- ScriptSystem with C++ callback registration and lifecycle, IGame/GameContext; PluginManager with C++ IPlugin instances.
- ImGui editor panels, inspector, hierarchy, asset browser and basic gizmos in src/Engine.cpp.
- AssetManifest with IDs, scanners and metadata; keyboard/gamepad actions.
- Windows WinMM audio in src/AudioSystem.cpp; Windows-specific opengl32/gdi32/winmm CMake linkage.
- tools/package.ps1 and publish-game-downloads.yml for Windows build release automation.
- ENGINE_DEVELOPMENT_SOURCE_OF_TRUTH.md contains the earlier prototype roadmap and status.

Primary gaps:
- EditorApp and RuntimeApp wrappers exist, but editor implementation and ImGui are still inside engine_core.
- Separate game executables compile gameplay in C++; no arbitrary project player with scripted gameplay.
- No complete project asset database (stable persistent file GUID lifecycle, import cache, dependency graph, rename/update).
- No dynamically loadable, versioned extension ABI.
- No undoable document/scene command model with editor/runtime scene snapshots.
- Windows-specific sound and link dependencies block native Linux/macOS builds.

## C. Target system and compile-time dependency rules

Logical graph:

SimpleEngineEditor -> EditorApplication -> EditorModules -> RuntimeServices
SimpleEnginePlayer -------------------------------> RuntimeServices
SimpleEngineCLI -----> ProjectService/AssetCooker/BuildService -> RuntimeServices
RuntimeServices -> Core, ECS, Scene, Assets, Renderer, Physics, Audio, Script, Input, RuntimeUI
EditorModules -> Inspector, Viewports, Hierarchy, AssetBrowser, ImportUI, AnimationUI, ProfilerUI, Console, ExportUI
PluginHost -> versioned extension ABI -> selected public Runtime/Editor APIs

Invariants:
- Engine core must not include imgui headers or editor-only modules.
- Player must not require a compiler, Git, CMake, or editor.
- BuildService, CLI and editor must invoke the SAME underlying cook/package logic.
- No renderer-specific OpenGL types in Scene/ECS component schemas.
- Project paths must be relative and explicitly resolved through a ProjectContext/VFS.
- Editor owns authored scene data; RuntimeWorld owns simulation state.
- Every high-level editor mutation must participate in Undo/Redo unless explicitly marked non-undoable.
- Plugins and scripts access narrow service APIs, never raw long-lived internal pointers.

Proposed target tree (migrate gradually, not a one-time rewrite):

~~~
SimpleEngine/
  CMakeLists.txt
  CMakePresets.json
  cmake/
  engine/
    core/           # IDs, logging, memory, jobs, filesystem, time
    project/        # ProjectManager, ProjectContext, schema migration
    ecs/            # EntityHandle, ComponentRegistry, systems
    scene/          # SceneDocument, RuntimeWorld, prefabs, serialization
    assets/         # AssetDatabase, ImporterRegistry, dependency graph, VFS
    renderer/       # render device, OpenGL backend, render graph, 2D features
    physics/        # Physics2D interface, Box2D implementation
    audio/          # AudioEngine, miniaudio implementation
    scripting/      # LuaRuntime, bindings, lifecycle and reflection
    input/          # actions, mappings and device abstraction
    ui/             # runtime UI, independent of ImGui
    platform/       # windows, linux, macos adapters
  editor/
    app/
    project_browser/
    panels/         # Hierarchy, Inspector, Assets, Console, Profiler
    viewports/      # SceneView, GameView, gizmos, selections
    commands/       # ICommand, history, transactions, change events
    authoring/      # tilemap, animator, prefab, UI, particle editors
  player/           # native entrypoint and exported runtime template
  cli/              # noninteractive validate/import/test/export
  sdk/
    include/simpleengine/       # stable public APIs, not private engine headers
    plugin_c_api/               # platform-neutral ABI tables and opaque handles
    samples/
  tools/
    asset_cooker/
    export_pipeline/
    migration/
  templates/
  examples/
  tests/
    unit/
    integration/
    scene_fixtures/
    export_smoke/
    benchmarks/
  docs/
    architecture/
    user_manual/
    sdk/
    tutorials/
~~~

First practical CMake targets:
- se_core, se_scene, se_assets, se_render, se_physics, se_audio, se_script, se_runtime, se_editor
- SimpleEngineEditor, SimpleEnginePlayer, SimpleEngineCLI
- Keep the old target names as compatibility wrappers until sample migrations complete.

## D. Runtime, scene and entity contracts

Current Entity is an incrementing integer index; retain a compact runtime handle but add a stable serialized identity.

- EntityGUID: 128-bit identity in authored scene files.
- EntityHandle: index+generation within a specific runtime world; invalidate stale handles on deletion.
- SceneGUID / AssetGUID: persistent IDs allocated once, tracked in project asset database.
- ComponentTypeID / ComponentSchemaVersion: registered component metadata.
- RuntimeWorld: simulation-specific entity map, rigidbody instances, script state and transient caches.
- SceneDocument: editor-authoritative scene tree; can contain unloaded components and unresolved references safely.

Parent-child hierarchy:
- ParentGUID and ordered child GUIDs (one authoritative ownership model).
- Transform local vs world semantics, cycle prevention, reparent preserving world transforms, deterministic traversal.
- Separate hierarchy order from render sorting order and UI z-order.

Reflection and component registry:
- Name, stable type ID, schema version, registered properties, types, ranges, asset references, enum options and editor hints.
- Inspector, serialization, animation, Lua field exposure, prefab override diffs and plugin components use the SAME descriptors.
- Unknown components must round-trip without silent data loss; provide missing-plugin warnings.

Scene files:
- Versioned header, scene GUID, entities and per-component typed records.
- No absolute file paths or internal GPU handles.
- Referential integrity validation (missing assets, missing entities, cyclic parenting).
- Atomic saves (write temporary file, fsync where supported, rename), autosave/recovery, backups, change-detection and explicit migration.

Edit vs Play:
- On Play: make a runtime world from the document snapshot, initialize scripts/physics/assets and start simulation.
- On Pause: suspend simulation time but allow inspection.
- On Step: advance one fixed tick.
- On Stop: shut down scripts/physics/runtime world; retain unchanged authored document.
- Optional 'Apply Changes' deliberately copies selected differences as one undoable document transaction.
- User-facing behavior must be covered by regression tests.

World update lifecycle:
~~~
Poll input -> dispatch input actions -> fixed-step accumulator
  -> script fixed callbacks -> physics step -> collision events
  -> script event callbacks -> animation/timers -> world transform update
  -> render extraction -> render thread/GPU -> audio/service maintenance
~~~

Use explicit ordering contracts and main-thread-only world mutation initially. Thread workers can perform import, decoding, background asset cooking, immutable render prep; command queues apply updates on the world thread. Avoid premature arbitrary concurrent writes to ECS.

## E. Project specification

A project is portable and self-contained:

~~~
MyGame/
  project.simple.json
  Assets/
    Scenes/
    Scripts/
    Sprites/
    Audio/
    Animations/
    Prefabs/
    UI/
  ProjectSettings/
    Input.json
    Physics.json
    Build.json
  Packages/
    packages.lock.json
  .simpleengine/          # generated, gitignored
    cache/
    logs/
    session/
~~~

Project schema fields: projectId, name, formatVersion, engineCompatibility, mainSceneAssetId, display/window options, physics time step and coordinate unit, enabled plugins, package lock, build profiles, settings. Avoid user-machine-specific absolute paths.

Project opening stages: normalize/sanitize root; validate manifest; open project lock; compatibility check; schema migration preview; scan asset database; schedule changed imports; resolve startup scene; load editor document. File modifications should be watched and debounced.

Sample games currently have independent C++ sources and game JSON files. Convert them into openable projects in phases, preserving their existing binaries until parity is proven.

## F. Asset database and content pipeline

Authoritative project metadata: GUID, normalized path, file hash, importer ID/version, import settings hash, platform variant, dependent GUIDs, imported product identifiers, labels/type. Use SQLite (or a similarly transaction-safe indexed store) for derived metadata and sidecar GUID files or manifest records that are safe to commit to Git.

Source files are read-only inputs; derived outputs live in .simpleengine/cache. The cooker cache key includes content hash, importer version, import settings, target platform and relevant build tool version. Produce deterministic results when inputs are the same.

Pipeline:
1. Discover filesystem changes (watch and periodic reconciliation).
2. Update/mint GUID and detect rename/move.
3. Validate file type, limits and importer eligibility.
4. Inspect dependencies and detect cycles.
5. Import/consolidate textures, audio, fonts, spritesheets, scenes, shaders.
6. Generate thumbnails and platform-specific cooked products.
7. Update index transactionally and invalidate downstream dependents.
8. Expose safe AssetHandle lookups in editor/runtime.

Data reference rules:
- Serialized scenes refer to GUIDs, not absolute paths.
- Missing resources produce editable placeholders and actionable diagnostics.
- Live reimport updates resources without invalidating scene references.
- Asset browser actions preserve IDs during rename/move.
- Hash and signing of user-visible downloadable packages is distinct from internal cache hashes.
- The importer runs untrusted parsers on user files: bound memory/size/time, validate decompressed dimensions, do not execute imported scripts automatically.

## G. Rendering architecture

Preserve the current OpenGL implementation behind IRenderDevice; don't rewrite merely to add a second API. Split RenderWorld extraction from graphics backend calls. Use a dedicated Scene framebuffer (selection overlay, picking ID pass, grid/gizmos) and independent Game framebuffer (real camera output, no gizmos).

Milestones:
- First: framebuffer offscreen rendering, scalable resolution, camera pan/zoom, sprite draw order, clipping, texture batching, grid, picking.
- Next: multiple cameras, orthographic projection, render layers, parallax, tilemap chunk batches, viewport scaling, 2D skeletal/sprite animation.
- Later: material/shader graph API, render targets, additive/multiply blends, particles, 2D lights and shadows, normal maps, postprocessing.
- Profile first: frame CPU/GPU timings, draw calls, texture uploads, allocated bytes, tilemap culling, memory budgets.

Do not promise Vulkan/Metal/Direct3D until OpenGL is proven portable and abstracted. On macOS, plan to support an OpenGL compatibility path first if technically viable, while treating a Metal-native backend as a later dedicated effort.

## H. Physics, audio, UI, input and animation

Physics: adapter to Box2D 3 (C17 API), with RuntimeWorld/body mapping and synchronizing transforms after step. Static/dynamic/kinematic bodies, box/circle/capsule/polygon/collider fixtures, sensors, layers/masks, materials, ray and shape queries, joints, fixed timestep, scaled units, contact events. Keep old AABB system for migration until parity tests pass.

Audio: replace WinMM-specific implementation with platform-independent backend (candidate: miniaudio). Support one-shot and streaming clips, sound buses and mixer groups, master/music/SFX volumes, spatial panning, voice budget, output-device changes. Never block the audio thread on ECS or filesystem locks.

Runtime UI (not ImGui): retained-mode widgets, anchors, flex/grid layouts, text, images, buttons, scroll, focus/navigation, accessibility hooks, localization and resolution scaling. Editor UI remains ImGui. Runtime UI emits ordinary input/gameplay events.

Input: device-neutral ActionMap; keyboard/gamepad/pointer/touch adapters, dead zones, chords, remapping and gamepad UI navigation; import/export mappings through project settings. Preserve current keyboard/gamepad actions via compatibility adapter.

Animation: separate AnimationClip (time -> property paths), AnimatorController (states, transitions, conditions, blend), animated sprite/cutout, events, editor timeline. Animation must modify the component reflection API rather than hardcode component types.

## I. Scripting platform

Phase 1 language: embedded Lua with a restricted SimpleEngine API, using sol2 or deliberate Lua C API wrappers. Keep native C++ components as an advanced option. Do not promise C# first: .NET hosting, ABI and cross-platform export substantially increase scope.

Script component requirements:
- Per-entity instance (multiple components permitted), stable type/asset ID.
- Explicit lifecycle: Awake/Create, Start, Update, FixedUpdate, event, Destroy/Dispose, scene unload.
- Inspector-visible properties with types, default values, asset/entity references and migration.
- Script compile/load validation before play; line-numbered error reporting.
- Reload strategy: pause world, serialize allowed script state, validate replacement, swap safely or revert without corrupting scene.
- Main-thread world access by default; async jobs marshal results to command queues.
- Controlled APIs for time, physics, assets, input, scene loading, logging, spawning, saving.
- No unsafe raw C++ pointers exposed to Lua, no arbitrary file/process/network permissions by default.
- Lua sandboxing reduces attack surface but is NOT a strong security boundary for untrusted code. Treat unknown projects/plugins as untrusted; ask consent, isolate highly privileged actions and use OS process isolation for high-risk tooling.

Native C++ scripting/plugin development becomes an optional SDK workflow, not a prerequisite for making games.

## J. Extension SDK and packages

Current IPlugin ownership inside C++ works in-tree but is not an inter-compiler ABI. Introduce extension types:

1. Lua editor extensions: portable panels and commands with constrained capabilities.
2. Native engine extensions: optional shared libraries with explicit versioned C ABI, opaque handles, function tables, lifecycle callbacks.
3. Content packages: templates, scripts, shaders, assets and examples with no native execution.

Plugin host API areas: component registration, Inspector property drawers, panels, menu commands, importers, project templates, build pipeline providers, asset creation, events and logging. Give plugins explicit capability manifests; separate editor-only and runtime capabilities.

Native ABI:
- extern "C" entry point returning versioned function tables.
- Explicit ABI major/minor, size fields for extensibility; no STL types, ownership of memory specified; host allocator interfaces.
- Handle lifetimes and thread affinity declared; block load on incompatible ABI, missing dependency, wrong platform/architecture.
- Avoid automatic native binary loading when opening untrusted projects.
- Consider loading untrusted importers in a worker process, not inside the editor.
- Initial native plugin reload can require editor restart. Hot unload is risky and deferred until reference/lifetime guarantees exist.

Package format:
~~~
some.plugin/
  package.json       # ID, version, compatible engine/API range, OS/architecture, deps, permissions
  README.md
  Editor/
  Runtime/
  Resources/
  bin/<platform-arch>/
~~~

Versioned packages.lock.json, local/URL/Git registry providers (initially Git/zip), dependency conflict diagnostics, reproducible resolution, offline cache, checksum validation and license/provenance review. Do not build a hosted commercial marketplace in early phases.

## K. One-click exports and separate deliverables

Build is a project data cooking and packaging operation, not a full engine rebuild for script-only games:

Source project -> validation -> asset dependency closure -> platform-specific cook -> data pack -> player template -> portable folder/archive -> smoke test/checksum.

Product binaries:
- SimpleEngineEditor-Windows-x64, Linux-x64, macOS-arm64 (eventual editor builds).
- SimpleEnginePlayer / runtime templates, per OS and architecture.
- SimpleEngineCLI for headless import, validation, build/export and tests.
- SimpleEngineSDK for extensions and embedding.
- Starter projects and fully exported demonstration games.

Player package example:
~~~
MyGame-Windows-x64/
  MyGame.exe
  game.pak
  runtime-libraries/
  licenses/
  manifest.json
~~~

Game pack design: versioned manifest, asset ID index, scene files, compressed processed textures/audio/shaders, project settings, scripts and checksums. Optional encryption is not DRM. Do not bundle editor UI or source C++ by default. Native extensions need correctly matched binaries and ABI check.

Export milestones:
1. Windows x64 locally and headless (existing ZIP approach upgraded to generic player).
2. Linux x64 editor/player/export from appropriate build hosts.
3. macOS arm64/x64 with signing/notarization guidance where applicable.
4. WebAssembly/browser after renderer, filesystem, audio, threading and asset-fetch integration.
5. Android/iOS after touch/UI, permissions, package/signing and platform toolchains.
6. Console support only if access, SDK licensing and partner requirements are available.

Use native CI runners when appropriate; compiling on one OS does not establish runtime compatibility on another. Run exported game smoke tests on fresh VMs/containers/GUI-capable runners, not only source-level unit tests.

## L. Build, CI, release and developer experience

Split release workflows from PR validation:
- Pull requests: Windows builds + unit/integration/serialization tests + clang-format/clang-tidy or equivalent + dependency check; no GitHub Release.
- Pushes to main/tags: release candidate artifact pipeline; avoid publishing untested builds; tags or designated channels for stable releases.
- Nightly: cross-platform matrix, sanitizers, fuzz importers/serializer/asset pack reader, performance regression, asset migration fixtures.
- Separate editor, player, CLI, SDK and examples as distinct artifacts. Existing publish-game-downloads.yml can coexist while migration occurs; eventually supersede it.
- CMakePresets.json for development, release and platform/CI presets; pin external dependencies with manifests/lockfiles.
- Build reproducibility metadata: commit, toolchain, dependency revisions, engine API version; SHA256 plus optional signing/attestation.
- Add generated API docs, user manual, sample downloads and changelog.
- Define branch protection with required checks once stable; no need for write permissions in untrusted PR job contexts.

Security and licensing:
- Inventory all vendored libraries/files, source licenses and asset provenance; add root LICENSE if distribution terms are intended.
- Ship license notices for all dependencies in exported editor/player packages.
- Threat model plugins, script permissions, archive extraction, zip slip, path traversal, decompression bombs, project-file parsing and symlinks.
- Crash handling with opt-in data collection and redaction; avoid mandatory telemetry.
- Maintain compatibility/support policy, published release channels and project migration procedure.

## M. Work breakdown, dependencies, and gates

Indicative effort bands are planning placeholders, not deadlines. Assumes a motivated solo developer familiar with C++, assisted by CI and tests. A small experienced team can parallelize distinct modules, but architectural prerequisites remain.

| Phase | Indicative solo effort | Main epics | Required measurable exit gate |
|---|---|---|---|
| 0. Baseline and design | 2-4 weeks | ADRs, dependency licenses, CI smoke, golden projects, target APIs | All existing demos/tests green, baseline benchmarks captured |
| 1. Engine/editor/runtime split | 6-10 weeks | CMake targets, runtime services, platform adapters, headless bootstrap | Player runs without ImGui; editor opens demo; compatibility tests green |
| 2. Project and scene foundation | 6-10 weeks | project format, stable GUIDs, scene documents, hierarchy, schema migrations | Two unrelated projects open without recompiling; safe Play/Stop |
| 3. Professional editor MVP | 8-14 weeks | docking, scene/game FBOs, gizmos, picking, inspector reflection, undo/redo, console | 2D level can be built and persisted through GUI, undo/redo survives representative edits |
| 4. Script and asset pipeline | 8-14 weeks | Lua instances/bindings, imported GUID assets, thumbnails, dependency invalidation | Import+rename sprite preserves references; attach/edit Lua without recompiling |
| 5. Playable 2D toolkit | 10-18 weeks | Box2D, tilemap painting, animation timeline, input project settings, audio | Full sample game made using editor+scripts and packaged for Windows |
| 6. Export and stable beta | 8-14 weeks | generic player, cooked asset pack, Windows exporter, SDK docs, profiler/debugger | Clean-machine standalone export; migration tests; first public beta |
| 7. Multi-platform release | 12-24 weeks | Linux/macOS portability, audio/backend, CI export tests, signing guidance | Editor/player verified on supported OS targets, platform export smoke green |
| 8. Extension ecosystem | 12-24 weeks | C ABI, Lua editor extensions, packages.lock, dependency solver, plugin templates | Independently built plugin works with declared compatible engine versions |
| 9. Advanced tools and polish | ongoing | runtime UI authoring, lighting, particles, localization, visual scripting, package browser | Real third-party games shipped; documentation/support and performance SLOs met |

These phases partially overlap. A broad Unity-like engine is a multi-year project for an individual; do not assume these additive estimates guarantee a release date.

Dependency sequence:
0 -> 1 -> 2 -> 3.
2 -> 4 -> 5 -> 6.
1 -> platform abstraction -> 7.
2 + reflection registry + scripting -> 8.
3 + 4 + 5 -> 9.

Priority order:
P0: prevent data loss, runtime/editor decoupling, project identity and scene isolation.
P1: editor usability, scripting, imported asset references, Windows generic export.
P2: Box2D integration, gameplay UI, animator, authoring workflow.
P3: portability, native plugins, package distribution, visual scripting, advanced rendering.

## N. Suggested first 12 PRs (small reviewable vertical slices)

1. Document ADR-001 architecture and compatibility baseline; add architecture CI checks banning ImGui imports in runtime-only targets.
2. Extract editor rendering panels out of Engine.cpp into dedicated panel classes without behavior changes.
3. Add se_core, se_runtime and se_editor CMake targets with compatibility aliases.
4. Add ProjectContext/ProjectManifest reader/writer and a CLI validation command, with schema tests.
5. Implement Project Manager UI: create/open project and recent projects.
6. Add EntityGUID and EntityHandle generation; update serializer with backward-compatible load/migration fixtures.
7. Add SceneDocument and RuntimeWorld, ensure Play/Stop doesn't mutate scene content.
8. Render scene to framebuffer; independent Scene View and Game View panels with selection.
9. Introduce ComponentRegistry metadata and generic Inspector property panel, beginning with Transform and Sprite.
10. Add transactional ICommand undo/redo for create, rename, transform, reparent, duplicate, delete.
11. Implement AssetDatabase GUID imports, move/rename preservation and incremental scan tests.
12. Embed minimal LuaRuntime: one script asset, per-entity instance, property exposure and error console; migrate one sample behavior.

After each PR: run existing CTest, launch editor and runtime, exercise serialization fixtures, check exported asset packaging, and update the roadmap status. Avoid landing broad refactors without user-visible vertical-slice tests.

## O. Engineering quality targets (initial proposed SLOs; refine by measurement)

- Reliability: no scene/project data loss on crashes; atomic saves, recovery fixture tests, migration rollback.
- Runtime: steady 60 FPS for a defined reference 2D scene on documented baseline hardware; target 16.67 ms/frame, not a universal promise.
- Editor responsiveness: typical inspector interaction < 100 ms; keep asset import asynchronous; collect p50/p95 project load times.
- Scale: define and benchmark a reference project with 10,000 entities and 1,000 assets; optimize measured bottlenecks before higher promises.
- Export: Windows sample boot and display a frame on clean target machine with bundled libraries and no development toolchain.
- Formats: 100% project/scene/prefab fixtures round-trip through supported schema versions.
- Extensibility: ABI compatibility tests for last supported minor version range; native plugins fail safely on mismatch.
- Performance telemetry: local profile traces by default; sharing is opt-in only.
- Supply chain: locked/pinned dependencies, reproducible metadata, license/notice report.

## P. Risk register and decisions

1. Scope explosion (critical): do not attempt rendering, plugin market, C#, mobile and visual scripting simultaneously; milestone exit gates.
2. Editor/runtime coupling (critical): extract ImGui and editor panels before new runtime features.
3. Broken project format or lost assets (critical): stable GUIDs, atomic saves, migration fixtures, backups.
4. Unsafe plugins/scripts (critical): permission review, sandbox limits, process isolation for importers, no blind auto-exec.
5. Platform portability (high): WinMM and CMake Windows libraries first; staged native CI.
6. ABI breakage (high): C boundary, versioned tables and SDK compatibility suite.
7. Rendering rewrite risk (high): keep current OpenGL initially and add render abstraction only.
8. Physics behavioral differences (medium): migration adapters and golden physics tests.
9. Package/build complexity (medium): one shared headless cooking pipeline, portable player template.
10. Ecosystem fragmentation (high): semver, project format migration, published roadmap and documented deprecations.

Decision backlog / ADRs:
- ADR-001 module dependency direction and naming.
- ADR-002 serialized GUID, runtime handle generation and undo/redo.
- ADR-003 project/scene/prefab schema and migration policy.
- ADR-004 renderer API, threads and OpenGL portability.
- ADR-005 Box2D integration and physics units.
- ADR-006 Lua scripting and trust model.
- ADR-007 C ABI/native plugin support policy.
- ADR-008 asset database and imported product caching.
- ADR-009 generic export template/game.pak format.
- ADR-010 dependency/license policy and release channels.

## Q. Definition of product milestones

Alpha: user creates an independent project, builds a complete small static level visually, saves/reopens and uses Play/Stop without source edits.
Playable Alpha: a user builds Snake or Flappy Bird in the editor with Lua behavior and exported assets, and exports it on Windows without a compiler.
Public Beta: project migrations, undo/recovery, stable asset imports, animation/physics/tools, simple debugger, headless export and documentation withstand external testers.
Stable 1.0: multiple real independent projects released; supported OS matrix verified; stable core serialization and Lua APIs; explicit security/update/support policy; reproducible releases.
Extended Platform: extension SDK/package ecosystem, additional exports, advanced authoring/profiling, optional visual scripting and accessibility/localization polish.

## R. References (architecture patterns, not code to copy)

- Dear ImGui docking: https://github.com/ocornut/imgui/wiki/Docking
- Box2D 3: https://box2d.org/documentation/
- miniaudio: https://miniaud.io/docs/manual/index.html
- Lua bindings/sol2: https://github.com/ThePhD/sol2
- Godot resource UIDs: https://docs.godotengine.org/en/stable/classes/class_resourceuid.html
- Godot native GDExtension model: https://docs.godotengine.org/en/4.5/tutorials/scripting/gdextension/what_is_gdextension.html
- Godot game export: https://docs.godotengine.org/en/stable/tutorials/export/exporting_for_windows.html
- Unity editor/runtime assembly split: https://docs.unity.com/en-us/engine/6000.3/manual/packages-list/cus-pkg-lp/cus-pkg-development/cus-asmdef
- CMake presets: https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html

This document proposes interfaces and future deliverables; names, timings and targets are not claims about implemented repository functionality.
