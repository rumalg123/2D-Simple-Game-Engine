# Phase 11: Incremental Asset Imports and Compiled Game Textures

SimpleEngine now compiles project assets to a versioned disposable cache,
based on the existing persistent asset GUID database and dependency graph.

## Location and lifecycle

The source catalog remains at `Assets/asset_manifest.json` under version control.
Compiled outputs live in `.simpleengine/cache/<platform>/` outside `Assets`.
The `.simpleengine/` directory is Git-ignored and can be deleted safely.
Each build publishes an `artifact-map.tsv` mapping an asset ID/GUID to its
exact current cache artifact. Source files are never rewritten by importing.

## Supported importers

Textures are compiled to binary `SEIMG001` containing an eight-byte magic,
little-endian width and height (u32 each), and RGBA8 pixels in row order.
The runtime resource manager validates header, dimensions and full payload.
The importer can downscale using deterministic nearest-neighbor sampling,
flip vertically and premultiply alpha. Do not enable premultiplied alpha
unless the renderer's blending mode expects that format.

Other indexed file types currently produce unchanged `.seblob` byte copies.
These are cached dependency-aware artifacts, not compiled Lua or audio.

## Development commands (Windows PowerShell)

```powershell
.\build\simpleengine_project.exe assets-build "C:\Games\MyGame"
.\build\simpleengine_project.exe assets-import-settings "C:\Games\MyGame" "texture:sprites/hero" 1024 0 0
.\build\simpleengine_project.exe assets-build "C:\Games\MyGame"
.\build\simpleengine_project.exe assets-clean "C:\Games\MyGame"
```

`assets-build` synchronizes source fingerprints, builds dependencies first,
and reuses artifacts whose content/settings/platform/dependency keys match.
The optional platform argument controls cache isolation; `linux-x64` does
not mean the game engine can build a native Linux executable yet.

## Export and the Build Game panel

In the editor's Build Game panel, check **Use compiled texture artifacts**.
The exporter runs the importer and copies compiled textures as
`Assets/Imported/<GUID>.seimg`, updating only exported scene texture paths.
The source project remains unchanged. Equivalent CLI:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\export-project.ps1 -ProjectPath "C:\Games\MyGame" -BuildDir build -UseImportedTextures
```

Source images and the original catalog are retained in the package for
backward compatibility, so this feature does not yet shrink the ZIP.
Only indexed `textureName` scene references are rewritten to compiled paths.

## Cache keys and safety

A cache key includes GUID, source fingerprint, importer version, platform,
texture settings and the keys of referenced dependencies. Editing a texture
invalidates that texture and scene artifacts depending on it. Platform
variants have separate caches and cache cleanup affects only one target.
Importer sources are bounded to 256 MiB, target names are validated,
and imported texture sizes and data lengths are checked at runtime.

Cache fingerprints use fast FNV-1a, not a cryptographic security hash.
The Windows exporter's SHA-256 inventory is the distribution integrity check.

Future work includes high-quality resize, GPU compression and mipmaps,
multithreaded import tasks, audio/font conversion and cache pruning.
