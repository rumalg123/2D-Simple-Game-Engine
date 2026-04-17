#include "App.h"

#include <utility>

namespace {
int runEngine(Engine& engine) {
    if (!engine.init()) {
        return -1;
    }

    engine.run();
    engine.cleanup();
    return 0;
}
}

RuntimeApp::RuntimeApp(std::unique_ptr<IGame> game, GameConfig config)
    : engine(std::move(game), makeRuntimeConfig(std::move(config))) {}

int RuntimeApp::run() {
    return runEngine(engine);
}

GameConfig RuntimeApp::makeRuntimeConfig(GameConfig config) {
    config.editorEnabled = false;
    config.startPlaying = true;
    return config;
}

EditorApp::EditorApp(std::unique_ptr<IGame> game, GameConfig config)
    : engine(std::move(game), makeEditorConfig(std::move(config))) {}

int EditorApp::run() {
    return runEngine(engine);
}

GameConfig EditorApp::makeEditorConfig(GameConfig config) {
    config.editorEnabled = true;
    return config;
}

int runGameApp(std::unique_ptr<IGame> game, GameConfig config) {
    if (config.editorEnabled) {
        EditorApp app(std::move(game), std::move(config));
        return app.run();
    }

    RuntimeApp app(std::move(game), std::move(config));
    return app.run();
}
