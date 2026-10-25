# Standalone editor and generic scene player

This implements the first working separation between the authoring executable
and the runtime executable. It is **not yet** a complete Unity-style project
manager or a scripting environment.

## Binaries

| Executable | Entry point | Libraries | Purpose |
| --- | --- | --- | --- |
| \`SimpleEngineEditor.exe\` | \`src/editor/EditorMain.cpp\` | \`engine_core\`, \`se_editor\`, \`se_editor_ui\`, \`se_runtime\` | Existing visual editor with demo-game integration |
| \`SimpleEnginePlayer.exe\` | \`src/player/PlayerMain.cpp\` | \`se_player_core\`, \`se_runtime\` | Render and simulate serialized scenes without Dear ImGui |
| \`engine.exe\` | \`src/main.cpp\` | existing compatibility targets | Existing demo CLI, preserved |
| sample executables | existing sample main functions | existing compatibility targets | Preserved until project-based scripting is introduced |

CMake compiles \`src/Engine.cpp\` twice for this transitional split:
- \`engine_core\`: regular editor-enabled implementation.
- \`se_player_core\`: \`SIMPLEENGINE_PLAYER_ONLY=1\`, which removes editor
  initialization, UI processing, input-capture and teardown calls at compile
  time.

The player directly links \`se_player_core\` and \`se_runtime\`, not \`se_editor\`
or Dear ImGui. An exported runtime player can therefore remain much smaller
than the authoring editor. The long-term roadmap plans to replace this
compile-time variant with explicit composition via \`EditorContext\` and
separate editor services.

## Build and run

Windows/MSYS2 UCRT64:

\`\`\`sh
cmake --preset ucrt64-debug
cmake --build --preset ucrt64-debug
ctest --preset ucrt64-debug
\`\`\`

Launch the editor (bundled demo):

\`\`\`powershell
.\build\ucrt64-debug\SimpleEngineEditor.exe --config .\demo.game.json
\`\`\`

Launch the standalone player using the bundled example project:

\`\`\`powershell
.\build\ucrt64-debug\SimpleEnginePlayer.exe --config .\examples\player-scene\game.config.json
\`\`\`

The player supports \`--config PATH\`, \`--scene PATH\`, and \`--help\`.
Config paths and scene overrides are resolved relative to the directory of
the supplied config file, so a portable package launches correctly when run
from another working directory.

\`startupScene\` is an optional string in the existing flat game JSON config.
For example:

\`\`\`json
{
  "windowTitle": "My Scene",
  "windowWidth": 960,
  "windowHeight": 540,
  "editorEnabled": false,
  "startupScene": "assets/scenes/welcome.scene.json",
  "assetRoot": "assets",
  "prefabDirectory": "assets/prefabs"
}
\`\`\`

Scene JSON uses the existing \`SceneSerializer\` format. An invalid scene path
must fail startup with a diagnostic; it must not silently render an empty game.

## What this milestone proves

- A generic game runtime can start without an \`IGame\` implementation.
- The player can read existing serialized scene files.
- Dear ImGui and editor libraries are absent from the player link graph.
- Windows PR CI builds both new binaries and checks for ImGui symbols in the
  player, plus a headless config/scene loading fixture test.
- Release packaging can ship separate editor and player ZIPs alongside the
  existing legacy demonstration packages.
- Existing demos, saved scenes and game executables remain source compatible.

## Current limitations

- The example scene is static, with basic text components.
- Project manager, Lua scripting, editor authoring documents, scene-graph
  hierarchy, asset cooking and one-click per-project builds are not built.
- Existing editor is still backed by the demo and an \`Engine\` class with
  private state accessed by editor methods.
- Editor and player are currently Windows-focused because WinMM audio and
  platform-specific linking have not been abstracted yet.
- Game projects using user-defined C++ scripts still need separately compiled
  game code until the scripting system ships.

## Next phase

1. Add a versioned project manifest and \`ProjectContext\`, independent of the
   \`IGame\` and demo executable model.
2. Implement New/Open Project in the editor and portable project templates.
3. Add stable scene/entity/resource identity and a safe Edit/Play world split.
4. Introduce Lua scripts and an asset import database, then adapt the player
   to run exported project packs.
5. Extract remaining editor state behind \`EditorContext\` and command APIs.
