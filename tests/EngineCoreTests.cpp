#include "GameConfig.h"
#include "InputMap.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "SceneSerializer.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
class TempDirectory {
public:
    TempDirectory() {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() / ("SimpleEngineTests_" + std::to_string(suffix));
        std::filesystem::create_directories(path);
    }

    ~TempDirectory() {
        if (preserve) {
            return;
        }
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }

    const std::filesystem::path& get() const {
        return path;
    }

    void preserveOnFailure() {
        preserve = true;
    }

private:
    std::filesystem::path path;
    bool preserve = false;
};

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void expectNear(float actual, float expected, const std::string& message) {
    if (std::fabs(actual - expected) > 0.0001f) {
        throw std::runtime_error(message + " expected " + std::to_string(expected) + " got " + std::to_string(actual));
    }
}

void testInputMapSupportsKeyboardAndGamepadBindings() {
    InputMap input;
    input.bindAction("MoveLeft", 65);
    input.bindKey("MoveLeft", 37);
    input.bindGamepadButton("MoveLeft", 14);
    input.bindGamepadAxis("MoveLeft", 0, GamepadAxisDirection::Negative, 0.35f);
    input.bindGamepadAxis("MoveRight", 0, GamepadAxisDirection::Positive, 0.35f);

    expect(input.isBoundToKey("MoveLeft", 65), "MoveLeft should keep its primary keyboard binding.");
    expect(input.isBoundToKey("MoveLeft", 37), "MoveLeft should allow multiple keyboard bindings.");
    expect(!input.isDown("MoveLeft"), "MoveLeft should start released.");

    input.handleKeyChanged(65, true);
    expect(input.isDown("MoveLeft"), "Keyboard press should activate an action.");
    expectNear(input.getAxis("MoveLeft", "MoveRight"), -1.0f, "Keyboard left should drive the horizontal axis.");

    input.handleKeyChanged(65, false);
    expect(!input.isDown("MoveLeft"), "Keyboard release should deactivate an action when no other binding is down.");

    input.setGamepadButtonState(14, true);
    expect(input.isDown("MoveLeft"), "Gamepad button should activate an action.");
    input.setGamepadButtonState(14, false);
    expect(!input.isDown("MoveLeft"), "Gamepad button release should deactivate an action.");

    input.setGamepadAxisValue(0, -0.50f);
    expect(input.isDown("MoveLeft"), "Negative gamepad axis should activate its bound action.");
    expectNear(input.getAxis("MoveLeft", "MoveRight"), -1.0f, "Negative gamepad axis should drive the action axis.");

    input.setGamepadAxisValue(0, 0.50f);
    expect(!input.isDown("MoveLeft"), "Positive axis movement should release the negative action.");
    expect(input.isDown("MoveRight"), "Positive gamepad axis should activate its bound action.");
    expectNear(input.getAxis("MoveLeft", "MoveRight"), 1.0f, "Positive gamepad axis should drive the action axis.");

    input.clearGamepadState();
    expect(!input.isDown("MoveRight"), "Clearing gamepad state should release gamepad-only actions.");

    const InputAction& moveLeft = input.getActions().at("MoveLeft");
    expect(moveLeft.keys.size() == 2, "MoveLeft should store both keyboard keys.");
    expect(moveLeft.gamepadButtons.size() == 1, "MoveLeft should store one gamepad button.");
    expect(moveLeft.gamepadAxes.size() == 1, "MoveLeft should store one gamepad axis binding.");
}

void testResourceManagerReusesNamedTextures() {
    ResourceManager resources;
    const TextureHandle first = resources.createSolidColorTexture("white", 255, 255, 255, 255);
    const TextureHandle second = resources.createSolidColorTexture("white", 1, 2, 3, 4);

    expect(first == second, "ResourceManager should reuse existing texture handles by name.");
    expect(resources.textureCount() == 1, "Duplicate named textures should not create extra texture resources.");

    const TextureResource& texture = resources.getTexture(first);
    expect(texture.width == 1 && texture.height == 1, "Solid color texture should be one pixel.");
    expect(texture.pixels.size() == 4, "Solid color texture should contain RGBA bytes.");
    expect(texture.pixels[0] == 255 && texture.pixels[3] == 255, "Solid color texture should keep original pixel data.");
}

void testGameConfigLoadsFlatJsonAndKeepsDefaults() {
    TempDirectory temp;
    const std::filesystem::path configPath = temp.get() / "game.config.json";

    {
        std::ofstream output(configPath.string());
        output << "{\n";
        output << "  \"windowTitle\": \"Snake\",\n";
        output << "  \"windowWidth\": 960,\n";
        output << "  \"windowHeight\": 540,\n";
        output << "  \"editorEnabled\": false,\n";
        output << "  \"hotReload\": false,\n";
        output << "  \"assetRoot\": \"samples/snake/assets\"\n";
        output << "}\n";
    }

    GameConfig config;
    config.prefabDirectory = "custom_prefabs";
    std::string error;
    const bool loaded = loadGameConfigFromFile(configPath.string(), config, error);
    expect(loaded, "Loading game config failed: " + error);

    expect(config.windowTitle == "Snake", "Config should load the window title.");
    expect(config.windowWidth == 960, "Config should load the window width.");
    expect(config.windowHeight == 540, "Config should load the window height.");
    expect(!config.editorEnabled, "Config should load editor mode.");
    expect(!config.hotReload, "Config should load hot reload mode.");
    expect(config.startPlaying, "Missing config keys should keep default bool values.");
    expect(config.vsync, "Missing config keys should keep default vsync value.");
    expect(config.assetRoot == "samples/snake/assets", "Config should load asset root.");
    expect(config.prefabDirectory == "custom_prefabs", "Missing string keys should keep existing values.");
}

