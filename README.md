# SimpleEngine

SimpleEngine is a small C++17 2D engine prototype with an ImGui editor, ECS scene model,
OpenGL sprite rendering, simple AABB physics, prefabs, JSON scene save/load, and a game hook
interface.

## Build

```powershell
cmake -S . -B build
cmake --build build
```

The executable is written to `build/engine.exe`. Assets are copied beside the executable after
each build.

Run the demo with the default editor-enabled configuration:

```powershell
.\build\engine.exe --config .\demo.game.json
```

Run the same game without editor panels:

```powershell
.\build\engine.exe --config .\demo.game.json --runtime
```

Build the separate Snake sample game:

```powershell
cmake --build build --target snake_sample
.\build\snake_sample.exe --config .\samples\snake\snake.game.json
```

## Tests

Run the core regression tests with CTest:

```powershell
cmake --build build --target engine_tests
ctest --test-dir build --output-on-failure
```

The current test executable covers input action bindings, resource reuse, and JSON round-tripping
for text and tilemap scene data.

## Package

Create a distributable folder and zip with the executable, runtime DLLs, assets, README, launcher,
and package manifest:

```powershell
.\tools\package.ps1
```

The default output is:

- `dist/SimpleEngine`
- `dist/SimpleEngine.zip`

You can also run the CMake target after configuring the project:

```powershell
cmake --build build --target package_game
```

## Project Layout

- `engine_core` is the reusable static library target.
- `engine` is the current demo executable.
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

Then pass the game object into the engine:

```cpp
#include "Engine.h"
#include "MyGame.h"

int main() {
    Engine engine(createMyGame());

    if (!engine.init()) {
        return -1;
    }

    engine.run();
    engine.cleanup();
    return 0;
}
```

The current demo follows this pattern with `createDemoGame()`.

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
alpha, layers, scene/prefab JSON, and editor inspection. The demo scene includes `DemoTilemap`
using `assets/tiles.ppm`.

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
