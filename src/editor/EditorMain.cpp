#include "App.h"
#include "DemoGame.h"
#include "GameConfig.h"

#include <filesystem>
#include <iostream>
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
    std::string configPath;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--help" || argument == "-h") {
            std::cout << "SimpleEngineEditor [--config <game.config.json>]\n"
                         "The current editor opens the bundled demo or a configured startup scene.\n";
            return 0;
        }
        if (argument == "--config" && index + 1 < argc) {
            configPath = argv[++index];
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
        const std::filesystem::path base = std::filesystem::absolute(configPath).parent_path();
        resolveAgainst(base, config.assetRoot);
        resolveAgainst(base, config.prefabDirectory);
        resolveAgainst(base, config.editorLayoutPath);
        resolveAgainst(base, config.startupScene);
    }
    config.editorEnabled = true;
    return EditorApp(createDemoGame(), config).run();
}
