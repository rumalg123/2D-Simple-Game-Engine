# 2D Engine Development Source Of Truth

This file is the working source of truth for turning SimpleEngine from a demo-oriented C++ 2D engine prototype into something a game developer can use to build and package small 2D games such as Snake, Flappy Bird, and arcade prototypes.

Status values:

- Done: implemented and verified.
- In Progress: active implementation has started.
- Planned: accepted requirement, not started yet.

## Current Focus

1. UI/editor authoring improvements.
2. Tilemap collision and richer physics scene settings.
3. Asset metadata consumers for animation, font, and audio workflows.

## Implemented Surface

- Reusable `engine_core` static library with `engine`, `engine_tests`, `snake_sample`, `flappy_bird_sample`, and `basic_game_template` executables.
- `IGame` hook interface through `configureInput`, `registerScripts`, `loadInitialScene`, `registerPlugins`, `onFixedUpdate`, `onEvent`, and `debugStats`.
- ECS scene model with OpenGL sprite rendering, prefabs, JSON scene/prefab save and load, and an ImGui editor.
- Named input actions with keyboard keys, gamepad buttons, gamepad axes, `isDown()`, and `getAxis()`.
- Action changed events for named input actions.
- Game-facing scene reload, clear, and load-from-file requests through `GameContext`.
- Game-facing helpers for entity lookup, sprite/text/collider creation, prefab instantiation, and grid-to-world conversion.
- Dedicated `RuntimeApp` and `EditorApp` wrappers with `runGameApp()` selection from config.
- Basic event dispatch for key changes, collisions, and triggers through `IGame::onEvent`.
- Script lifecycle callbacks for create, start, variable update, fixed update, event, and destroy.
- Collision layers/masks, enter/stay/exit contact phases, and filtered AABB overlap queries.
- Sprite animation components using grid-based sprite sheets and UV frame selection.
- Tilemap components rendered through the sprite path with scene/prefab JSON persistence.
- Text components for simple block-font HUD and world text.
- Audio system for PCM `.wav` clips, procedural tones, and one-shot playback.
- Asset manifest JSON catalog with stable IDs, type inference, scan/import workflow, and metadata blocks for sprite sheets, animation clips, fonts, and audio.
- Windows package script for selected game executables, configs, assets, runtime DLLs, README, launcher, zip, and manifest.
- Reusable `templates/basic_game` starter with config, README, input setup, scene helpers, prefab usage, HUD text, and package target.
- CTest package smoke coverage for selected-game package output.

## Roadmap

