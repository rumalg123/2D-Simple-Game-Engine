#include "Engine.h"
#include "GameConfig.h"
#include "ProjectManager.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace {
void resolveAgainst(const std::filesystem::path& base, std::string& path) {
    if (path.empty()) {
        return;
    }
    std::filesystem::path value(path);
    if (value.is_relative()) {
        value = base / value;
    }
    path = value.lexically_normal().string();
}
}

int main(int argc, char** argv) {
    GameConfig config;
    std::filesystem::path configDirectory = std::filesystem::current_path();
    std::string configPath;
    std::string projectPath;
    std::string sceneOverride;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--help" || argument == "-h") {
            std::cout << "SimpleEnginePlayer [--project <project-folder>] [--config <game.config.json>] [--scene <scene.json>]\n"
                         "Config and scene paths are resolved relative to the config directory.\n"
                         "A packaged game auto-loads adjacent project.simple.json when launched without arguments.\n";
            return 0;
        }
        if ((argument == "--config" || argument == "--scene" || argument == "--project") && index + 1 < argc) {
            const std::string value = argv[++index];
            if (argument == "--config") {
                configPath = value;
            } else if (argument == "--project") {
                projectPath = value;
            } else {
                sceneOverride = value;
            }
            continue;
        }
        std::cerr << "Unknown or incomplete argument: " << argument << '\n';
        return 1;
    }

    // Exported games carry project.simple.json beside their executable.
    // With no CLI arguments, open that project automatically, independent
    // of the launching terminal's working directory.
    if (argc == 1) {
        std::error_code ec;
        const auto executable = std::filesystem::absolute(argv[0], ec);
        if (!ec) {
            const auto adjacent = executable.parent_path();
            if (std::filesystem::is_regular_file(adjacent / ProjectManager::ManifestName))
                projectPath = adjacent.string();
        }
    }

    if (!projectPath.empty() && !configPath.empty()) {
        std::cerr << "--project and --config cannot be used together.\n";
        return 1;
    }
    if (!projectPath.empty()) {
        ProjectInfo project;
        std::string error;
        if (!ProjectManager::open(projectPath, project, error)) {
            std::cerr << "Cannot open project: " << error << '\n';
            return 1;
        }
        if (project.config.startupScene.empty()) {
            std::cerr << "Project has no startupScene. Save a scene first.\n";
            return 1;
        }
        config = project.config;
        configDirectory = project.root;
        // Relative texture and other imported asset paths resolve from the
        // packaged project, never an unrelated Explorer/terminal directory.
        std::filesystem::current_path(project.root, ec);
        if (ec) {
            std::cerr << "Cannot enter game project directory: " << ec.message() << '\\n';
            return 1;
        }
    }

    if (!configPath.empty()) {
        std::string error;
        if (!loadGameConfigFromFile(configPath, config, error)) {
            std::cerr << error << '\n';
            return 1;
        }
        configDirectory = std::filesystem::absolute(configPath).parent_path();
    }

    // Player binaries must never initialize editor-specific libraries.
    config.editorEnabled = false;
    config.startPlaying = true;
    resolveAgainst(configDirectory, config.assetRoot);
    resolveAgainst(configDirectory, config.prefabDirectory);
    resolveAgainst(configDirectory, config.startupScene);
    if (!sceneOverride.empty()) {
        config.startupScene = sceneOverride;
        resolveAgainst(configDirectory, config.startupScene);
    }

    Engine game(std::unique_ptr<IGame>{}, config);
    if (!game.init()) {
        return 1;
    }
    game.run();
    game.cleanup();
    return 0;
}
