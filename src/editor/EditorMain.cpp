#include "App.h"
#include "DemoGame.h"
#include "GameConfig.h"
#include "ProjectLauncher.h"
#include "ProjectManager.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace {
void resolveAgainst(const std::filesystem::path& base, std::string& path) {
    if (path.empty()) return;
    std::filesystem::path value(path);
    if (value.is_relative()) value = base / value;
    path = value.lexically_normal().string();
}
}

int main(int argc, char** argv) {
    GameConfig config;
    std::string configPath;
    std::string projectPath;
    bool demo = false;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--help" || argument == "-h") {
            std::cout << "SimpleEngineEditor [--project <project-folder>]\n"
                         "                   [--config <game.config.json>] [--demo]\n"
                         "With no arguments, opens the visual New/Open Project launcher.\n"
                         "--project opens an independent scene project. --config/--demo "
                         "preserves the built-in C++ demo.\n";
            return 0;
        }
        if ((argument == "--config" || argument == "--project") && index + 1 < argc) {
            if (argument == "--config") configPath = argv[++index];
            else projectPath = argv[++index];
            continue;
        }
        if (argument == "--demo") {
            demo = true;
            continue;
        }
        std::cerr << "Unknown or incomplete argument: " << argument << '\n';
        return 1;
    }
    if ((!configPath.empty() && !projectPath.empty()) ||
        (!projectPath.empty() && demo)) {
        std::cerr << "Choose either --project or --config/--demo.\n";
        return 1;
    }

    if (!projectPath.empty() || (configPath.empty() && !demo)) {
        ProjectInfo project;
        if (!projectPath.empty()) {
            std::string error;
            if (!ProjectManager::open(projectPath, project, error)) {
                std::cerr << "Cannot open project: " << error << '\n';
                return 1;
            }
        } else if (!showProjectLauncher(project)) {
            return 0; // Project selection cancelled.
        }
        project.config.editorEnabled = true;
        return EditorApp(std::unique_ptr<IGame>{}, std::move(project.config)).run();
    }

    if (!configPath.empty()) {
        std::string error;
        if (!loadGameConfigFromFile(configPath, config, error)) {
            std::cerr << error << '\n';
            return 1;
        }
        const std::filesystem::path base = std::filesystem::absolute(configPath).parent_path();
        resolveAgainst(base, config.assetRoot);
        resolveAgainst(base, config.prefabDirectory);
        resolveAgainst(base, config.editorLayoutPath);
        resolveAgainst(base, config.startupScene);
    }
    config.editorEnabled = true;
    return EditorApp(createDemoGame(), config).run();
}