| Status | Area | Requirement | Notes |
| --- | --- | --- | --- |
| Done | Source of truth | Keep this file as the canonical roadmap | Update status here as features are implemented. |
| Done | Core architecture | Split reusable engine code into `engine_core` | Demo, tests, samples, and template all link against the static library. |
| Done | Game hook API | Add an `IGame` and `GameContext` entry point for game code | Hooks cover input setup, script registration, scene loading, plugins, fixed update, events, and debug stats. |
| Done | Game project workflow | Add a project template and sample game project layout | `samples/snake`, `samples/flappy_bird`, and `templates/basic_game` build as separate executables linked against `engine_core`. |
| Done | Runtime configuration | Add per-game config for title, resolution, editor/runtime mode, asset paths, hot reload, and vsync | Implemented with `GameConfig`, flat JSON loading, and `demo.game.json`. |
| Done | Runtime/editor split | Allow packaged games to run without editor panels | `RuntimeApp` forces runtime mode, `EditorApp` forces editor mode, and `runGameApp()` selects between them from config. |
| Done | Packaging | Add baseline package output for the demo executable | `tools/package.ps1` and `package_game` produce a folder, zip, launcher, config, copied assets/runtime DLLs, README, and manifest. |
| Done | Packaging | Package a selected game project, not only the demo executable | `package_snake_sample`, `package_flappy_bird_sample`, and `package_basic_game_template` package game executables with config, README, assets directory, runtime DLLs, launcher, zip, and manifest. |
| Done | Rendering | Render sprites through OpenGL with texture resource reuse | Sprite components are the base render path for normal sprites, text, and tilemaps. |
| Done | Prefabs and serialization | Save and load scenes and prefabs through JSON | Current JSON covers core transforms, sprites, animation, text, tilemaps, tags, prefabs, and resources. |
| Done | Gameplay API | Add high-level helpers for sprites, text, colliders, prefabs, tags, and entity lookup | `Scene` now has creation and lookup helpers; `GameContext` can instantiate prefabs. |
| Done | Script lifecycle | Add script callbacks for create/start/update/fixed update/destroy/collision/trigger/input | `ScriptLifecycle` covers create, start, variable update, fixed update, destroy, and event dispatch for key, action, collision, and trigger events while playing. Legacy script registration maps to fixed update. Multiple scripts per entity and script-owned state remain future work. |
| Done | Input mapping | Bind named actions to keyboard keys, gamepad buttons, and gamepad axes | Game code can query actions with `isDown()` and axis pairs with `getAxis()`. |
| Done | Input events | Add action pressed/released events | `EventType::ActionChanged` reports named action transitions from keyboard and gamepad state. |
| Done | Scene management | Add game-facing scene load/reload/change APIs | `GameContext` exposes `reloadScene`, `clearScene`, and `loadScene`. |
| Done | Asset pipeline | Add asset manifest, stable asset IDs, sprite sheet metadata, animation clips, font assets, audio metadata, and import workflow | `AssetManifest` saves/loads `asset_manifest.json`, scans/imports supported files with stable IDs, exposes manifest access through game/plugin contexts, and stores metadata blocks for sprite sheets, animation clips, fonts, and audio. Runtime consumers for those richer metadata blocks and drag/drop editing remain future refinements. |
| Done | Sprite animation | Add grid-based sprite sheet animation components | Supports atlas rows/columns, frame ranges, playback speed, looping, editor editing, and JSON persistence. |
| Done | Tilemaps | Add renderable tilemap components | Tilemaps use atlas grids, row-major tile data, layers, tint/alpha, editor inspection, and JSON persistence. |
| Done | Audio | Add basic game audio playback | Supports Windows WinMM output, 8-bit/16-bit PCM WAV, mono/stereo clips, procedural tones, and one-shot playback. |
| Done | 2D physics/collision | Add collision layers/masks, enter/stay/exit events, and queries | `ColliderComponent` has layer/mask filters, contact events report `CollisionPhase::Enter/Stay/Exit`, `PhysicsSystem::queryAabb()` supports filtered overlap queries, and collider layer/mask fields persist through editor and JSON. |
| Planned | 2D physics/collision | Add tilemap collision and richer physics scene settings | AABB entity collision is stronger now, but tilemap collision, query shapes beyond AABB, and broader scene-level physics settings remain future work. |
| Done | Grid gameplay | Add grid/tile helpers for games like Snake | `Grid.h` provides grid containment, indexing, and cell-to-world conversion; Snake uses it. |
| Done | Text and HUD | Add simple block-font text components | Supports screen-space and world-space text through the sprite batcher, including tint, alpha, layers, and JSON persistence. |
| Planned | UI system | Add game UI primitives: font rendering, buttons, anchors/layout, menus, overlays | Current block text is useful only for simple HUDs. |
| Done | Editor tools | Provide an ImGui editor shell and inspector for current components | Inspector can view/tune core components including sprite animation and tilemaps. |
| Planned | Editor tools | Improve inspector component editing, drag/drop assets, prefab editing, tile painting, collider editing, scene picker, undo/redo, and play/edit separation | Current ImGui editor is useful but still prototype-level. |
| Done | Samples | Add complete Snake and Flappy Bird samples | `snake_sample` and `flappy_bird_sample` are separate runtime game executables linked against `engine_core`. |
| Done | Tests | Add core regression executable and CTest entry | Existing tests cover input bindings, resource reuse, config loading/defaults, and scene JSON for text/tilemaps. |
| Done | Tests | Expand regression coverage around config, runtime mode, scene management, input events, helpers, and packaging | Tests cover app mode normalization, action changes, scene requests, gameplay helpers, prefab helpers, grid helpers, script lifecycle callbacks, physics contact phases/filtering/query, asset manifest scan/save/load, config loading, resources, scene JSON, and selected-game package smoke. |

## Milestone 1: Developer Can Launch A Runtime Game

Required:

- Done: source-of-truth roadmap exists.
- Done: `GameConfig` type and flat JSON config loader.
- Done: `Engine` accepts config through constructor.
- Done: `--config`, `--runtime`, and `--editor` command-line options.
- Done: runtime mode skips editor UI and ImGui setup.
- Done: dedicated `RuntimeApp` and `EditorApp` wrappers exist.
- Done: asset and prefab directories come from config.
- Done: tests cover config defaults, config loading, app mode normalization, and selected-game package smoke.

## Milestone 2: Developer Can Create A Separate Game

Required:

- Done: `samples/snake` builds as a separate executable linked to `engine_core`.
- Done: `samples/flappy_bird` builds as a separate executable linked to `engine_core`.
- Done: reusable `engine_core` static library exists.
- Done: game entry points can implement `IGame` and pass the game object into the app runner.
- Done: baseline package script can produce a demo distribution folder and zip.
- Done: packaging can target one selected sample.
- Done: reusable `templates/basic_game` exists.

## Milestone 3: Developer Can Author Gameplay Efficiently

Required:

- Done: named keyboard and gamepad action bindings.
- Done: sprite animation component and editor/JSON support.
- Done: tilemap component and editor/JSON support.
- Done: simple text/HUD component and JSON support.
- Done: basic audio playback through `GameContext`.
- Done: asset manifest catalog and scan/import workflow.
- Done: collision layer/mask filtering, contact phases, and AABB overlap queries.
- Done: entity lookup helpers.
- Done: sprite/text/collider creation helpers.
- Done: prefab instantiation helpers through `GameContext`.
- Done: action pressed/released input events.
- Done: script lifecycle callbacks for create/start/update/fixed update/event/destroy.
- Done: scene reload/change APIs.
- Done: Snake sample uses grid helpers.
- Done: Flappy Bird sample uses runtime scene/prefab helpers.
