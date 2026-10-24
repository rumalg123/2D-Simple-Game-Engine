#include "LuaScriptRuntime.h"

#include "InputMap.h"
#include "ScriptSystem.h"
#include "AnimationTools.h"
#include "TilemapTools.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <memory>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {
constexpr std::uintmax_t MaxScriptBytes = 256 * 1024;

struct Execution {
    ScriptContext context{};
    Entity entity = InvalidEntity;
    ScriptComponent* script = nullptr;
    unsigned instructionTicks = 0;
};

Execution* execution(lua_State* L) {
    return static_cast<Execution*>(lua_touserdata(L, lua_upvalueindex(1)));
}

void budgetHook(lua_State* L, lua_Debug*) {
    auto* state = *static_cast<Execution**>(lua_getextraspace(L));
    // Hook fires every 10,000 Lua instructions. Limit callbacks to 200,000.
    if (state && ++state->instructionTicks > 20)
        luaL_error(L, "SimpleEngine: Lua callback instruction budget exceeded");
}

int position(lua_State* L) {
    const Execution* e = execution(L);
    const TransformComponent* t = e && e->context.scene
        ? e->context.scene->getTransform(e->entity) : nullptr;
    if (!t) { lua_pushnil(L); return 1; }
    lua_pushnumber(L, t->x);
    lua_pushnumber(L, t->y);
    return 2;
}

int setPosition(lua_State* L) {
    Execution* e = execution(L);
    const float x = static_cast<float>(luaL_checknumber(L, 1));
    const float y = static_cast<float>(luaL_checknumber(L, 2));
    if (e && e->context.scene && e->context.scene->isValidEntity(e->entity)) {
        e->context.scene->setTransform(e->entity, {x, y});
    }
    return 0;
}

int translate(lua_State* L) {
    Execution* e = execution(L);
    const float dx = static_cast<float>(luaL_checknumber(L, 1));
    const float dy = static_cast<float>(luaL_checknumber(L, 2));
    if (e && e->context.scene) {
        if (TransformComponent* t = e->context.scene->getTransform(e->entity)) {
            t->x += dx;
            t->y += dy;
        }
    }
    return 0;
}

int inputDown(lua_State* L) {
    const Execution* e = execution(L);
    const char* action = luaL_checkstring(L, 1);
    const bool down = e && e->context.inputMap &&
                      e->context.inputMap->isDown(action);
    lua_pushboolean(L, down);
    return 1;
}

int getFloat(lua_State* L) {
    const Execution* e = execution(L);
    const char* name = luaL_checkstring(L, 1);
    const float fallback = static_cast<float>(luaL_optnumber(L, 2, 0.0));
    const float value = e && e->script
        ? ScriptSystem::getFloatParameter(*e->script, name, fallback) : fallback;
    lua_pushnumber(L, value);
    return 1;
}

int setFloat(lua_State* L) {
    Execution* e = execution(L);
    const char* name = luaL_checkstring(L, 1);
    const float value = static_cast<float>(luaL_checknumber(L, 2));
    if (e && e->script) e->script->parameters[name] = value;
    return 0;
}

// The API deliberately operates on this scripted entity's own tilemap.
// Scripts cannot mutate arbitrary other scenes or objects by raw numeric ID.
bool luaCell(lua_State* L, int& col, int& row) {
    const lua_Integer x=luaL_checkinteger(L,1), y=luaL_checkinteger(L,2);
    if(x<std::numeric_limits<int>::min() || x>std::numeric_limits<int>::max() ||
       y<std::numeric_limits<int>::min() || y>std::numeric_limits<int>::max())
        return false;
    col=static_cast<int>(x);row=static_cast<int>(y);
    return true;
}

int tileAt(lua_State* L) {
    int column=0,row=0;
    if(!luaCell(L,column,row)) {lua_pushinteger(L,-1);return 1;}
    const Execution* e=execution(L);
    const TilemapComponent* map=e && e->context.scene
        ? e->context.scene->getTilemap(e->entity) : nullptr;
    lua_pushinteger(L,map ? TilemapTools::get(*map,column,row) : -1);
    return 1;
}

int setTile(lua_State* L) {
    int column=0,row=0;
    if(!luaCell(L,column,row)) {lua_pushboolean(L,false);return 1;}
    const lua_Integer tile=luaL_checkinteger(L,3);
    const Execution* e=execution(L);
    TilemapComponent* map=e && e->context.scene
        ? e->context.scene->getTilemap(e->entity) : nullptr;
    const bool changed=map && tile>=TilemapTools::EmptyTile &&
        tile<=std::numeric_limits<int>::max() &&
        TilemapTools::paint(*map,column,row,static_cast<int>(tile));
    lua_pushboolean(L,changed);
    return 1;
}

