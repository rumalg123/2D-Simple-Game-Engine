# Serialized scene player example

This sample does not compile or register a C++ `IGame`. The generic player
loads the scene JSON directly from a game configuration file.

From the repository root after building:

```powershell
.\build\SimpleEnginePlayer.exe --config examples\player-scene\game.config.json
```

The config paths resolve relative to its own directory, so this command can
also be launched from another working directory with an absolute config path.

The scene displays two static text entities using the current block-font
renderer. Gameplay scripting, a full project manifest, and asset cooking
are future milestones.

To try a scene saved by the existing editor, use
`SimpleEnginePlayer.exe --config ... --scene <scene.json>`.
