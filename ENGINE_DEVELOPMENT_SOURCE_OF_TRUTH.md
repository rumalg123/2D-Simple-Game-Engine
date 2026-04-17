# 2D Engine Development Source Of Truth

This file is the working source of truth for turning SimpleEngine from a demo-oriented C++ 2D engine prototype into something a game developer can use to build and package small 2D games such as Snake, Flappy Bird, and arcade prototypes.

Status values:

- Done: implemented and verified.
- In Progress: active implementation has started.
- Planned: accepted requirement, not started yet.

## Current Focus

1. Game/runtime configuration and runtime-only mode.
2. Separate sample game projects for Snake and Flappy Bird.
3. Game-facing helper APIs so developers do not have to manually assemble every component.

## Roadmap

| Status | Area | Requirement | Notes |
| --- | --- | --- | --- |
| Done | Source of truth | Keep this file as the canonical roadmap | Update status here as features are implemented. |
| In Progress | Game project workflow | Add a project template and sample game project layout | `samples/snake` now builds as a separate executable. `samples/flappy_bird` and `templates/basic_game` remain planned. |
| Done | Runtime configuration | Add per-game config for title, resolution, editor/runtime mode, asset paths, hot reload, and vsync | Implemented with `GameConfig`, flat JSON loading, and `demo.game.json`. |
| In Progress | Runtime/editor split | Allow packaged games to run without editor panels | Basic runtime mode skips ImGui/editor UI and still runs rendering, input, audio, scripts, and physics. Dedicated `EditorApp`/`RuntimeApp` split is still planned. |
| Planned | Packaging | Package a selected game project, not only the demo executable | Output should include game executable, assets, config, runtime DLLs, and manifest. |
| Planned | Gameplay API | Add high-level helpers for sprites, text, colliders, prefabs, tags, and entity lookup | Avoid requiring game code to manually construct every ECS component. |
| Planned | Script lifecycle | Add script callbacks for create/start/update/fixed update/destroy/collision/trigger/input | Support multiple scripts per entity and script-owned state later. |
| Planned | Input events | Add action pressed/released events | Needed for deterministic grid games like Snake. |
| Planned | Scene management | Add game-facing scene load/reload/change APIs | Needed for menus, game over, level restart, and pause screens. |
| Planned | Asset pipeline | Add asset manifest, stable asset IDs, sprite sheet metadata, animation clips, font assets, audio metadata, and import workflow | Texture loading already exists; metadata and project ownership are missing. |
| Planned | 2D physics/collision | Add collision layers/masks, enter/stay/exit events, queries, tilemap collision, and physics scene settings | Current AABB physics is enough for simple prototypes but not enough for a fuller engine. |
| Planned | Grid gameplay | Add grid/tile helpers for games like Snake | Snake should not depend on continuous physics. |
| Planned | UI system | Add game UI primitives: font rendering, buttons, anchors/layout, menus, overlays | Current block text is useful only for simple HUDs. |
| Planned | Editor tools | Improve inspector component editing, drag/drop assets, prefab editing, tile painting, collider editing, scene picker, undo/redo, and play/edit separation | Current ImGui editor is useful but still prototype-level. |
| In Progress | Samples | Add complete Snake and Flappy Bird samples | Snake sample has started as `snake_sample`; Flappy Bird sample remains planned. |
| Planned | Tests | Expand regression coverage around config, runtime mode, scene management, input events, helpers, and packaging | Existing tests cover input bindings, resources, and scene JSON basics. |

## Milestone 1: Developer Can Launch A Runtime Game

Required:

- Done: source-of-truth roadmap exists.
- Done: `GameConfig` type and flat JSON config loader.
- Done: `Engine` accepts config through constructor.
- Done: `--config`, `--runtime`, and `--editor` command-line options.
- Done: runtime mode skips editor UI and ImGui setup.
- Done: asset and prefab directories come from config.
- Done: tests cover config defaults and config loading. Build passes; local execution is currently blocked by Windows Application Control.

## Milestone 2: Developer Can Create A Separate Game

Required:

- Done: `samples/snake` builds as a separate executable linked to `engine_core`.
- Planned: `samples/flappy_bird` builds as a separate executable linked to `engine_core`.
- Planned: reusable `templates/basic_game` exists.
- Planned: packaging can target one selected sample.

## Milestone 3: Developer Can Author Gameplay Efficiently

Required:

- Planned: entity lookup helpers.
- Planned: sprite/text/collider creation helpers.
- Planned: action pressed/released input events.
- Planned: scene reload/change APIs.
- Planned: Snake sample uses grid helpers.
- Planned: Flappy Bird sample uses runtime scene/prefab helpers.
