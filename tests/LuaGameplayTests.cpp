#include "InputMap.h"
#include "LuaScriptRuntime.h"
#include "ProjectManager.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "SceneSerializer.h"
#include "ScriptSystem.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool ok, const std::string& why) {
    if (!ok) throw std::runtime_error(why);
}
struct TemporaryDirectory {
    std::filesystem::path root;
    TemporaryDirectory() {
        root = std::filesystem::temp_directory_path() /
            ("simpleengine_lua_" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root);
    }
    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }
};
void write(const std::filesystem::path& file, const char* text) {
    std::ofstream out(file);
    require(bool(out), "Could not create Lua fixture");
    out << text;
    require(bool(out), "Could not write Lua fixture");
}
void near(float actual, float expected, const std::string& reason) {
    const float difference = actual > expected ? actual - expected : expected - actual;
    require(difference < 0.0001f, reason);
}
}

int main() {
    try {
        TemporaryDirectory temp;
        const auto scriptsPath = temp.root / "Scripts";
        std::filesystem::create_directories(scriptsPath);
        write(scriptsPath / "Mover.lua", R"LUA(
local updates = 0
function on_create()
    engine.set_float("created", 1)
    engine.set_float("no_os", os == nil and 1 or 0)
    engine.set_float("no_io", io == nil and 1 or 0)
end
function on_start()
    engine.set_float("started", 1)
end
function on_update(dt)
    updates = updates + 1
    engine.set_float("updates", updates)
    engine.translate(dt, 0)
    if engine.input_down("MoveRight") then
        engine.translate(1, 0)
    end
end
function on_fixed_update(dt)
    engine.translate(0, dt)
end
function on_destroy()
    engine.log("destroyed")
end
)LUA");
        write(scriptsPath / "Bad.lua", "function on_update(dt) this is not lua end \n");

        std::vector<std::string> errors;
        require(LuaScriptRuntime::validateDirectory(scriptsPath, errors) == 1,
                "Validator did not accept only the valid script");
        require(errors.size() == 1, "Invalid Lua script did not report an error");
        errors.clear();

        LuaScriptRuntime lua;
        ScriptSystem scripts;
        require(lua.registerDirectory(scripts, scriptsPath, errors) == 1,
                "Lua registration should skip invalid scripts");
        require(scripts.hasScript("Mover") && !scripts.hasScript("Bad"),
                "Registered script names do not match valid assets");
        require(errors.size() == 1, "Invalid script registration was not reported");

        Scene scene;
        const Entity alice = scene.createEntity("Alice", {0, 0});
        const Entity bob = scene.createEntity("Bob", {10, 0});
        scene.setScript(alice, {"Mover", true, 0});
        scene.setScript(bob, {"Mover", true, 0});
        InputMap input;
        input.bindAction("MoveRight", 68);
        scripts.update(scene, 0.25f, nullptr, &input);
        require(lua.activeInstanceCount() == 2, "Per-entity Lua states not created");
        near(scene.getTransform(alice)->x, 0.25f, "Alice did not move");
        near(scene.getTransform(bob)->x, 10.25f, "Bob did not move");
        near(scene.getScript(alice)->parameters.at("created"), 1, "on_create missing");
        near(scene.getScript(bob)->parameters.at("started"), 1, "on_start missing");
        near(scene.getScript(alice)->parameters.at("no_os"), 1, "os exposed in script");
        near(scene.getScript(alice)->parameters.at("no_io"), 1, "io exposed in script");

        input.handleKeyChanged(68, true);
        scripts.update(scene, 0.25f, nullptr, &input);
        near(scene.getTransform(alice)->x, 1.50f, "Input action did not move entity");
        near(scene.getTransform(bob)->x, 11.50f, "Input action did not move second entity");
        near(scene.getScript(alice)->parameters.at("updates"), 2, "Alice state changed incorrectly");
        near(scene.getScript(bob)->parameters.at("updates"), 2, "Bob state changed incorrectly");
        scripts.fixedUpdate(scene, 0.02f, nullptr, &input);
        near(scene.getTransform(alice)->y, 0.02f, "Lua fixed update did not run");

        scripts.destroyAll(scene, nullptr, &input);
        require(lua.activeInstanceCount() == 0, "Destroyed scripts leaked Lua states");
        scripts.clear();

        // Runtime errors and infinite loops must not hang the game or spam logs.
        write(scriptsPath / "Throw.lua",
              "function on_update(dt) error('deliberate test error') end\\n");
        write(scriptsPath / "Infinite.lua",
              "function on_update(dt) while true do end end\\n");
        LuaScriptRuntime failureLua;
        ScriptSystem failureScripts;
        errors.clear();
        require(failureLua.registerDirectory(failureScripts, scriptsPath, errors) == 3,
                "Valid scripts should still register alongside invalid scripts");
        Scene failureScene;
        const Entity thrower = failureScene.createEntity("Thrower");
        const Entity infinite = failureScene.createEntity("Infinite");
        failureScene.setScript(thrower, {"Throw", true, 0});
        failureScene.setScript(infinite, {"Infinite", true, 0});
        failureScripts.update(failureScene, 0.016f);
        require(!failureScene.getScript(thrower)->active &&
                !failureScene.getScript(infinite)->active,
                "Failing or looping Lua scripts were not disabled");
        failureScripts.destroyAll(failureScene);
        failureScripts.clear();

        // Real created projects must already be valid and scriptable end-to-end.
        ProjectInfo project;
        std::string error;
        require(ProjectManager::create(temp.root, "Lua Test Game", project, error),
                "Could not create starter project: " + error);
        const auto starterFile =
            std::filesystem::path(project.config.assetRoot) / "Scripts/WelcomeController.lua";
        require(std::filesystem::is_regular_file(starterFile),
                "New projects do not include a Lua starter script");
        Scene starterScene;
        ResourceManager resources;
        require(loadSceneFromJson(starterScene, resources, project.config.startupScene, error),
                "Created scene cannot be read: " + error);
        const Entity welcome = starterScene.findEntityByName("Welcome");
        require(welcome != InvalidEntity && starterScene.getScript(welcome) &&
                starterScene.getScript(welcome)->scriptName == "WelcomeController",
                "Starter scene is not wired to its script");

        LuaScriptRuntime projectLua;
        ScriptSystem projectScripts;
        errors.clear();
        require(projectLua.registerDirectory(projectScripts,
                    std::filesystem::path(project.config.assetRoot) / "Scripts", errors) == 1,
                "Starter Lua script did not register");
        require(errors.empty(), "Starter Lua script has invalid syntax");
        const float xBefore = starterScene.getTransform(welcome)->x;
        projectScripts.update(starterScene, 0.5f);
        require(starterScene.getTransform(welcome)->x != xBefore,
                "Starter Lua gameplay did not change entity position");
        projectScripts.destroyAll(starterScene);
        projectScripts.clear();

        std::cout << "Lua syntax validation, script isolation, entity movement, input, "
                     "lifecycle and playable project template passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Lua gameplay test failed: " << e.what() << '\n';
        return 1;
    }
}
