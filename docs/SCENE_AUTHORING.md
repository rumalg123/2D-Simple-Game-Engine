# Scene authoring: stable identity and non-destructive Play Mode

This phase moves the 2D editor closer to a safe authoring workflow:
persistent entity IDs, Play/Stop isolation, bounded undo/redo checkpoints and
safer writes. These features are not a general scripting or prefab-override
system yet.

## Play, Stop, Pause, Step

- **Edit:** Scenes and components are editable. Only this state should be saved.
- **Play:** Copies the current authored scene (including all ECS component
  pools and stable entity GUIDs). The runtime copy receives simulation updates,
  scripts and physics.
- **Pause/Resume:** Freezes or continues the temporary runtime world.
- **Step:** Advances exactly one fixed simulation step while paused.
- **Stop:** Destroys runtime script instances, clears physics contacts and
  restores the authored scene snapshot; all runtime-only spawning, deletion,
  transform changes and component edits are discarded.
- **Reset Scene while playing:** Ends the simulation and starts again from
  the authored scene. **Reset Scene in edit mode** reloads the initial scene.
- **Save Scene JSON:** Rejected during Play/Pause. Stop first to avoid
  accidentally writing simulation changes into authored project files.

The compiled standalone player runs the scene normally, without an
editor snapshot or Dear ImGui. Legacy configurations with
`startPlaying=true` still start the editor in Play Mode, but can now Stop
to return to their initial authored state.

**Current limit:** ECS scene data is isolated; ResourceManager textures,
other external mutable services, plugin internal state, runtime audio and
external files are not deep-cloned. Scripts/plugins that modify external
services can produce side effects beyond scene rollback. A fully isolated
runtime world and cross-scene Play Mode will need additional work.

## Stable entity GUIDs

Every created entity receives a random UUIDv4-format GUID independent of its
temporary numeric `Entity` handle. Scene serialization writes both fields:

```json
{"id":0,"guid":"79fa90e1-879c-49d5-93e8-510d2f39a4bd","name":{"name":"Player"}}
```

- `scene.guid(entity)`: persistent string identity.
- `scene.findEntityByGuid(id)`: resolve stable identity in this scene.
- `scene.setEntityGuid(entity, id)`: restore a GUID during deserialization,
  rejecting malformed/duplicate strings.
- Copying a scene preserves its GUIDs; creating or duplicating an entity
  mints a new GUID; deleting an entity removes its GUID mapping.
- Existing scene files without GUIDs remain loadable. New GUIDs are
  assigned at load time and become persistent the next time the scene is saved.
- A file containing invalid/duplicate GUIDs or repeated numeric IDs fails
  to load without replacing the user's currently open scene.
- The Inspector displays the selected entity GUID and offers **Copy GUID**.

Entity GUIDs are the foundation for prefabs, hierarchies, scene references
and versioned schemas. A UUID is not a runtime handle, and is not currently
an asset database identifier.

## Undo/Redo checkpoints

The editor maintains a bounded 32-snapshot undo/redo history, intended
for structural and explicit authoring operations.

- Create empty, duplicate, delete and new empty scene record a snapshot.
- Add/remove components, Set Player and Set Camera Target record a snapshot.
- The Inspector exposes **Undo checkpoint** for bulk field/property editing.
- Undo/Redo in the Engine panel reverses/reapplies recorded scenes.
- A new action clears the redo chain; scene reloading resets undo history.
- Play Mode modifications do not become undo checkpoints.

**Current limit:** continuous property drag edits are not yet automatically
grouped into one undo transaction. Record an Inspector checkpoint before
a long field edit, or use the structural action history. Future work should
introduce command-level property transactions, transform gizmo transactions,
and change coalescing.

## Safer scene saves

`saveSceneToJson` now serializes to a unique temporary file next to the
destination and renames the file over the original only if writing finishes
successfully. If writing or committing fails, it reports the error and
cleans the temporary file, preserving the old destination when possible.
This is not a replacement for an explicit crash-recovery/backups system.

## Tests and developer checks

```sh
cmake --preset ucrt64-debug
cmake --build --preset ucrt64-debug
ctest --preset ucrt64-debug
```

`scene_authoring_tests` covers unique/stable identities, copying and
deletion, duplicate/invalid ID validation, legacy scene migration,
JSON GUID roundtrip, failed import retaining the existing scene,
runtime copy rollback and bounded undo/redo including redo invalidation.

## Next milestones

1. Command-based undo/redo for Inspector field/property edits and gizmos.
2. Split `SceneDocument` and `RuntimeWorld` with independent non-scene
   runtime state (resources, scripts, plugin state, audio effects).
3. Persistent asset GUIDs and prefab/scene reference remapping.
4. Lua gameplay scripting and a cross-platform extension SDK.
