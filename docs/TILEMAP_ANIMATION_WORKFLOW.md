# Tilemap Painter, Sprite Animation Timeline and Collision Optimization

This phase expands SimpleEngine's 2D level-authoring tools while preserving the
existing scene JSON, sprite sheet format, project startup and Lua scripting.

## Visual Tilemap Painter

In SimpleEngineEditor, create or select an entity containing a Tilemap
component. The new **Tilemap Painter** window provides:

- A scrollable, zoomable full-map canvas with indexed tile colors and labels.
- **Pencil:** hold left mouse and drag across tiles. One undo checkpoint is
  recorded for the full drag gesture.
- **Eraser:** drag to set cells to -1 (an empty, noncollidable tile).
- **Flood Fill:** replace a connected area of the same tile value.
- **Line:** drag from the start tile to the end tile.
- **Eyedropper:** sample an existing tile index as the active brush.
- **Atlas Brush:** select an index in the atlas (current maximum 16x16).
- **Resize (Preserve):** change tilemap dimensions without losing existing
  cells that remain in bounds. New cells are initialized to -1.
- **Clear to Empty / Checkerboard:** edit entire maps in one undoable action.
- **Canvas Zoom:** 10 to 40 px per tile, with clipped drawing for large maps.

The map editing limit is **256×256 tiles** (65,536 cells) to prevent
unbounded ImGui painting or fill operations. Runtime collision lookups still
support older imported maps larger than that editing limit. The current
canvas uses distinct colors and tile-number labels instead of true
texture-atlas thumbnails. Texture-accurate brush previews, layers per cell,
animated tiles and map chunk streaming remain future milestones.

The Inspector retains atlas layout, tile dimensions, tint, collision
enabled/solid/trigger, filtering and texture selection. The old 8×16
numeric mini-grid was removed because it was unsafe for resizing and
could not edit larger maps.

## Sprite Animation Timeline

Select an entity with a Sprite Animation component. The new **Sprite
Animation Timeline** panel exposes Play/Pause Preview, Restart, Step Back,
Step Forward, frame scrubbing, duration-per-frame and looping. A
horizontal frame strip shows the selected atlas frame. While in Edit Mode,
scrubbing updates the Sprite component's UV bounds immediately. During Play
Mode, the panel becomes read-only and the running animation is shown.

Animation tick/UV calculation is now shared by the editor and runtime through
AnimationTools. Normalization clamps invalid sheet dimensions, frame ranges
and nonfinite durations. Frame updates are bounded even for large dt.

The initial timeline is frame-based, **not yet** a keyframe/curve editor with
animation state machines, blend trees, clips or skeletal animation.

## Faster Tilemap Collision Queries

The existing AABB/collision solver remains in place. It is not replaced by
Box2D. Previously, each query could scan every tile on each map. Now
PhysicsSystem uses TilemapTools::overlappingCells to compute the small range
of cells that could intersect a collider, then runs the existing precise
per-tile overlap and collision/trigger logic for those candidates.

For example, a 128×128 map and a character occupying one tile should test
only a handful of nearby cells, rather than scanning all 16,384. This is
a broad-phase candidate optimization, **not a claim of continuous physics
or Box2D-level behavior**. The existing scene JSON collision flags, layer
and mask filtering, and collision events remain unchanged.

## Persistence, Undo, Lua and Player

- Tile painter actions work on the same TilemapComponent data that already
  serializes to JSON and loads in the standalone player.
- Brush drag uses one undo checkpoint; flood fill, resize, clear and
  checkerboard each use a single checkpoint.
- Painting is disabled while Play/Pause simulation is active, preserving
  authored-scene isolation.
- Lua gameplay scripts can now call `engine.tile_at(column,row)`
  and `engine.set_tile(column,row,atlasIndex)` to read or paint their own
  entity's Tilemap. Temporary runtime modifications roll back on Stop.
- Existing Lua movement/parameter scripts remain compatible.

## Test coverage

\`\`\`sh
cmake --preset ucrt64-debug
cmake --build --preset ucrt64-debug
ctest --preset ucrt64-debug
\`\`\`

The \`tilemap_animation_tests\` executable links **only** \`se_runtime\`.
It covers brush value validation, erase, line, flood fill barriers, resize,
checkerboard, world-to-cell mappings, collision cell ranges, indexed collision
queries on a 128×128 map, animation advance/UV/step/loop behavior, authoring
undo and scene serialization roundtrips.

The PR validation workflow builds this test and the full Windows engine.
Before merging, the native build and CTest must pass, and the visual painter,
animation timeline and game player should be smoke-tested.

## Future major work

- Texture-accurate atlas thumbnail palette and direct paint-on-scene viewport.
- True tile layers, brush stamps, autotiling/rule tiles and tile animation.
- Advanced sprite animation clip assets and animation state machines.
- Optional Box2D backend integration with a clear migration and compatibility
  plan, including joint solvers, sensors and stable Lua collision events.
