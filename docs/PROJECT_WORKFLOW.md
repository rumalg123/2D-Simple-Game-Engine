# Creating SimpleEngine game projects (foundation)

This is the first user-project feature from the [engine architecture roadmap](ENGINE_ARCHITECTURE_ROADMAP.md).
Projects no longer have to start as edits to a C++ sample directory.

## Commands

Build on Windows using the existing UCRT64/CMake toolchain:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target simpleengine_project
```

Create and inspect a game project:

```sh
./build/simpleengine_project.exe create . "My Platformer"
./build/simpleengine_project.exe info "./My Platformer"
```

The `create` operation refuses to overwrite an existing directory, validates
portable project names, and writes the following structure:

```text
My Platformer/
├── project.simple.json
├── Assets/
│   ├── Scenes/
│   ├── Sprites/
│   ├── Scripts/
│   ├── Audio/
│   └── Prefabs/
└── ProjectSettings/
```

The manifest contains initial window settings and relative asset, prefab and
editor-layout paths. `ProjectManager::open` resolves those paths to the
project directory, rejects path traversal and symlinks leaving the root,
and returns a `ProjectInfo` with paths ready for runtime services.

Run `ctest --test-dir build --output-on-failure -R project_manager_tests`
for creation/reopen/validation regression coverage.

## Current limitations

- **CLI project creation/opening is implemented. The visual New Project /
  Open Project UI is not implemented yet.**
- `ProjectManager` currently reuses `GameConfig` and its flat JSON parser
  rather than a full versioned project/scene schema. The manifest's
  `formatVersion` field is reserved for the future migration layer.
- No main scene, stable GUIDs or project-specific Lua gameplay execution yet.
- The existing `engine` binary still runs the built-in demo; it does not
  load arbitrary project gameplay automatically.
- The editor and player have not yet been separated into independently linked
  applications; `engine_core` still links editor code.
- As before, builds are currently Windows-oriented.

## Next integration step

Introduce a first-class `ProjectContext` in the editor: a start screen for
New/Open Project, remembered recent projects, and the ability to initialize
`Engine` from a validated `ProjectInfo`. Then add persistent scene documents
and a true editor-free player.
