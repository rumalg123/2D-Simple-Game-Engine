# Snake Sample

This sample is a separate game executable linked against `engine_core`. It intentionally lives outside the demo entry point so the engine can grow toward a real game project workflow.

Build:

```powershell
cmake --build build --target snake_sample
```

Run runtime-only:

```powershell
.\build\snake_sample.exe --config .\samples\snake\snake.game.json
```

Controls:

- `WASD` or arrow keys: change direction.
- `R`: reset scene.
- `Esc`: exit.