int playAnimation(lua_State* L) {
    const Execution* e=execution(L);
    const char* name=luaL_checkstring(L,1);
    SpriteAnimationComponent* animation=e && e->context.scene
        ? e->context.scene->getSpriteAnimation(e->entity) : nullptr;
    const bool ok=animation &&
        AnimationTools::selectClip(*animation,name);
    lua_pushboolean(L,ok);
    return 1;
}
int currentAnimation(lua_State* L) {
    const Execution* e=execution(L);
    const SpriteAnimationComponent* animation=e && e->context.scene
        ? e->context.scene->getSpriteAnimation(e->entity) : nullptr;
    if(!animation || animation->activeClip.empty()) {
        lua_pushnil(L);return 1;
    }
    lua_pushlstring(L,animation->activeClip.c_str(),animation->activeClip.size());
    return 1;
}
int stopAnimation(lua_State* L) {
    const Execution* e=execution(L);
    SpriteAnimationComponent* animation=e && e->context.scene
        ? e->context.scene->getSpriteAnimation(e->entity) : nullptr;
    if(animation) animation->playing=false;
    return 0;
}
int animationFinished(lua_State* L) {
    const Execution* e=execution(L);
    const SpriteAnimationComponent* animation=e && e->context.scene
        ? e->context.scene->getSpriteAnimation(e->entity) : nullptr;
    lua_pushboolean(L,animation && !animation->loop && !animation->playing &&
                        animation->currentFrame==animation->frameCount-1);
    return 1;
}

int entityGuid(lua_State* L) {
    const Execution* e = execution(L);
    const std::string guid = e && e->context.scene
        ? e->context.scene->guid(e->entity) : "";
    lua_pushlstring(L, guid.data(), guid.size());
    return 1;
}

int logMessage(lua_State* L) {
    const Execution* e = execution(L);
    const char* msg = luaL_checkstring(L, 1);
    const std::string id = e && e->context.scene
        ? e->context.scene->guid(e->entity) : "";
    std::fprintf(stderr, "[Lua entity %s] %s\n", id.c_str(), msg);
    return 0;
}

void expose(lua_State* L, const char* name, lua_CFunction fn, Execution* e) {
    lua_pushlightuserdata(L, e);
    lua_pushcclosure(L, fn, 1);
    lua_setfield(L, -2, name);
}

void setupState(lua_State* L, Execution* e) {
    *static_cast<Execution**>(lua_getextraspace(L)) = e;
    lua_sethook(L, budgetHook, LUA_MASKCOUNT, 10000);

    // Only deterministic game-script essentials. Lua io/os/package/debug
    // libraries are not exposed. This is not a strong security sandbox.
    luaL_requiref(L, "_G", luaopen_base, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "table", luaopen_table, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "string", luaopen_string, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "math", luaopen_math, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "utf8", luaopen_utf8, 1);
    lua_pop(L, 1);
    for (const char* name : {"dofile", "loadfile", "load", "collectgarbage"}) {
        lua_pushnil(L);
        lua_setglobal(L, name);
    }

    lua_newtable(L);
    expose(L, "position", position, e);
    expose(L, "set_position", setPosition, e);
    expose(L, "translate", translate, e);
    expose(L, "input_down", inputDown, e);
    expose(L, "get_float", getFloat, e);
    expose(L, "set_float", setFloat, e);
    expose(L, "tile_at", tileAt, e);
    expose(L, "set_tile", setTile, e);
    expose(L, "play_animation", playAnimation, e);
    expose(L, "current_animation", currentAnimation, e);
    expose(L, "stop_animation", stopAnimation, e);
    expose(L, "animation_finished", animationFinished, e);
    expose(L, "entity_guid", entityGuid, e);
    expose(L, "log", logMessage, e);
    lua_setglobal(L, "engine");
}

void reportError(lua_State* L, const std::string& file, const char* action) {
    const char* msg = lua_tostring(L, -1);
    std::fprintf(stderr, "[Lua] %s (%s): %s\n", file.c_str(), action,
                 msg ? msg : "unknown Lua error");
    lua_pop(L, 1);
}

std::vector<std::filesystem::path> listScripts(
    const std::filesystem::path& root, std::vector<std::string>& errors) {
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) return files;
    if (ec || !std::filesystem::is_directory(root, ec)) {
        errors.push_back("Scripts path is not a directory: " + root.string());
        return files;
    }
    for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
        if (ec) break;
        if (entry.path().extension() != ".lua") continue;
        if (entry.is_symlink(ec) || !entry.is_regular_file(ec)) {
            errors.push_back("Refusing non-regular or symlinked Lua file: " +
                             entry.path().string());
            continue;
        }
        const auto size = entry.file_size(ec);
        if (ec || size > MaxScriptBytes) {
            errors.push_back("Lua script exceeds 256 KiB or cannot be read: " +
                             entry.path().string());
            continue;
        }
        files.push_back(entry.path());
    }
    if (ec) errors.push_back("Failed to scan scripts: " + ec.message());
    std::sort(files.begin(), files.end());
    return files;
}
}

struct LuaScriptRuntime::Instance {
    lua_State* L = nullptr;
    Execution execution;
    std::string path;
    ~Instance() { if (L) lua_close(L); }
};

