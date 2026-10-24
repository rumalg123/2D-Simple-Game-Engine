#pragma once

#include "ECS.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class ScriptSystem;
struct ScriptContext;
struct ScriptComponent;
struct Event;

// Lua game-script binding. A separate Lua 5.4 state is created per entity,
// preventing globals in one entity's script from leaking to another.
// C++ scripts in ScriptSystem continue to work without changes.
class LuaScriptRuntime {
public:
    LuaScriptRuntime();
    ~LuaScriptRuntime();
    LuaScriptRuntime(const LuaScriptRuntime&) = delete;
    LuaScriptRuntime& operator=(const LuaScriptRuntime&) = delete;

    // Registers valid top-level *.lua files (stem = ScriptComponent.scriptName).
    // Invalid files are reported and skipped without stopping other scripts.
    std::size_t registerDirectory(ScriptSystem& scripts,
                                  const std::filesystem::path& directory,
                                  std::vector<std::string>& errors);

    // Pure validation: compile without executing. Safe for headless CI.
    static bool validateFile(const std::filesystem::path& path, std::string& error);
    static std::size_t validateDirectory(const std::filesystem::path& directory,
                                         std::vector<std::string>& errors);

    void clear();
    std::size_t activeInstanceCount() const { return instances.size(); }

private:
    struct Instance;
    std::unordered_map<Entity, std::unique_ptr<Instance>> instances;

    void createInstance(ScriptContext& context, Entity entity,
                        ScriptComponent& script, const std::filesystem::path& file);
    void invoke(ScriptContext& context, Entity entity, ScriptComponent& script,
                const char* function, float deltaTime = 0.0f, bool withDelta = false);
    void removeInstance(ScriptContext& context, Entity entity,
                        ScriptComponent& script);
};
