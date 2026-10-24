#include "ProjectManager.h"
#include "LuaScriptRuntime.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "SceneSerializer.h"
#include "ScriptSystem.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 3 || argc > 4) {
        std::cerr << "Usage:\n"
                  << "  simpleengine_project create <parent-directory> <project-name>\n"
                  << "  simpleengine_project info <project-directory>\n"
                  << "  simpleengine_project validate <project-directory>\n";
        return 2;
    }

    ProjectInfo project;
    std::string error;
    const std::string operation = argv[1];
    bool success = false;
    if (operation == "create" && argc == 4) {
        success = ProjectManager::create(argv[2], argv[3], project, error);
    } else if ((operation == "info" || operation == "validate") && argc == 3) {
        success = ProjectManager::open(argv[2], project, error);
    } else {
        std::cerr << "Unknown command or incorrect arguments.\n";
        return 2;
    }

    if (!success) {
        std::cerr << "Project error: " << error << '\n';
        return 1;
    }

    if (operation == "validate") {
        std::vector<std::string> errors;
        LuaScriptRuntime lua;
        ScriptSystem registered;
        const std::size_t scripts = lua.registerDirectory(
            registered, std::filesystem::path(project.config.assetRoot) / "Scripts", errors);

        if (project.config.startupScene.empty()) {
            errors.push_back("No startupScene is configured.");
        } else {
            Scene scene;
            ResourceManager resources;
            std::string sceneError;
            if (!loadSceneFromJson(scene, resources, project.config.startupScene, sceneError)) {
                errors.push_back(sceneError);
            } else {
                for (std::size_t i = 0; i < scene.getScriptPool().size(); ++i) {
                    const ScriptComponent& script = scene.getScriptPool().componentAt(i);
                    if (script.active && !script.scriptName.empty() &&
                        !registered.hasScript(script.scriptName)) {
                        errors.push_back("Scene references missing/invalid Lua script: " +
                                         script.scriptName);
                    }
                }
                std::cout << "Scene entities: " << scene.livingEntityCount() << '\n';
            }
        }
        for (const auto& issue : errors) std::cerr << "Validation: " << issue << '\n';
        if (!errors.empty()) {
            std::cerr << "Project validation failed with " << errors.size() << " issue(s).\n";
            return 1;
        }
        std::cout << "Project validated successfully; " << scripts
                  << " Lua script(s) compiled.\n";
        return 0;
    }

    std::cout << "Project: " << project.name << '\n'
              << "Root: " << project.root.string() << '\n'
              << "Assets: " << project.config.assetRoot << '\n'
              << "Startup scene: " << (project.config.startupScene.empty()
                                      ? "(not configured)" : project.config.startupScene) << '\n';
    return 0;
}
