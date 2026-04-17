#include "App.h"
#include "Event.h"
#include "Game.h"
#include "GameConfig.h"
#include "InputMap.h"
#include "Prefab.h"
#include "ResourceManager.h"
#include "Scene.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
constexpr float PlayerSpeed = 0.72f;
constexpr float PlayerHalfSize = 0.055f;
constexpr float GoalHalfSize = 0.06f;
constexpr float WorldHalfWidth = 1.0f;
constexpr float WorldHalfHeight = 0.75f;

class BasicGame final : public IGame {
public:
    const char* name() const override {
        return "Basic Game Template";
    }

    const char* controlsHelp() const override {
        return "WASD/arrows moves, Space marks the goal, R resets, Esc exits.";
    }

    void configureInput(GameContext& context) override {
        if (!context.inputMap) {
            return;
        }

        context.inputMap->bindAction("MoveLeft", GLFW_KEY_A);
        context.inputMap->bindKey("MoveLeft", GLFW_KEY_LEFT);
        context.inputMap->bindGamepadButton("MoveLeft", GLFW_GAMEPAD_BUTTON_DPAD_LEFT);
        context.inputMap->bindGamepadAxis("MoveLeft", GLFW_GAMEPAD_AXIS_LEFT_X, GamepadAxisDirection::Negative);

        context.inputMap->bindAction("MoveRight", GLFW_KEY_D);
        context.inputMap->bindKey("MoveRight", GLFW_KEY_RIGHT);
        context.inputMap->bindGamepadButton("MoveRight", GLFW_GAMEPAD_BUTTON_DPAD_RIGHT);
        context.inputMap->bindGamepadAxis("MoveRight", GLFW_GAMEPAD_AXIS_LEFT_X, GamepadAxisDirection::Positive);

        context.inputMap->bindAction("MoveUp", GLFW_KEY_W);
        context.inputMap->bindKey("MoveUp", GLFW_KEY_UP);
        context.inputMap->bindGamepadButton("MoveUp", GLFW_GAMEPAD_BUTTON_DPAD_UP);
        context.inputMap->bindGamepadAxis("MoveUp", GLFW_GAMEPAD_AXIS_LEFT_Y, GamepadAxisDirection::Negative);

        context.inputMap->bindAction("MoveDown", GLFW_KEY_S);
        context.inputMap->bindKey("MoveDown", GLFW_KEY_DOWN);
        context.inputMap->bindGamepadButton("MoveDown", GLFW_GAMEPAD_BUTTON_DPAD_DOWN);
        context.inputMap->bindGamepadAxis("MoveDown", GLFW_GAMEPAD_AXIS_LEFT_Y, GamepadAxisDirection::Positive);

        context.inputMap->bindAction("Confirm", GLFW_KEY_SPACE);
        context.inputMap->bindKey("Confirm", GLFW_KEY_ENTER);
        context.inputMap->bindGamepadButton("Confirm", GLFW_GAMEPAD_BUTTON_A);
        context.inputMap->bindAction("ResetScene", GLFW_KEY_R);
    }

    void loadInitialScene(GameContext& context) override {
        if (!context.scene || !context.resources || !context.prefabs) {
            return;
        }

        Scene& scene = *context.scene;
        ResourceManager& resources = *context.resources;
        PrefabRegistry& prefabs = *context.prefabs;

        scene.clear();
        scene.setCamera({0.0f, 0.0f, WorldHalfWidth, WorldHalfHeight, InvalidEntity});

        backgroundTexture = resources.createSolidColorTexture("basic_background", 28, 42, 58, 255);
        playerTexture = resources.createSolidColorTexture("basic_player", 88, 190, 255, 255);
        goalTexture = resources.createSolidColorTexture("basic_goal", 245, 218, 92, 255);

        registerPrefabs(prefabs);
        scene.createSprite(
            "Background",
            {0.0f, 0.0f},
            {WorldHalfWidth, WorldHalfHeight, 1.0f, 1.0f, 1.0f, 1.0f, -100, backgroundTexture},
            "Background");

        player = context.instantiatePrefab("Player", {-0.35f, 0.0f});
        goal = context.instantiatePrefab("Goal", {0.42f, 0.0f});
        hud = scene.createText(
            "Hud",
            {-0.96f, 0.90f},
            {"MOVE TO THE GOAL", 0.040f, 0.065f, 0.010f, 0.030f, 1.0f, 1.0f, 1.0f, 1.0f, 100, true},
            "UI");

        goalReached = false;
        confirmCount = 0;
    }

