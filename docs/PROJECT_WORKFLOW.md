# Create, edit and play independent game projects

This is the first complete project-oriented editor workflow, building on the
[architecture roadmap](ENGINE_ARCHITECTURE_ROADMAP.md). The standalone project
format remains the flat `GameConfig` JSON prototype, not a stable versioned SDK.

## Visual workflow

1. Run `SimpleEngineEditor.exe` **without command-line arguments**.
2. The **SimpleEngine Projects** launcher opens, with sections for **Create
   Project** and **Open Project**.
3. Enter a parent directory and project name, then select **Create and Open**.
   Or enter an existing project directory and choose **Open Existing Project**.
4. The editor opens `Assets/Scenes/Main.scene.json` without the bundled
   C++ demo game. The scene contains a default Welcome text entity.
5. Use **Scene Hierarchy** to add/duplicate/delete entities, **Object
   Inspector** to adjust their components, and **Engine → Save Scene JSON**
   to save changes to your configured startup scene.
6. Close the editor, then play that same scene using:

```powershell
.\SimpleEnginePlayer.exe --project "C:\Games\MyFirstGame"
```

The editor and player are separate executables. The player links only
`se_player_core` and `se_runtime` and does not load Dear ImGui.

## Direct startup (automation and existing projects)

You do not need the graphical launcher if the project already exists:

```powershell
.\SimpleEngineEditor.exe --project "C:\Games\MyFirstGame"
.\SimpleEnginePlayer.exe --project "C:\Games\MyFirstGame"
```

For the built-in C++ demo, the legacy configuration mode is preserved:

```powershell
.\SimpleEngineEditor.exe --config demo.game.json
.\SimpleEngineEditor.exe --demo
```

The existing `engine.exe`, Snake, Flappy Bird and standalone editor/player
packaging targets remain in the repository.

## Headless project commands

```powershell
.\simpleengine_project.exe create "C:\Games" "MyFirstGame"
.\simpleengine_project.exe info "C:\Games\MyFirstGame"
```

A newly generated project contains:

```text
MyFirstGame/
├── project.simple.json
├── Assets/
│   ├── Scenes/
│   │   └── Main.scene.json
│   ├── Sprites/
│   ├── Scripts/
│   ├── Audio/
│   └── Prefabs/
└── ProjectSettings/
```

Its manifest points to `Assets/Scenes/Main.scene.json` with the
`startupScene` property. `ProjectManager::open` validates project-relative
configuration paths and that the configured scene exists within the project.
The generated starter scene uses the existing scene serializer format and
is covered by `project_manager_tests`.

## Windows build and tests

The existing Windows/MSYS2 UCRT64 workflow builds all executables, including
the launcher and the editor-free player, then runs CTest:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

The new project-scene test loads the generated scene with
`loadSceneFromJson()`, and also checks that missing startup scenes and
escaping paths cannot be opened.

## Boundaries and next steps

- New/Open Project is a **separate visual launcher window**. It does not
  yet support a remembered Recent Projects list or a native OS folder picker.
- When launched with `--project`, the editor uses project settings and the
  scene, without compiling any custom game C++ code. User Lua scripts are not
  implemented yet.
- Save Scene writes to the project's configured startup scene. A legacy
  editor session without a startup scene still uses `assets/scene_saved.json`.
- The new project uses a flat manifest, so a formal project schema version,
  schema migrations and persistent entity/asset GUIDs remain future work.
- Editor Play/Pause currently shares the authored scene rather than cloning
  a simulation world. Non-destructive Play Mode requires a separate document
  and runtime scene snapshot milestone.
- Windows is the first supported editor/player target. Cross-platform audio,
  exporter and plugin SDK will be addressed in later phases.
