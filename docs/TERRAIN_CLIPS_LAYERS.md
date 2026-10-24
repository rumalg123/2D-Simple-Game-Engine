# Phase 8: Connected Terrain, Tilemap Layers, Named Animation Clips

This phase adds reusable 2D game-authoring features to SimpleEngine. The
existing project format, scene entity GUIDs, direct tile painting, sprite
animation parameters, Lua scripts and standalone player remain supported.

## Connected terrain autotiling

SimpleEngine now provides an opt-in 4-direction, 16-variant terrain brush.
A connected cell's atlas index is calculated as:

`terrainBaseTile + mask` with **N = 1, E = 2, S = 4, W = 8**.

For example, a row of three tiles has mask values `2, 10, 8`. Painting
or erasing recomputes only the edited cell and its four neighbors.

### Workflow

1. Select a Tilemap entity in Scene Hierarchy.
2. Open **Tilemap Painter** and choose **Prepare 4x4 Atlas** if the map
   does not already have 16 consecutive tile variants.
3. Set **Terrain Atlas Base** to the index of the first of 16 variants.
4. Click **Enable Terrain**; existing nonempty tiles become terrain occupancy.
5. Choose **Terrain Paint** or **Terrain Erase** and drag to connect/disconnect
   terrain automatically. All affected atlas indices update.
6. Click **Bake Terrain to Static Tiles** if you want to edit the generated
   tile indices with ordinary Pencil, Flood Fill, Line or Eraser afterward.

The brush saves its occupancy channel separately as `terrainCells` so
connections survive scene reloads and map resize. Opt-in terrain JSON fields
are `terrainEnabled`, `terrainBaseTile` and `terrainCells`.

The underlying editing bounds remain 256×256 cells. Terrain is
**one material per tilemap entity**, and uses a fixed 16-atlas-tile ordering.
This is not yet Godot-like multi-terrain sets, 3×3 corner logic or authored
rule-tables, which require a distinct future tile-set resource system.

## Multiple tilemap layers

Each Tilemap entity is a separate editable layer with its own scene GUID,
draw-layer number, atlas texture, collision mask, collision toggle and tile
data. In Scene Hierarchy:

- Choose **Add Tile Layer** to create a new Tilemap entity. If an existing
  tilemap is selected, its settings and dimensions are copied, but the new
  layer starts empty and is placed one draw-layer above.
- Use **Layer Up / Layer Down** to change the selected map's draw order.
- Select a layer to paint it independently in Tilemap Painter.
- Save the scene normally. Each map remains a separate entity in JSON and
  is loaded by the standalone player without ImGui.

This uses the existing multi-entity Scene and TilemapComponent rendering,
rather than replacing the renderer or introducing a new nested layer format.

## Named sprite-sheet animation clips

Previously a sprite animation component could use only one numeric frame
range. It now supports up to 64 named clips, each storing:

```json
{
  "name": "Run",
  "firstFrame": 4,
  "frameCount": 6,
  "secondsPerFrame": 0.08,
  "loop": true
}
```

The **Sprite Animation Timeline** can add, select, edit and delete these
clips. Selecting a clip restarts its playback and updates the active
frame range, duration and looping. The clip list and active clip name are
saved in `spriteAnimation.clips` and `spriteAnimation.activeClip`.
Existing scenes without clips load and run with the old animation fields.

Clips live in the component, and can be copied with entities/prefabs.
Reusable `.anim` assets, blend trees, graph transitions and skeletal
animation are future work.

## Lua animation API

Scripts attached to an entity with a Sprite Animation component can call:

| Function | Behavior |
|---|---|
| `engine.play_animation("Run")` | Switch to named clip and restart; returns true/false |
| `engine.current_animation()` | Active clip name, or nil when no active clip |
| `engine.stop_animation()` | Pause playback on this entity |
| `engine.animation_finished()` | True at the end of a non-looping clip |

For example:

```lua
function on_start()
    engine.play_animation("Idle")
end

function on_update(dt)
    if engine.input_down("MoveRight") then
        if engine.current_animation() ~= "Run" then
            engine.play_animation("Run")
        end
        engine.translate(2.0 * dt, 0)
    elseif engine.current_animation() ~= "Idle" then
        engine.play_animation("Idle")
    end
end
```

For now, scripts explicitly choose clips. No transition graph or clip
crossfading is implied.

## Validation and compatibility

This milestone adds `terrain_clip_tests` for neighbor correctness, terrain
erase and resize, invalid variant ranges, baking, multi-layer GUID and draw
order persistence, named clip lifecycle, serialized scene roundtrip, and
legacy JSON compatibility. `lua_gameplay_tests` also tests clip selection
and non-looping completion through the Lua API.

Windows PR CI still compiles the full editor, editor-free player, samples,
smoke tests and the complete CTest suite. A manual editor test is needed to
verify mouse strokes, named clip selection, saved scene reopening and
layer ordering.

The external design references are Godot 4 TileMapLayer terrain connect and
AnimationTree/AnimationPlayer separation; these are architectural references,
not a claim of feature parity.