    void onEvent(GameContext&, const Event& event) override {
        if (event.type == EventType::ActionChanged && event.action.pressed && event.action.actionName == "Confirm") {
            ++confirmCount;
        }
    }

    void onFixedUpdate(GameContext& context, float deltaTime) override {
        if (!context.scene || !context.inputMap) {
            return;
        }

        Scene& scene = *context.scene;
        InputMap& input = *context.inputMap;
        TransformComponent* transform = scene.getTransform(player);
        if (!transform) {
            return;
        }

        float inputX = input.getAxis("MoveLeft", "MoveRight");
        float inputY = input.getAxis("MoveDown", "MoveUp");
        if (inputX != 0.0f && inputY != 0.0f) {
            constexpr float diagonalScale = 0.70710678f;
            inputX *= diagonalScale;
            inputY *= diagonalScale;
        }

        transform->x = std::clamp(transform->x + inputX * PlayerSpeed * deltaTime, -0.92f, 0.92f);
        transform->y = std::clamp(transform->y + inputY * PlayerSpeed * deltaTime, -0.68f, 0.68f);

        updateGoalState(scene, *transform);
        updateHud(scene, *transform);
    }

    std::vector<GameDebugStat> debugStats() const override {
        return {{"Confirm", confirmCount}, {"Goal", goalReached ? 1 : 0}};
    }

private:
    Entity player = InvalidEntity;
    Entity goal = InvalidEntity;
    Entity hud = InvalidEntity;
    TextureHandle backgroundTexture = InvalidTexture;
    TextureHandle playerTexture = InvalidTexture;
    TextureHandle goalTexture = InvalidTexture;
    int confirmCount = 0;
    bool goalReached = false;

    void registerPrefabs(PrefabRegistry& prefabs) {
        Prefab playerPrefab;
        playerPrefab.name = "Player";
        playerPrefab.sprite = SpriteComponent{PlayerHalfSize, PlayerHalfSize, 1.0f, 1.0f, 1.0f, 1.0f, 10, playerTexture};
        playerPrefab.collider = ColliderComponent{PlayerHalfSize, PlayerHalfSize, false, true};
        playerPrefab.tag = TagComponent{"Player"};
        prefabs.registerPrefab(playerPrefab);

        Prefab goalPrefab;
        goalPrefab.name = "Goal";
        goalPrefab.sprite = SpriteComponent{GoalHalfSize, GoalHalfSize, 1.0f, 1.0f, 1.0f, 1.0f, 5, goalTexture};
        goalPrefab.collider = ColliderComponent{GoalHalfSize, GoalHalfSize, false, true};
        goalPrefab.tag = TagComponent{"Goal"};
        prefabs.registerPrefab(goalPrefab);
    }

    void updateGoalState(Scene& scene, const TransformComponent& playerTransform) {
        const TransformComponent* goalTransform = scene.getTransform(goal);
        if (!goalTransform) {
            return;
        }

        const bool overlaps =
            std::abs(playerTransform.x - goalTransform->x) < PlayerHalfSize + GoalHalfSize &&
            std::abs(playerTransform.y - goalTransform->y) < PlayerHalfSize + GoalHalfSize;
        goalReached = overlaps;

        SpriteComponent* playerSprite = scene.getSprite(player);
        if (playerSprite) {
            playerSprite->red = overlaps ? 0.60f : 1.0f;
            playerSprite->green = overlaps ? 1.0f : 1.0f;
            playerSprite->blue = overlaps ? 0.60f : 1.0f;
        }
    }

    void updateHud(Scene& scene, const TransformComponent& playerTransform) {
        TextComponent* text = scene.getText(hud);
        if (!text) {
            return;
        }

        text->text = goalReached ? "GOAL REACHED  SPACE COUNT " : "MOVE TO THE GOAL  SPACE COUNT ";
        text->text += std::to_string(confirmCount);
        text->text += "  X ";
        text->text += std::to_string(static_cast<int>(playerTransform.x * 100.0f));
    }
};

GameConfig makeDefaultConfig() {
    GameConfig config;
    config.windowTitle = "Basic Game Template";
    config.windowWidth = 800;
    config.windowHeight = 600;
    config.editorEnabled = false;
    config.startPlaying = true;
    config.hotReload = false;
    config.assetRoot = "templates/basic_game/assets";
    config.prefabDirectory = "templates/basic_game/assets/prefabs";
    config.editorLayoutPath = "assets/editor_layout.ini";
    return config;
}
}

int main(int argc, char** argv) {
    GameConfig config = makeDefaultConfig();

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

    return runGameApp(std::make_unique<BasicGame>(), config);
}
