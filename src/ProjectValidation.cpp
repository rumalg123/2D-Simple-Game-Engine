#include "ProjectValidation.h"

#include "LuaScriptRuntime.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "SceneSerializer.h"
#include "ScriptSystem.h"

#include <algorithm>
#include <filesystem>
#include <set>
#include <string>
#include <system_error>
#include <vector>

ProjectValidationResult validateProject(const ProjectInfo& project) {
    ProjectValidationResult result;
    const auto assetRoot = std::filesystem::path(project.config.assetRoot);
    const auto scriptsRoot = assetRoot / "Scripts";
    LuaScriptRuntime lua;
    ScriptSystem scripts;
    result.luaScripts = lua.registerDirectory(scripts, scriptsRoot, result.errors);

    std::set<std::filesystem::path> candidateScenes;
    if (!project.config.startupScene.empty())
        candidateScenes.insert(std::filesystem::path(project.config.startupScene)
                                   .lexically_normal());

    const auto sceneRoot = assetRoot / "Scenes";
    std::error_code ec;
    if (std::filesystem::is_directory(sceneRoot, ec)) {
        std::filesystem::recursive_directory_iterator iter(
            sceneRoot, std::filesystem::directory_options::none, ec);
        const std::filesystem::recursive_directory_iterator end;
        while (!ec && iter != end) {
            const auto path = iter->path();
            if (iter->is_symlink(ec)) {
                result.errors.push_back("Scene path is a symbolic link: " + path.string());
                iter.disable_recursion_pending();
            } else if (iter->is_regular_file(ec)) {
                const std::string name = path.filename().string();
                if (name.size() >= 11 &&
                    name.compare(name.size() - 11, 11, ".scene.json") == 0)
                    candidateScenes.insert(path.lexically_normal());
            }
            iter.increment(ec);
        }
        if (ec) result.errors.push_back("Cannot scan scene directory: " + ec.message());
    }
    if (candidateScenes.empty()) {
        result.errors.push_back("Project has no startup scene or saved scenes.");
        return result;
    }
    for (const auto& path : candidateScenes) {
        ResourceManager resources;
        Scene scene;
        std::string error;
        if (!loadSceneFromJson(scene, resources, path.string(), error)) {
            result.errors.push_back(error);
            continue;
        }
        ++result.scenes;
        result.entities += scene.livingEntityCount();
        for (std::size_t i = 0; i < scene.getScriptPool().size(); ++i) {
            const auto& script = scene.getScriptPool().componentAt(i);
            if (script.active && !script.scriptName.empty() &&
                !scripts.hasScript(script.scriptName)) {
                result.errors.push_back(
                    path.filename().string() + " references missing/invalid Lua script: " +
                    script.scriptName);
            }
        }
    }
    return result;
}
