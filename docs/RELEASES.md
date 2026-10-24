# Downloading automated game builds

GitHub Actions publishes Windows x64 application ZIPs when a commit lands on
`main`, including a pull request merge. No workflow is started for a PR before
it is merged and no build is published for pushes to other branches.

## Downloads

Open [Latest Release](https://github.com/rumalg123/2D-Simple-Game-Engine/releases/latest)
and select the archive for the program you want to run:

| File | Contents |
| --- | --- |
| `SimpleEngine-Windows-x64.zip` | Legacy engine/demo, assets, config, DLLs, and launcher |
| `SimpleEngineEditor-Windows-x64.zip` | Independent editor executable, demo assets and launcher |
| `SimpleEnginePlayer-Windows-x64.zip` | Editor-free serialized-scene player, welcome scene, DLLs and launcher |
| `SnakeSample-Windows-x64.zip` | Playable standalone Snake game |
| `FlappyBirdSample-Windows-x64.zip` | Playable standalone Flappy Bird game |
| `BasicGameTemplate-Windows-x64.zip` | Standalone starter game |
| `GameFiles.zip` | Original game source files, per-game JSON config and available assets; does not contain executables |
| `SHA256SUMS.txt` | SHA-256 checksums for the ZIP downloads |

To start a Windows game, extract its ZIP into a folder, keep its files together,
and double-click `launch.bat`. The launcher uses the package-local `game.config.json`. Legacy games use
`--runtime`, the independent editor uses `--editor`, and the independent
player uses `--config` without editor-specific command-line flags.

The separate `GameFiles.zip` is **optional** when playing. Each standalone
Windows ZIP already contains its required configuration and runtime files.
The engine ZIP includes the root `assets/` folder; Snake, Flappy Bird, and
the basic template currently have no dedicated asset directories, so their
packages include empty `assets/` folders by design.

## Build and release pipeline

The workflow is `.github/workflows/publish-game-downloads.yml`.

1. Check out the exact new `main` commit.
2. Install Windows GCC, CMake, Ninja, GLFW, and binutils in MSYS2 UCRT64.
3. Build the engine, games, template, and tests using CMake.
4. Inspect the compiled executables and their DLL dependencies; bundle UCRT64
   runtime DLLs while excluding Windows system DLLs.
5. Run CTest, including the packaging smoke test.
6. Package the six applications with `tools/package.ps1`.
7. Generate the separate game files download with `tools/package-game-files.ps1`.
8. Calculate SHA-256 checksums and publish eight assets to a GitHub Release,
   tagged `build-<GitHub Actions run number>`.

A failed build or failed test stops the release, so a failed pipeline never
intentionally publishes incomplete download assets. The Release is marked
**Latest** only after all build and packaging steps have passed. Builds are
serialized to prevent concurrent releases overwriting each other.

GitHub's automatically generated source-code archives are **not** runnable
applications. Download one of the `*-Windows-x64.zip` assets instead.

## Platform limitations

This repo is currently Windows-focused. `src/AudioSystem.cpp` depends on
Windows WinMM, and `CMakeLists.txt` directly links `opengl32`, `gdi32`,
and `winmm`. Therefore the workflow does not publish misleading Linux or
macOS executables. To support those platforms, a cross-platform audio backend,
platform-conditional CMake linking, native packaging, and per-platform CI
tests are required.

The Windows builds are unsigned portable ZIPs, not installers. On some Windows
machines, SmartScreen may warn because the files are not code-signed.
