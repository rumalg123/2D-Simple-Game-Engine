# Basic Game Template

This is a minimal starting point for a new runtime game linked against `engine_core`. It demonstrates:

- `RuntimeApp` / `EditorApp` selection through `runGameApp`.
- `IGame` setup and command-line config loading.
- Named keyboard and gamepad input actions.
- Scene helper APIs for sprites and text.
- Runtime prefab registration and `GameContext::instantiatePrefab`.
- A small HUD and debug stats.

Build the template in this repository:

```powershell
cmake --build build --target basic_game_template
```

Run runtime-only:

```powershell
.\build\basic_game_template.exe --config .\templates\basic_game\basic_game.game.json
```

Package it:

```powershell
cmake --build build --target package_basic_game_template
```

To start a new game, copy this folder to `samples/my_game`, rename the source file and config, then add a CMake target that follows this pattern:

```cmake
add_executable(my_game
    samples/my_game/src/MyGame.cpp
)

target_link_libraries(my_game PRIVATE engine_core)
```

Controls:

- `WASD`, arrow keys, or left gamepad stick: move.
- `Space`, `Enter`, or gamepad A: confirm.
- `R`: reset scene.
- `Esc`: exit.
