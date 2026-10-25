# Phase 10: Persistent Asset Database and Managed Resources

SimpleEngine now upgrades the existing `Assets/asset_manifest.json` catalog
to support **stable asset GUIDs**, asset change fingerprints and dependencies.
It builds on the phase 9 game exporter rather than introducing a different
manifest format, another project root or new runtime dependency.

## Why GUIDs?

Asset file names are for organization, not identity. Unity and Godot use
identifiers to preserve references when assets move; SimpleEngine now does
the same for texture and sprite-sheet references.

Each indexed file has the following core metadata:

```json
{
  "id": "texture:sprites/hero",
  "guid": "6f873142df3741a4a7e0a85e3d547abc",
  "type": "texture",
  "path": "Sprites/Hero.png",
  "contentHash": "b4dd34070a935ea7",
  "sourceBytes": 9022,
  "dependencies": []
}
```

- **GUID:** path-independent 128-bit randomly generated identity. Preserved
  across supported managed moves and future source edits.
- **ID:** the existing legacy identifier remains unchanged when moving a
  cataloged resource. Existing scene `textureName` fields keep resolving.
- **Hash:** 64-bit FNV-1a content fingerprint to tell whether source content
  has changed after an import. It is **not** a cryptographic integrity hash
  or a substitute for the exporter's SHA-256 inventory.
- **Dependencies:** catalog IDs referenced by the asset. They are currently
  inferred from saved scene/prefab `textureName`, `scriptName` and animation
  clip sheet metadata. This is a targeted dependency graph, not a parser for
  every potential JSON/user extension.

## CLI workflows

After building, commands operate on independent user-created projects:

```powershell
# Scan sprites, Lua, scenes, prefab, font and audio files.
.\build\simpleengine_project.exe assets-sync "C:\Games\MyGame"

# Diagnose deleted files, missing references and GUID problems.
.\build\simpleengine_project.exe assets-audit "C:\Games\MyGame"

# Show IDs, GUIDs, file paths and fingerprints.
.\build\simpleengine_project.exe assets-list "C:\Games\MyGame"

# Explain what an asset uses and which other assets use it.
.\build\simpleengine_project.exe assets-deps "C:\Games\MyGame" "texture:sprites/hero"

# Rename a texture while preserving its ID/GUID:
.\build\simpleengine_project.exe assets-move "C:\Games\MyGame" "texture:sprites/hero" "Sprites/hero-renamed.png"
```

Move destinations must refer to **an existing directory within Assets**.
The operation rejects parent traversal, symlinks, file overwrites and
type changes. It updates the file and saves the catalog. Scene, prefab
and Lua script moves are currently blocked because their startup/script
registration paths have not yet been migrated to stable references.

**Import/rescan first.** Files created after the last scan are not yet
cataloged; running `assets-sync` adds them. Import rescans keep IDs and
GUIDs and update changed source hashes.

## Using the editor

In **Asset Browser**, select **Scan Assets** to import assets and save the
manifest. The list displays GUID, source hash and size, and allows copying
the GUID. **Audit Asset Database** reports missing catalog files or broken
references. For supported resources, **Move / Rename** opens a dialog to
specify a new path relative to the Assets directory.

After moving a texture, reopen the scene to refresh the current
ResourceManager's cached file-backed texture. Editing a running scene while
moving a resource is disabled.

## Stable scene and prefab texture lookup

When a scene or prefab is loaded, SimpleEngine looks for
`Assets/asset_manifest.json` by walking up from the document directory.
A `textureName` matching a catalog ID is resolved to the **current** asset
path. A stale `texturePath` from before a move no longer breaks the link.
Projects without an asset catalog continue using their legacy paths.

The game exporter also resolves catalog IDs while rewriting texture paths
for the distribution ZIP. Therefore, after a supported managed rename,
the exported game contains the renamed resource and a portable path, not
the outdated absolute authoring-machine path.

## Project validation and CI

Projects that contain a saved asset database must pass its audit before
`simpleengine_project validate` and the Windows exporter will accept them.
Projects without an asset database remain compatible.

New CTest targets exercise:
- Importing scenes, textures and Lua scripts with persistent GUIDs.
- Safe moves including parent traversal, missing destination and type changes.
- Loading scenes via catalog ID after the resource moves.
- Preserving GUIDs when source contents and fingerprints change.
- Reopening and auditing manifests, including missing source files.
- Running the real project CLI through sync, list, audit, move and failure cases.
- Exporting a game with a moved catalog texture, rewriting outdated paths and
  validating the standalone executable.

## Current limits and follow-on architecture

- The GUID catalog is centralized in the existing source asset manifest,
  **not individual .meta files**; keep it under version control.
- FNV source fingerprints do not generate GPU-optimized build artifacts.
- Runtime texture resource loading still relies on supported formats;
  more advanced texture import settings and image transcoding require a
  separate pipeline.
- Project asset dependency inference is intentionally limited to known
  engine reference fields, and scripts are **not** parsed for dynamic asset
  loads. Prefab/script/scene managed moves are prohibited until those
  references are migrated.
- No background database watcher, compiled asset-cache directory, automatic
  multi-user merge reconciliation, or binary packfile yet.
