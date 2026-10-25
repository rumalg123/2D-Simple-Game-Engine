#include "ProjectManager.h"
#include "ProjectValidation.h"
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
        const ProjectValidationResult result = validateProject(project);
        for (const auto& issue : result.errors)
            std::cerr << "Validation: " << issue << '\n';
        if (!result.ok()) {
            std::cerr << "Project validation failed with "
                      << result.errors.size() << " issue(s).\n";
            return 1;
        }
        std::cout << "Project validated successfully; " << result.scenes
                  << " scene(s), " << result.entities << " entities and "
                  << result.luaScripts << " Lua script(s) compiled.\n";
        return 0;
    }

    std::cout << "Project: " << project.name << '\n'
              << "Root: " << project.root.string() << '\n'
              << "Assets: " << project.config.assetRoot << '\n'
              << "Startup scene: " << (project.config.startupScene.empty()
                                      ? "(not configured)" : project.config.startupScene) << '\n';
    return 0;
}
