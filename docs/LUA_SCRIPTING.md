# Lua 5.4 gameplay scripting

SimpleEngine now loads game-script files from each project's
\`Assets/Scripts/*.lua\` folder. Script files run **without C++ recompilation** in
both \`SimpleEngineEditor\` and the editor-free \`SimpleEnginePlayer\`.

## Create and use a script

1. Create a game through the visual project launcher, or with:
   \`simpleengine_project create <parent> <name>\`.
2. The project includes \`Assets/Scripts/WelcomeController.lua\` and a
   \`Main.scene.json\` entity with \`scriptName: "WelcomeController"\`.
3. Press **Play** in the editor to observe the Welcome text animate.
4. In the Asset Browser use **Create Lua Script**. Select an entity in the
   Hierarchy first to attach it automatically, or add a Script component in
   Inspector and select the script from **Registered Script**.
5. Edit \`.lua\` in any text editor, press **Stop**, then **Reload Scripts** and
   **Play** again. Scripts are compiled on reload; a syntax error skips only
   the affected file, and a diagnostic is printed to stderr.
6. Save the scene before running:
   \`SimpleEnginePlayer --project "<project-folder>"\`.

Supported lifecycle functions are optional. Use global function names:
\`on_create()\`, \`on_start()\`, \`on_update(dt)\`,
\`on_fixed_update(dt)\`, \`on_event(typeId)\`, \`on_destroy()\`.

Lua state is **independent per entity**, so local/global variables declared by
one instance do not leak to another. Script state is released on destroy, scene
reset and engine cleanup. Existing registered native C++ ScriptSystem callbacks
continue to work. Script file name (without \`.lua\`) must match
\`ScriptComponent.scriptName\`; only immediate \`.lua\` files are discovered
within the Scripts folder, not nested folders.

## Example

\`\`\`lua
local seconds = 0

function on_start()
    engine.log("Player ready: " .. engine.entity_guid())
end

function on_update(dt)
    seconds = seconds + dt
    local speed = engine.get_float("speed", 2.0)
    if engine.input_down("MoveLeft") then
        engine.translate(-speed * dt, 0)
    end
    if engine.input_down("MoveRight") then
        engine.translate(speed * dt, 0)
    end
end
\`\`\`

The Inspector stores numeric custom parameters on the Script component in the
scene JSON; use \`engine.set_float(name, value)\` to change a parameter during
simulation. Runtime changes are discarded after editor Stop.

### Lua engine API

| Function | Meaning |
|---|---|
| \`engine.position()\` | Returns current entity world x, y; returns nil if no Transform |
| \`engine.set_position(x, y)\` | Sets current entity Transform |
| \`engine.translate(dx, dy)\` | Adds to current entity Transform |
| \`engine.input_down("MoveLeft")\` | Reads action state (boolean) |
| \`engine.get_float("speed", 2.0)\` | Numeric Script component parameter or fallback |
| \`engine.set_float("speed", 3.0)\` | Updates that component parameter |
| \`engine.entity_guid()\` | Persistent GUID of current entity |
| \`engine.tile_at(col, row)\` | Reads tile index on this entity's Tilemap, or -1 for absent/empty |
| \`engine.set_tile(col, row, value)\` | Paints owning entity's Tilemap; returns true when a cell changes |
| \`engine.log("message")\` | Writes a diagnostic to stderr |

For projects with no native \`IGame\`, these mappings are installed by default:
MoveLeft = A/Left Arrow, MoveRight = D/Right Arrow, MoveUp = W/Up Arrow,
MoveDown = S/Down Arrow. Native C++ game subclasses continue configuring
their own mappings.

## CLI validation and CTest

\`\`\`powershell
.\simpleengine_project.exe validate "C:\Games\MyGame"
\`\`\`

This validates the project manifest, loads the startup scene headlessly,
compiles the project's Lua script files without executing them, and checks
that active Script components reference registered Lua scripts.

The \`lua_gameplay_tests\` regression target exercises per-entity script state,
event lifecycle, movement, input, restricted standard library access,
invalid source reporting, and running the Lua controller generated for a
brand-new project. It requires no graphics window and links only \`se_runtime\`.

## Building and distribution

A Lua 5.4 development library is now required at CMake configure time.
Windows builds use the MSYS2 package
\`mingw-w64-ucrt-x86_64-lua54\` (not the default Lua 5.5 package).
CMake uses \`find_package(Lua 5.4 EXACT REQUIRED)\`.

The existing packaging workflow recursively collects the Lua 5.4 runtime
DLL with other UCRT64 dependencies. Windows ZIPs now include the Lua MIT
license in \`licenses/Lua.txt\`. New CTest coverage runs on Windows PR builds.

## Trust and current limitations

- This is an embedded Lua gameplay API, **not a strong security sandbox**.
  \`io\`, \`os\`, \`package\`, \`debug\`, and dangerous file-loading helpers are
  not exposed by default, but executing an untrusted game project can still
  be unsafe. Review projects/scripts before pressing Play.
- Lua syntax validation compiles without executing the script; runtime
  callbacks can still fail. Lua errors disable the failing Script component
  to prevent repeated frame-by-frame failures. A 200,000-instruction budget
  per callback interrupts accidental infinite Lua loops. The instruction
  budget is a reliability safeguard, not secure execution isolation.
- No Lua hot reload while a scene is running; Stop and press Reload Scripts.
- Only **one ScriptComponent** per entity is supported by the current ECS.
- Current Lua public API cannot spawn/destroy entities, handle typed events,
  use audio, or load prefabs; those API expansions need controlled handles,
  versioning, and comprehensive tests.
- This milestone does not include Windows/Linux/macOS game exporters or a
  general-purpose plugin SDK.
