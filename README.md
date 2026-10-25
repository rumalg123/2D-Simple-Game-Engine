# SimpleEngine

## Ready-to-run downloads

Portable Windows x64 builds are published automatically after each push or
merged pull request to `main`. Download the editor, standalone scene player,
engine demo, Snake, Flappy Bird, or basic game template ZIP from the
[latest GitHub Release](https://github.com/rumalg123/2D-Simple-Game-Engine/releases/latest).
Extract the ZIP and run `launch.bat`. Runtime DLLs, config files, and required
assets are included. The optional `GameFiles.zip` contains the original game
source/config/assets as a separate download.

See [release and compatibility details](docs/RELEASES.md).

For the planned Unity-style 2D creation platform, see the
[architecture and development roadmap](docs/ENGINE_ARCHITECTURE_ROADMAP.md).
Linux/macOS binaries are not currently distributed because audio and linking
are Windows-specific.

SimpleEngine is a small C++17 2D engine prototype with an ImGui editor, ECS scene model,
OpenGL sprite rendering, simple AABB physics, prefabs, JSON scene save/load, and a game hook
interface.

## Independent editor and scene player

`SimpleEngineEditor.exe` launches the visual editor; `SimpleEnginePlayer.exe`
runs serialized scenes without linking Dear ImGui. These targets are built
alongside the existing `engine.exe`, Snake, Flappy Bird and starter executables.

```powershell
.\build\ucrt64-debug\SimpleEngineEditor.exe --config .\demo.game.json
.\build\ucrt64-debug\SimpleEnginePlayer.exe --config .\examples\player-scene\game.config.json
```

### New/Open Project

Launch the editor without arguments to open the visual project launcher,
or create a starter project from the command line. New projects contain a
playable scene and can be opened directly in the editor or player.

```powershell
.\build\ucrt64-debug\SimpleEngineEditor.exe
.\build\ucrt64-debug\simpleengine_project.exe create . MyFirstGame
.\build\ucrt64-debug\SimpleEngineEditor.exe --project .\MyFirstGame
.\build\ucrt64-debug\SimpleEnginePlayer.exe --project .\MyFirstGame
```

In the editor, adjust the starter scene in Scene Hierarchy / Inspector and
choose **Save Scene JSON**. The standalone player then loads those changes.
The older `--config` demo mode still works.

See [Project Workflow](docs/PROJECT_WORKFLOW.md) for the full process,
[Standalone Editor and Player](docs/EDITOR_PLAYER.md) for runtime details,
and [Architecture Roadmap](docs/ENGINE_ARCHITECTURE_ROADMAP.md) for upcoming
scripting, non-destructive Play Mode, GUIDs and export tooling.

## Connected Terrain, Layers and Named Animation Clips

The editor now supports **4-direction terrain autotiling** (16 atlas
variants), named tilemap layers with independent draw ordering, and
multiple named sprite-sheet clips (such as Idle, Run, Jump) per animated
entity. Lua can change clips during gameplay using
`engine.play_animation("Run")` and inspect the active clip.

Use the Tilemap Painter's **Enable Terrain / Terrain Paint / Terrain Erase**
controls, **Add Tile Layer** in Scene Hierarchy, and the **Named Animation
Clips** section of Sprite Animation Timeline.

**Try it immediately:** [Terrain + Clips Demo](examples/terrain-clip-demo/README.md)
is an openable project with connected terrain, separate draw layers, Lua
movement and named Idle/Run animation clips.

See [Terrain, Clips & Layers](docs/TERRAIN_CLIPS_LAYERS.md) for exact atlas
mask ordering, CLI/gameplay examples, compatibility and limitations.

## Visual Tilemap Painter and Sprite Animation Timeline

The editor now has two dedicated level-authoring windows: a full-size,
zoomable **Tilemap Painter** with pencil, erase, flood fill, line, picker,
resize-preserve and pattern brushes; and a **Sprite Animation Timeline**
with frame scrubbing, step, pause, loop, and sprite UV preview. Changes are
saved through the existing scene JSON and run in the standalone player.

Tile collision queries now check nearby map cells rather than iterating
every tile, making large 2D levels more practical to simulate.

See [Tilemap & Animation Workflow](docs/TILEMAP_ANIMATION_WORKFLOW.md)
for usage, limits, tests and future plans.

## Lua gameplay scripting

Independent projects now run Lua 5.4 gameplay scripts without recompiling
C++. New projects include `Assets/Scripts/WelcomeController.lua`; the
starter scene attaches this script to its Welcome text entity. The Asset
Browser also supports creating Lua files and attaching them to entities.

Lua scripts can implement per-entity create/start/update/fixed-update/destroy
hooks and call engine functions for movement, action input, numeric parameters,
entity GUID and logging. Existing native C++ scripts still work.

```powershell
.\build\ucrt64-debug\simpleengine_project.exe create . MyLuaGame
.\build\ucrt64-debug\simpleengine_project.exe validate .\MyLuaGame
.\build\ucrt64-debug\SimpleEngineEditor.exe --project .\MyLuaGame
.\build\ucrt64-debug\SimpleEnginePlayer.exe --project .\MyLuaGame
```

Use **Stop → Reload Lua Scripts → Play** to test Lua changes. The standalone
player runs the same scripts from the project directory. A Lua 5.4 development
library is required for builds from source.

See [Lua Scripting](docs/LUA_SCRIPTING.md) for API examples, lifecycle
details, safety limits and Windows build requirements.

## Build

Use the **MSYS2 UCRT64** toolchain for this Windows C++17 project. The compiler,
GLFW and Lua must all come from UCRT64; MINGW64 libraries belong to a different
runtime. The presets assume MSYS2 is installed at `C:/msys64` and require
CMake 3.21 or newer.

Install the dependencies in an **MSYS2 UCRT64** terminal:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-gdb mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-binutils mingw-w64-ucrt-x86_64-glfw mingw-w64-ucrt-x86_64-lua54
```

Use `lua54`, which provides the required Lua 5.4 headers and library.

### VS Code

Open this repository folder and install the recommended **C/C++** and
**CMake Tools** extensions. Run **CMake: Select Configure Preset** and choose
**MSYS2 UCRT64 - Debug** if prompted.

- **Ctrl+Shift+B** runs **Build Engine**, configuring and compiling all targets.
- **F5** with **Debug Editor** opens the editor's New/Open Project launcher.
  **Run Engine** launches the demo; **Debug Player (sample scene)** runs the
  standalone player example. Each launch builds first and uses UCRT64 GDB.
- **Tasks: Run Test Task > Test Engine** builds and runs the regression suite.

IntelliSense gets each file's includes and definitions from CMake, with
`build/ucrt64-debug/compile_commands.json` available as a fallback. The terminal
profile **MSYS2 UCRT64** is also available in the terminal dropdown. Open a new
terminal after reloading the workspace so the updated tool paths take effect.

### PowerShell

From the repository root:

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
cmake --preset ucrt64-debug
cmake --build --preset ucrt64-debug
ctest --preset ucrt64-debug
.\build\ucrt64-debug\SimpleEngineEditor.exe
```

The presets explicitly select GCC, G++, Ninja and the UCRT64 dependency prefix,
so they also work when another CMake or compiler appears earlier on the system
PATH. They use `build/ucrt64-debug` and `build/ucrt64-release`, avoiding old
generator/compiler caches in `build`. MinGW runtime DLLs are copied beside each
executable automatically, and the demo's assets are copied after building it.

For an optimized build, use:

```powershell
cmake --preset ucrt64-release
cmake --build --preset ucrt64-release
ctest --preset ucrt64-release
```

If MSYS2 is installed elsewhere, change `MSYS2_ROOT` in `CMakePresets.json`
and the `C:/msys64` paths in `.vscode/*.json` to match the installation.

Run the demo with the default editor-enabled configuration:

```powershell
.\build\ucrt64-debug\engine.exe --config .\demo.game.json
```

Run the same game without editor panels:

```powershell
.\build\ucrt64-debug\engine.exe --config .\demo.game.json --runtime
```

Build the separate Snake sample game:

```powershell
cmake --build --preset ucrt64-debug --target snake_sample
.\build\ucrt64-debug\snake_sample.exe --config .\samples\snake\snake.game.json
```

Build the separate Flappy Bird sample game:

```powershell
cmake --build --preset ucrt64-debug --target flappy_bird_sample
.\build\ucrt64-debug\flappy_bird_sample.exe --config .\samples\flappy_bird\flappy_bird.game.json
```

Build the reusable basic game template:

```powershell
cmake --build --preset ucrt64-debug --target basic_game_template
.\build\ucrt64-debug\basic_game_template.exe --config .\templates\basic_game\basic_game.game.json
```

## Tests

Run the core regression tests with CTest:

```powershell
cmake --build --preset ucrt64-debug
ctest --preset ucrt64-debug
```

The current test suite covers input action bindings, app mode config normalization, resource reuse,
asset manifest scan/save/load, prefab asset load/spawn, scene/gameplay helpers, script lifecycle
callbacks, physics contact phases, filters, gravity, tilemap collision, JSON round-tripping for
text/tilemap/collider scene data, and a package smoke test for the basic game template.

## Package

Create a distributable folder and zip with the executable, runtime DLLs, assets, config, README,
launcher, and package manifest:

```powershell
.\tools\package.ps1
```

The default output is:

- `dist/SimpleEngine`
- `dist/SimpleEngine.zip`

You can also run the CMake target after configuring the project:

```powershell
cmake --build --preset ucrt64-debug --target package_game
```

Package the Snake sample as its own runtime game:

```powershell
cmake --build --preset ucrt64-debug --target package_snake_sample
```

Package the Flappy Bird sample as its own runtime game:

```powershell
cmake --build --preset ucrt64-debug --target package_flappy_bird_sample
```

Package the basic game template:

```powershell
cmake --build --preset ucrt64-debug --target package_basic_game_template
```

Or call the package script directly for a selected executable:

```powershell
.\tools\package.ps1 `
  -PackageName SnakeSample `
  -ExecutableName snake_sample.exe `
  -GameConfig .\samples\snake\snake.game.json `
  -AssetsPath .\samples\snake\assets `
  -ReadmePath .\samples\snake\README.md `
  -AllowMissingAssets
```

## Assets

Each asset folder can keep an `asset_manifest.json` catalog. The manifest stores stable asset IDs,
relative paths, asset types, display names, and metadata blocks for sprite sheets, animation clips,
fonts, and audio.

The editor asset browser can load, scan, and save the manifest. Scanning imports supported texture,
audio, font, scene, prefab, sprite-sheet metadata, and animation-clip metadata files without
duplicating existing paths. Texture and sprite-sheet assets can be assigned to the selected sprite
by ID, so saved scenes reference a stable asset name instead of an incidental editor path.

The editor asset browser can load scene assets, load prefab assets into the prefab registry, and
spawn prefab assets into the current scene. Game and plugin code can access the current manifest
through `GameContext::assets` and `PluginContext::assets`.

## Project Layout

- `engine_core` is the reusable static library target.
- `engine` is the current demo executable.
- `samples/snake`, `samples/flappy_bird`, and `templates/basic_game` are separate game projects linked to `engine_core`.
- Engine runtime/editor code lives in `include/engine` and `src`.
- The current game entry point is `src/main.cpp`.
- The current demo game implementation is `src/DemoGame.cpp`, including demo scripts and demo-only editor tools.
- The demo scene and prefab setup are in `src/DemoScene.cpp`.

## Creating A Game

Create a class that implements `IGame` from `include/engine/Game.h`.

Use the hooks as follows:

- `configureInput` binds game actions.
- `registerScripts` registers game behavior scripts.
- `loadInitialScene` creates the starting scene and prefabs.
- `registerPlugins` adds editor/game tooling.
- `onFixedUpdate` runs game logic during the fixed timestep.
- `onEvent` handles collisions, triggers, and input events.
- `debugStats` exposes small game stats in the editor panel.

Then pass the game object into the app runner:

```cpp
#include "App.h"
#include "MyGame.h"

int main() {
    return runGameApp(createMyGame(), {});
}
```

Use `RuntimeApp` directly when an executable must always run without editor panels. Use `EditorApp`
directly when an executable must always start with editor UI. `runGameApp()` chooses between them
from `GameConfig::editorEnabled`.

The current demo follows this pattern with `createDemoGame()`.

## Scripts

Register entity scripts from `IGame::registerScripts` through `GameContext::scripts`.

`ScriptSystem::ScriptLifecycle` supports:

- `onCreate` when a script instance first appears on an entity.
- `onStart` before the first active update, fixed update, or event.
- `onUpdate` once per rendered frame while the simulation is playing.
- `onFixedUpdate` during the fixed timestep.
- `onEvent` for key, action, collision, and trigger events while the simulation is playing.
- `onDestroy` before a tracked script entity is destroyed or the scene is cleared.

The older `registerScript(name, update, parameters)` form still works and maps to fixed update so
existing fixed-timestep gameplay scripts keep their original behavior.

## Input

`InputMap` lets games bind named actions to keyboard keys, gamepad buttons, and gamepad axes:

```cpp
context.inputMap->bindAction("MoveLeft", GLFW_KEY_A);
context.inputMap->bindGamepadButton("MoveLeft", GLFW_GAMEPAD_BUTTON_DPAD_LEFT);
context.inputMap->bindGamepadAxis("MoveLeft", GLFW_GAMEPAD_AXIS_LEFT_X, GamepadAxisDirection::Negative);
```

Gameplay code should read actions by name with `isDown()` or `getAxis()`. `InputComponent` uses
the same action names, so keyboard and controller input automatically drive entity movement. The
engine polls connected GLFW gamepads every frame and aggregates them into the active input state.

## Physics And Collision

`ColliderComponent` supports AABB collision with `solid`, `trigger`, `layer`, and `mask` fields.
Two colliders interact only when both masks include the other collider's layer.

Collision and trigger events include a `CollisionPhase`:

- `Enter` when a pair begins overlapping.
- `Stay` while the pair remains overlapping.
- `Exit` when a tracked pair separates.

`PhysicsSystem::queryAabb()` runs the same AABB and layer/mask filtering without moving entities or
publishing events. `PhysicsSettings` controls gravity and contact phase emission. Collider layer and
mask fields are editable in the inspector and persist through scene and prefab JSON.

Tilemaps can opt into collision. Non-negative tile cells become static tile AABBs using the
tilemap's collision layer, mask, solid, and trigger fields. Empty `-1` cells do not collide.

## Sprite Animation

Sprites support UV rectangles through `SpriteComponent::uMin`, `vMin`, `uMax`, and `vMax`.
Add `SpriteAnimationComponent` to an entity to animate across a grid-based sprite sheet.

The animation component uses:

- `columns` and `rows` for the sprite-sheet grid.
- `firstFrame`, `frameCount`, and `currentFrame` for frame selection.
- `secondsPerFrame`, `playing`, and `loop` for playback.

Frames advance left to right, then top to bottom. The editor inspector can add and tune sprite
animations, and scene/prefab JSON persists the component as `spriteAnimation`.

## Tilemaps

Entities can render grid tile layers with `TilemapComponent`. A tilemap uses a `TransformComponent`
as the top-left anchor, a tile size, an atlas grid, a texture, and a row-major `tiles` array.
Tile value `-1` leaves a cell empty; non-negative values select atlas frames left to right, then
top to bottom.

Tilemaps are expanded into the normal sprite render path, so they support texture atlases, tint,
alpha, layers, optional tile collision, scene/prefab JSON, and editor inspection. The demo scene
includes `DemoTilemap` using `assets/tiles.ppm`.

## Audio

`GameContext` exposes `AudioSystem* audio` for simple game feedback sounds. The audio system can
load PCM `.wav` files or create short procedural tones:

```cpp
AudioClipHandle hit = context.audio->loadWavFromFile("hit", "assets/hit.wav");
context.audio->play(hit);
```

Current audio support is intentionally small: Windows WinMM output, 8-bit or 16-bit PCM WAV,
mono or stereo, one-shot playback, and no mixer groups yet.

## Text And HUD

Entities can render simple block-font text with `TextComponent`. Text uses the existing sprite
batcher, so it supports tint, alpha, layers, scene/prefab JSON, and the same render path as other
2D content.

Add `TextComponent` to an entity with a `TransformComponent`. If `screenSpace` is true, the
transform is treated as a camera-relative position where `(-1, 1)` is near the top-left and
`(1, -1)` is near the bottom-right. If `screenSpace` is false, the text is placed in world space.

## Current Limits

The engine is currently best suited for small arcade games and prototypes. Important systems
still missing include broader automated coverage, release signing, and platform-specific installers.