LuaScriptRuntime::LuaScriptRuntime() = default;
LuaScriptRuntime::~LuaScriptRuntime() { clear(); }

void LuaScriptRuntime::clear() { instances.clear(); }

bool LuaScriptRuntime::validateFile(const std::filesystem::path& path,
                                    std::string& error) {
    lua_State* L = luaL_newstate();
    if (!L) { error = "Unable to allocate Lua validation state."; return false; }
    const int code = luaL_loadfilex(L, path.string().c_str(), "t");
    if (code != LUA_OK) {
        const char* message = lua_tostring(L, -1);
        error = path.string() + ": " + (message ? message : "script compile failed");
    } else {
        error.clear();
    }
    lua_close(L);
    return code == LUA_OK;
}

std::size_t LuaScriptRuntime::validateDirectory(
    const std::filesystem::path& root, std::vector<std::string>& errors) {
    std::size_t valid = 0;
    for (const auto& file : listScripts(root, errors)) {
        std::string error;
        if (validateFile(file, error)) ++valid;
        else errors.push_back(error);
    }
    return valid;
}

std::size_t LuaScriptRuntime::registerDirectory(
    ScriptSystem& scripts, const std::filesystem::path& root,
    std::vector<std::string>& errors) {
    std::size_t registered = 0;
    for (const auto& file : listScripts(root, errors)) {
        std::string error;
        if (!validateFile(file, error)) {
            errors.push_back(error);
            continue;
        }
        const std::string name = file.stem().string();
        if (name.empty() || scripts.hasScript(name)) {
            errors.push_back("Lua name conflicts with an existing script: " + name);
            continue;
        }

        ScriptSystem::ScriptLifecycle hooks;
        hooks.onCreate = [this, file](ScriptContext& context, Entity entity,
                                       ScriptComponent& script) {
            createInstance(context, entity, script, file);
        };
        hooks.onStart = [this](ScriptContext& context, Entity entity,
                               ScriptComponent& script) {
            invoke(context, entity, script, "on_start");
        };
        hooks.onUpdate = [this](ScriptContext& context, Entity entity,
                                ScriptComponent& script, float dt) {
            invoke(context, entity, script, "on_update", dt, true);
        };
        hooks.onFixedUpdate = [this](ScriptContext& context, Entity entity,
                                     ScriptComponent& script, float dt) {
            invoke(context, entity, script, "on_fixed_update", dt, true);
        };
        hooks.onDestroy = [this](ScriptContext& context, Entity entity,
                                 ScriptComponent& script) {
            removeInstance(context, entity, script);
        };
        hooks.onEvent = [this](ScriptContext& context, Entity entity,
                               ScriptComponent& script, const Event& event) {
            // Event types are numeric until a stable public Lua event ABI exists.
            invoke(context, entity, script, "on_event",
                   static_cast<float>(event.type), true);
        };
        scripts.registerScript(name, std::move(hooks));
        ++registered;
    }
    return registered;
}

void LuaScriptRuntime::createInstance(
    ScriptContext& context, Entity entity, ScriptComponent& script,
    const std::filesystem::path& file) {
    instances.erase(entity);
    auto instance = std::make_unique<Instance>();
    instance->L = luaL_newstate();
    instance->path = file.string();
    if (!instance->L) {
        std::fprintf(stderr, "[Lua] Cannot allocate state for %s\n",
                     instance->path.c_str());
        return;
    }
    instance->execution = {context, entity, &script};
    setupState(instance->L, &instance->execution);
    lua_State* L = instance->L;
    if (luaL_loadfilex(L, file.string().c_str(), "t") != LUA_OK) {
        reportError(L, instance->path, "load");
        script.active = false;
        return;
    }
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        reportError(L, instance->path, "initialize");
        script.active = false;
        return;
    }
    instances[entity] = std::move(instance);
    invoke(context, entity, script, "on_create");
}

void LuaScriptRuntime::invoke(
    ScriptContext& context, Entity entity, ScriptComponent& script,
    const char* function, float dt, bool withDelta) {
    const auto found = instances.find(entity);
    if (found == instances.end()) return;
    Instance& instance = *found->second;
    instance.execution = {context, entity, &script};
    instance.execution.instructionTicks = 0;
    lua_State* L = instance.L;
    lua_getglobal(L, function);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        std::fprintf(stderr, "[Lua] %s: %s is not a function\n",
                     instance.path.c_str(), function);
        return;
    }
    if (withDelta) lua_pushnumber(L, dt);
    if (lua_pcall(L, withDelta ? 1 : 0, 0, 0) != LUA_OK) {
        reportError(L, instance.path, function);
        script.active = false; // Do not repeat errors every frame.
    }
}

void LuaScriptRuntime::removeInstance(
    ScriptContext& context, Entity entity, ScriptComponent& script) {
    invoke(context, entity, script, "on_destroy");
    instances.erase(entity);
}
