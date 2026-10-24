#include "GameConfig.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "SceneSerializer.h"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Expected config path and scene path.\n";
        return 1;
    }

    GameConfig config;
    std::string error;
    if (!loadGameConfigFromFile(argv[1], config, error)) {
        std::cerr << error << '\n';
        return 1;
    }
    if (config.startupScene != "assets/scenes/welcome.scene.json") {
        std::cerr << "startupScene was not parsed correctly.\n";
        return 1;
    }

    ResourceManager resources;
    Scene scene;
    if (!loadSceneFromJson(scene, resources, argv[2], error)) {
        std::cerr << error << '\n';
        return 1;
    }
    if (scene.livingEntityCount() != 2) {
        std::cerr << "Expected two entities in the starter scene.\n";
        return 1;
    }
    const Entity welcome = scene.findEntityByName("Welcome text");
    if (welcome == InvalidEntity || !scene.getText(welcome) ||
        scene.getText(welcome)->text != "SIMPLE ENGINE") {
        std::cerr << "The player scene is missing its welcome text.\n";
        return 1;
    }

    // Loading a missing scene must fail without pretending it was successful.
    Scene missingScene;
    std::string missingError;
    if (loadSceneFromJson(missingScene, resources, "__missing_scene__.json", missingError) ||
        missingError.empty()) {
        std::cerr << "Missing scenes must report a failure.\n";
        return 1;
    }

    std::cout << "Player config and standalone serialized scene passed.\n";
    return 0;
}
