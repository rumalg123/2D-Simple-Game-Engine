#include "Engine.h"
#include "Event.h"
#include "Game.h"
#include "GameConfig.h"
#include "Grid.h"
#include "InputMap.h"
#include "ResourceManager.h"
#include "Scene.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
constexpr int GridColumns = 20;
constexpr int GridRows = 15;
constexpr float CellSize = 0.08f;
constexpr float StepSeconds = 0.12f;
const GridLayout SnakeGrid{GridColumns, GridRows, CellSize, CellSize, {0.0f, 0.0f}, true};

bool containsCell(const std::vector<GridCell>& cells, GridCell target) {
    return std::find(cells.begin(), cells.end(), target) != cells.end();
}

Entity createSpriteEntity(
    Scene& scene,
    GridCell cell,
    TextureHandle texture,
    float halfSize,
    const char* tag,
    int layer) {
    return scene.createSprite(
        tag,
        gridToWorld(SnakeGrid, cell),
        {halfSize, halfSize, 1.0f, 1.0f, 1.0f, 1.0f, layer, texture},
        tag);
}

class SnakeGame final : public IGame {
public:
    const char* name() const override {
        return "Snake Sample";
    }

    const char* controlsHelp() const override {
        return "WASD/arrows changes direction, R resets, Esc exits.";
    }

    void configureInput(GameContext& context) override {
        if (!context.inputMap) {
            return;
        }

        context.inputMap->bindAction("SnakeLeft", GLFW_KEY_A);
        context.inputMap->bindKey("SnakeLeft", GLFW_KEY_LEFT);
        context.inputMap->bindAction("SnakeRight", GLFW_KEY_D);
        context.inputMap->bindKey("SnakeRight", GLFW_KEY_RIGHT);
        context.inputMap->bindAction("SnakeUp", GLFW_KEY_W);
        context.inputMap->bindKey("SnakeUp", GLFW_KEY_UP);
        context.inputMap->bindAction("SnakeDown", GLFW_KEY_S);
        context.inputMap->bindKey("SnakeDown", GLFW_KEY_DOWN);
        context.inputMap->bindAction("ResetScene", GLFW_KEY_R);
    }

    void loadInitialScene(GameContext& context) override {
        if (!context.scene || !context.resources) {
            return;
        }

        Scene& scene = *context.scene;
        ResourceManager& resources = *context.resources;

        snakeTexture = resources.createSolidColorTexture("snake_body", 70, 220, 120, 255);
        snakeHeadTexture = resources.createSolidColorTexture("snake_head", 180, 255, 120, 255);
        foodTexture = resources.createSolidColorTexture("snake_food", 245, 80, 88, 255);

        resetState(scene);
    }

    void onFixedUpdate(GameContext& context, float deltaTime) override {
        if (!context.scene || gameOver) {
            return;
        }

        elapsed += deltaTime;
        if (elapsed < StepSeconds) {
            return;
        }

        elapsed -= StepSeconds;
        direction = requestedDirection;
        step(*context.scene);
    }

    void onEvent(GameContext&, const Event& event) override {
        if (event.type != EventType::ActionChanged || !event.action.pressed) {
            return;
        }

        if (event.action.actionName == "SnakeLeft") {
            requestDirection({-1, 0});
        } else if (event.action.actionName == "SnakeRight") {
            requestDirection({1, 0});
        } else if (event.action.actionName == "SnakeUp") {
            requestDirection({0, -1});
        } else if (event.action.actionName == "SnakeDown") {
            requestDirection({0, 1});
        }
    }

    std::vector<GameDebugStat> debugStats() const override {
        return {{"Score", score}, {"Length", static_cast<int>(snake.size())}};
    }

private:
    std::vector<GridCell> snake;
    std::vector<Entity> snakeEntities;
    GridCell food{0, 0};
    GridCell direction{1, 0};
    GridCell requestedDirection{1, 0};
    Entity foodEntity = InvalidEntity;
    Entity scoreEntity = InvalidEntity;
    Entity messageEntity = InvalidEntity;
    TextureHandle snakeTexture = InvalidTexture;
    TextureHandle snakeHeadTexture = InvalidTexture;
    TextureHandle foodTexture = InvalidTexture;
    float elapsed = 0.0f;
    int score = 0;
    bool gameOver = false;

