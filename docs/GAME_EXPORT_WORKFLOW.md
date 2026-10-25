# Phase 9 — Export your own Windows games

This release introduces an end-to-end way to distribute games created with
SimpleEngine: validate, package, verify, and share a playable Windows ZIP.

## Build from the editor

1. Extract the **SimpleEngineEditor-Windows-x64.zip** release.
2. Open or create an independent game project.
3. Edit scenes, import art, create Lua scripts, and **Save Scene JSON**.
4. **Stop** Play Mode and select **Build Windows Game** in the **Build Game** panel.
5. The editor launches a PowerShell console containing validation and export
   output; check that it reports successful export.
6. For a project at `C:\Games\MyGame` the ZIP is placed in
   `C:\Games\MyGame_Exports\MyGame.zip`.

The editor download now includes the editor-free player, project validator,
export script and Windows runtime dependencies, so end users do not need to
clone the engine repository or compile C++ to export a project.

## Export from the CLI

First build the engine in Windows/MSYS2 UCRT64, then run:

```powershell
.\build\simpleengine_project.exe validate "C:\Games\MyGame"
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\export-project.ps1 -ProjectPath "C:\Games\MyGame" -BuildDir "build"
```

The script takes optional `-OutputDir`, `-PackageName`, `-NoZip` and
`-SkipBuild` arguments. With the prebuilt editor ZIP use
`-BuildDir` pointing to the extracted editor directory and `-SkipBuild`.

The result is a directory and a ZIP:

```text
MyGame.zip
MyGame/
  MyGame.exe
  Play.bat
  PLAY.txt
  project.simple.json
  export-manifest.json
  Assets/
    Scenes/Main.scene.json
    Scripts/...
    Sprites/...
    Audio/...
  licenses/Lua.txt
  [Windows/MinGW/UCRT/Lua runtime DLLs]
```

Players unzip the game and double-click **MyGame.exe** or **Play.bat**.
The runtime automatically detects the adjacent project manifest and resolves
assets relative to the executable's directory, even when launched through
a terminal with a different working directory. No editor is required.

## Headless self-check

```powershell
.\MyGame.exe --verify-bundle
```

This uses the same project validator as `simpleengine_project validate`,
without starting OpenGL or Lua gameplay execution. It validates the project
manifest, **all** saved scene files, Lua syntax and script references.

## Safety and portability

The exporter refuses to package any project that fails validation, has
asset paths outside its project, contains symlinks/junctions, hidden/cache
directories or unrecognized file formats. It also checks that referenced
textures are inside the project's Assets directory and rebases any source
absolute paths in exported JSON to project-relative paths.

It copies only recognized runtime asset types, runtime DLLs, the player and
license notices, not user source-code repositories. It stages all output
before publication, refuses to overwrite existing exports, and writes
a JSON inventory containing every shipped file's path, byte size and
**SHA-256** hash.

**Review your project data before sharing.** JSON files can still contain
secrets even if their extension is allowed. SHA-256 checksums are not
cryptographic signatures, and game Lua scripts/other assets are shipped
as readable files, not encrypted. Projects from untrusted sources may execute
unsafe code during gameplay.

## Automated validation

The Windows PR pipeline now includes a project export smoke test that:
- Generates a new project with Lua and a startup scene.
- Exports the project, verifies the ZIP, checks runtime DLLs and SHA-256 hashes.
- Launches the game executable with `--verify-bundle` without a GUI.
- Confirms the CLI can validate the **exported copy**, not just the source.
- Inserts bad Lua and a hidden secrets file to ensure an export fails safely.
- Confirms failed exports do not replace an existing playable ZIP.

The editor package smoke test verifies the downloadable editor contains the
exporter, project CLI, Lua license and standalone player. On merge to `main`
the release pipeline also includes `TerrainClipsGame.zip` as an automatically
exported example project.

## Current scope and future phases

Windows x64 is supported. Linux/macOS export requires cross-platform audio
and CMake link portability. A full asset database with persistent GUIDs,
export presets, asset pack encryption/compression, code signing, icons,
change-only builds, and editor-embedded progress reporting are planned later.
This exporter is not yet a full Unity Build Profiles replacement.
