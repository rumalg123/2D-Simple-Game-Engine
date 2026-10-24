#pragma once

#include <string>

struct GameConfig {
    std::string windowTitle = "Engine v0.3";
    int windowWidth = 800;
    int windowHeight = 600;
    bool editorEnabled = true;
    bool startPlaying = true;
    bool hotReload = true;
    bool vsync = true;
    std::string startupScene;
    std::string assetRoot = "assets";
    std::string prefabDirectory = "assets/prefabs";
    std::string editorLayoutPath = "assets/editor_layout.ini";
    int formatVersion = 1; // Project manifest schema version; legacy configs default to 1.
};

bool loadGameConfigFromFile(const std::string& path, GameConfig& config, std::string& error);

