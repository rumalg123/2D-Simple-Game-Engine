#pragma once

#include "Engine.h"
#include "Game.h"
#include "GameConfig.h"

#include <memory>

class RuntimeApp {
public:
    RuntimeApp(std::unique_ptr<IGame> game, GameConfig config = {});

    int run();

    static GameConfig makeRuntimeConfig(GameConfig config);

private:
    Engine engine;
};

class EditorApp {
public:
    EditorApp(std::unique_ptr<IGame> game, GameConfig config = {});

    int run();

    static GameConfig makeEditorConfig(GameConfig config);

private:
    Engine engine;
};

int runGameApp(std::unique_ptr<IGame> game, GameConfig config);
