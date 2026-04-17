# Flappy Bird Sample

This sample is a separate game executable linked against `engine_core`. It uses runtime input events, scene helpers, and prefab instantiation to build a small Flappy Bird style arcade loop.

Build:

```powershell
cmake --build build --target flappy_bird_sample
```

Run runtime-only:

```powershell
.\build\flappy_bird_sample.exe --config .\samples\flappy_bird\flappy_bird.game.json
```

Controls:

- `Space`, `W`, or up arrow: flap.
- `R`: reset scene.
- `Esc`: exit.

