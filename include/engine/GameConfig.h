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
    std::string assetRoot = "assets";
    std::string prefabDirectory = "assets/prefabs";
    std::string editorLayoutPath = "assets/editor_layout.ini";
};

bool loadGameConfigFromFile(const std::string& path, GameConfig& config, std::string& error);

