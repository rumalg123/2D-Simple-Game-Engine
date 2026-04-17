#include "DemoGame.h"
#include "Engine.h"
#include "GameConfig.h"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    GameConfig config;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--runtime") {
            config.editorEnabled = false;
            config.startPlaying = true;
            continue;
        }

        if (argument == "--editor") {
            config.editorEnabled = true;
            continue;
        }

        if (argument == "--config") {
            if (index + 1 >= argc) {
                std::cerr << "--config requires a file path.\n";
                return 1;
            }

            std::string error;
            if (!loadGameConfigFromFile(argv[++index], config, error)) {
                std::cerr << error << '\n';
                return 1;
            }
            continue;
        }

        std::cerr << "Unknown argument: " << argument << '\n';
        return 1;
    }

    Engine engine(createDemoGame(), config);

    if (!engine.init())
        return -1;

    engine.run();
    engine.cleanup();

    return 0;
}