void testGameConfigRejectsInvalidWindowSize() {
    TempDirectory temp;
    const std::filesystem::path configPath = temp.get() / "bad.config.json";

    {
        std::ofstream output(configPath.string());
        output << "{ \"windowWidth\": 0, \"windowHeight\": 600 }\n";
    }

    GameConfig config;
    std::string error;
    expect(!loadGameConfigFromFile(configPath.string(), config, error), "Invalid window size should fail.");
    expect(error.find("windowWidth") != std::string::npos, "Invalid window size should report the window keys.");
}

void testSceneJsonRoundTripsTextAndTilemaps() {
    TempDirectory temp;
    const std::filesystem::path scenePath = temp.get() / "scene.json";

    Scene scene;
    ResourceManager resources;
    const TextureHandle tileTexture = resources.createSolidColorTexture("test_tiles", 56, 152, 75, 255);

    const Entity discarded = scene.createEntity();
    scene.setName(discarded, {"Discarded"});

    const Entity entity = scene.createEntity();
    scene.destroyEntity(discarded);

    scene.setName(entity, {"LevelRoot"});
    scene.setTag(entity, {"Tilemap"});
    scene.setTransform(entity, {-1.25f, 1.50f});
    scene.setText(entity, {"HELLO\n2D", 0.04f, 0.07f, 0.01f, 0.02f, 0.9f, 0.8f, 0.7f, 1.0f, 80, false});
    scene.setTilemap(entity, {
        3,
        2,
        0.25f,
        0.50f,
        4,
        1,
        -30,
        tileTexture,
        0.8f,
        0.9f,
        1.0f,
        0.75f,
        {0, 1, -1, 2, 3, 0}
    });
    scene.setPlayerEntity(entity);
    scene.setCamera({5.0f, 6.0f, 1.5f, 2.0f, entity});

    std::string error;
    expect(saveSceneToJson(scene, resources, scenePath.string(), error), "Saving scene JSON failed: " + error);

    Scene loadedScene;
    ResourceManager loadedResources;
    if (!loadSceneFromJson(loadedScene, loadedResources, scenePath.string(), error)) {
        temp.preserveOnFailure();
        throw std::runtime_error("Loading scene JSON failed from " + scenePath.string() + ": " + error);
    }

    expect(loadedScene.livingEntityCount() == 1, "Loaded scene should contain one living entity.");
    const Entity loadedEntity = loadedScene.getPlayerEntity();
    expect(loadedEntity != InvalidEntity, "Loaded scene should remap player entity.");
    expect(loadedScene.getCamera().target == loadedEntity, "Loaded scene should remap camera target.");
    expectNear(loadedScene.getCamera().x, 5.0f, "Camera x should round-trip.");
    expectNear(loadedScene.getCamera().halfHeight, 2.0f, "Camera half-height should round-trip.");

    const NameComponent* name = loadedScene.getName(loadedEntity);
    expect(name && name->name == "LevelRoot", "Name component should round-trip.");

    const TransformComponent* transform = loadedScene.getTransform(loadedEntity);
    expect(transform != nullptr, "Transform component should round-trip.");
    expectNear(transform->x, -1.25f, "Transform x should round-trip.");
    expectNear(transform->y, 1.50f, "Transform y should round-trip.");

    const TextComponent* text = loadedScene.getText(loadedEntity);
    expect(text != nullptr, "Text component should round-trip.");
    expect(text->text == "HELLO\n2D", "Text content should round-trip escaped newlines.");
    expectNear(text->characterWidth, 0.04f, "Text character width should round-trip.");
    expect(text->screenSpace == false, "Text screen-space flag should round-trip.");

    const TilemapComponent* tilemap = loadedScene.getTilemap(loadedEntity);
    expect(tilemap != nullptr, "Tilemap component should round-trip.");
    expect(tilemap->columns == 3 && tilemap->rows == 2, "Tilemap dimensions should round-trip.");
    expectNear(tilemap->tileWidth, 0.25f, "Tilemap tile width should round-trip.");
    expectNear(tilemap->tileHeight, 0.50f, "Tilemap tile height should round-trip.");
    expect(tilemap->atlasColumns == 4 && tilemap->atlasRows == 1, "Tilemap atlas layout should round-trip.");
    expect(tilemap->layer == -30, "Tilemap layer should round-trip.");
    expectNear(tilemap->alpha, 0.75f, "Tilemap alpha should round-trip.");
    expect(tilemap->tiles == std::vector<int>({0, 1, -1, 2, 3, 0}), "Tilemap tile data should round-trip.");
    expect(tilemap->texture != InvalidTexture, "Loaded tilemap should resolve a usable texture.");
}

void runTest(const char* name, void (*test)()) {
    test();
    std::cout << "[PASS] " << name << '\n';
}
}

int main() {
    try {
        runTest("InputMap keyboard and gamepad bindings", testInputMapSupportsKeyboardAndGamepadBindings);
        runTest("ResourceManager named texture reuse", testResourceManagerReusesNamedTextures);
        runTest("GameConfig flat JSON loading", testGameConfigLoadsFlatJsonAndKeepsDefaults);
        runTest("GameConfig invalid window size rejection", testGameConfigRejectsInvalidWindowSize);
        runTest("Scene JSON text and tilemap round-trip", testSceneJsonRoundTripsTextAndTilemaps);
    } catch (const std::exception& exception) {
        std::cerr << "[FAIL] " << exception.what() << '\n';
        return 1;
    }

    return 0;
}
