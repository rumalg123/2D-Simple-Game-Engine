#include "Engine.h"
#include "GameConfig.h"

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
    std::string sceneOverride;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--help" || argument == "-h") {
            std::cout << "SimpleEnginePlayer [--config <game.config.json>] [--scene <scene.json>]\n"
                         "Config and scene paths are resolved relative to the config directory.\n"
                         "The player runs serialized scenes; Lua/game-project scripts are not yet supported.\n";
            return 0;
        }
        if ((argument == "--config" || argument == "--scene") && index + 1 < argc) {
            const std::string value = argv[++index];
            if (argument == "--config") {
                configPath = value;
            } else {
                sceneOverride = value;
            }
            continue;
        }
        std::cerr << "Unknown or incomplete argument: " << argument << '\n';
        return 1;
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