    void resetState(Scene& scene) {
        scene.clear();
        scene.setCamera({
            0.0f,
            0.0f,
            static_cast<float>(GridColumns) * CellSize * 0.5f,
            static_cast<float>(GridRows) * CellSize * 0.5f,
            InvalidEntity
        });

        snake = {{6, 7}, {5, 7}, {4, 7}};
        direction = {1, 0};
        requestedDirection = direction;
        elapsed = 0.0f;
        score = 0;
        gameOver = false;
        rebuildSnakeEntities(scene);
        spawnFood(scene);
        createHud(scene);
    }

    void requestDirection(GridCell next) {
        if (next.x + direction.x == 0 && next.y + direction.y == 0) {
            return;
        }

        requestedDirection = next;
    }

    void step(Scene& scene) {
        const GridCell nextHead{snake.front().x + direction.x, snake.front().y + direction.y};
        const bool willGrow = nextHead == food;
        const bool movingIntoReleasedTail = !willGrow && !snake.empty() && nextHead == snake.back();
        if (!gridContains(SnakeGrid, nextHead) || (containsCell(snake, nextHead) && !movingIntoReleasedTail)) {
            endGame(scene);
            return;
        }

        snake.insert(snake.begin(), nextHead);
        if (willGrow) {
            ++score;
            if (foodEntity != InvalidEntity) {
                scene.destroyEntity(foodEntity);
                foodEntity = InvalidEntity;
            }
            spawnFood(scene);
        } else {
            snake.pop_back();
        }

        rebuildSnakeEntities(scene);
        updateHud(scene);
    }

    void rebuildSnakeEntities(Scene& scene) {
        for (Entity entity : snakeEntities) {
            scene.destroyEntity(entity);
        }
        snakeEntities.clear();

        for (std::size_t index = 0; index < snake.size(); ++index) {
            const TextureHandle texture = index == 0 ? snakeHeadTexture : snakeTexture;
            snakeEntities.push_back(createSpriteEntity(scene, snake[index], texture, CellSize * 0.43f, "Snake", 10));
        }
    }

    void spawnFood(Scene& scene) {
        const int start = (score * 7 + static_cast<int>(snake.size()) * 3) % (GridColumns * GridRows);
        for (int offset = 0; offset < GridColumns * GridRows; ++offset) {
            const int value = (start + offset) % (GridColumns * GridRows);
            const GridCell candidate = gridFromIndex(value, GridColumns);
            if (!containsCell(snake, candidate)) {
                food = candidate;
                foodEntity = createSpriteEntity(scene, food, foodTexture, CellSize * 0.38f, "Food", 8);
                return;
            }
        }

        endGame(scene);
    }

    void createHud(Scene& scene) {
        scoreEntity = scene.createEntity();
        scene.setTransform(scoreEntity, {-0.96f, 0.90f});
        scene.setText(scoreEntity, {
            "SCORE 0",
            0.045f,
            0.070f,
            0.010f,
            0.030f,
            0.88f,
            1.0f,
            0.72f,
            1.0f,
            100,
            true
        });

        messageEntity = scene.createEntity();
        scene.setTransform(messageEntity, {-0.42f, -0.88f});
        scene.setText(messageEntity, {
            "",
            0.042f,
            0.066f,
            0.010f,
            0.030f,
            1.0f,
            0.82f,
            0.78f,
            1.0f,
            100,
            true
        });
    }

    void updateHud(Scene& scene) {
        TextComponent* scoreText = scene.getText(scoreEntity);
        if (scoreText) {
            scoreText->text = "SCORE " + std::to_string(score);
        }
    }

    void endGame(Scene& scene) {
        gameOver = true;
        TextComponent* message = scene.getText(messageEntity);
        if (message) {
            message->text = "GAME OVER  PRESS R";
        }
    }
};

GameConfig makeDefaultConfig() {
    GameConfig config;
    config.windowTitle = "Snake Sample";
    config.windowWidth = 800;
    config.windowHeight = 600;
    config.editorEnabled = false;
    config.startPlaying = true;
    config.hotReload = false;
    config.assetRoot = "samples/snake/assets";
    config.prefabDirectory = "samples/snake/assets/prefabs";
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

    Engine engine(std::make_unique<SnakeGame>(), config);
    if (!engine.init()) {
        return -1;
    }

    engine.run();
    engine.cleanup();
    return 0;
}
