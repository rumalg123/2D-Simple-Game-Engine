# Terrain + Animation Clips Demo

A complete project you can **open and play directly** with SimpleEngine.
No C++ recompilation or manual scene setup is required after building the
editor/player executables.

```powershell
.\build\ucrt64-debug\simpleengine_project.exe validate ".\examples\terrain-clip-demo"
.\build\ucrt64-debug\SimpleEngineEditor.exe --project ".\examples\terrain-clip-demo"
.\build\ucrt64-debug\SimpleEnginePlayer.exe --project ".\examples\terrain-clip-demo"
```

In the editor, explore the four scene entities:

- **Background Layer:** ordinary decorative 12×7 tilemap, draw layer -200.
- **Connected Ground:** 12×2 terrain occupancy map, 16-variant north/east/
  south/west autotiling, draw layer -50, collision enabled.
- **Lua Player:** colored sprite with `Idle` and `Run` clips and
  `TerrainPlayer.lua` which changes clips as horizontal input changes.
- **Instructions:** text overlay with the default keyboard controls.

Press **Play** in the editor or run the standalone player. Hold **A/D** or
the arrow keys to move and switch between the named clips.

Select the Ground map and use the Tilemap Painter's Terrain Paint / Erase
brushes to change its connected floor. Click Save Scene, reopen, and verify
that terrain occupancy and animation clips survive reload.

This demo uses colored default-white textures and a fixed logical atlas;
it illustrates gameplay and asset metadata, not finished sprite artwork.
Import a 4×4 terrain atlas and a 4-column sprite sheet to see texture-specific
terrain edges and animated frames. The demo source is not a benchmark.

Its file layout follows the same independent project format as projects
created by SimpleEngine's Project Hub. CTest runs
`terrain_demo_project_validate` to ensure the sample remains loadable and
its Lua source compiles.
