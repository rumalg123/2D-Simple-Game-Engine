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
constexpr float WorldHalfWidth = 1.35f;
constexpr float WorldHalfHeight = 1.0f;
constexpr float BirdX = -0.48f;
constexpr float BirdHalfWidth = 0.055f;
constexpr float BirdHalfHeight = 0.045f;
constexpr float Gravity = -2.45f;
constexpr float FlapVelocity = 0.82f;
constexpr float ScrollSpeed = 0.52f;
constexpr float PipeHalfWidth = 0.075f;
constexpr float PipeGapHeight = 0.48f;
constexpr float PipeSpacing = 0.88f;
constexpr float GroundTop = -0.86f;

struct PipePair {
    Entity top = InvalidEntity;
    Entity bottom = InvalidEntity;
    float x = 0.0f;
    float gapY = 0.0f;
    bool scored = false;
};

float gapYForIndex(int index) {
    constexpr float GapPattern[] = {0.10f, -0.18f, 0.24f, -0.05f, 0.18f, -0.24f};
    return GapPattern[static_cast<std::size_t>(index) % (sizeof(GapPattern) / sizeof(GapPattern[0]))];
}

bool actionPressed(const Event& event, const char* actionName) {
    return event.type == EventType::ActionChanged && event.action.pressed && event.action.actionName == actionName;
}

class FlappyBirdGame final : public IGame {
public:
    const char* name() const override {
        return "Flappy Bird Sample";
    }

    const char* controlsHelp() const override {
        return "Space/W/up flaps, R resets, Esc exits.";
    }

    void configureInput(GameContext& context) override {
        if (!context.inputMap) {
            return;
        }

        context.inputMap->bindAction("Flap", GLFW_KEY_SPACE);
        context.inputMap->bindKey("Flap", GLFW_KEY_W);
        context.inputMap->bindKey("Flap", GLFW_KEY_UP);
        context.inputMap->bindGamepadButton("Flap", GLFW_GAMEPAD_BUTTON_A);
        context.inputMap->bindAction("ResetScene", GLFW_KEY_R);
    }

    void loadInitialScene(GameContext& context) override {
        if (!context.scene || !context.resources || !context.prefabs) {
            return;
        }

        Scene& scene = *context.scene;
        ResourceManager& resources = *context.resources;
        scene.clear();
        scene.setCamera({0.0f, 0.0f, WorldHalfWidth, WorldHalfHeight, InvalidEntity});

        birdTexture = resources.createSolidColorTexture("flappy_bird", 255, 219, 88, 255);
        pipeTexture = resources.createSolidColorTexture("flappy_pipe", 80, 206, 106, 255);
        skyTexture = resources.createSolidColorTexture("flappy_sky", 92, 190, 235, 255);
        groundTexture = resources.createSolidColorTexture("flappy_ground", 218, 177, 92, 255);

        registerPipePrefab(*context.prefabs);
        resetState(context);
    }

    void onEvent(GameContext& context, const Event& event) override {
        if (actionPressed(event, "Flap")) {
            if (gameOver) {
                context.reloadScene();
                return;
            }

            started = true;
            birdVelocity = FlapVelocity;
            if (context.scene) {
                setMessageText(*context.scene, "");
            }
        }
    }

    void onFixedUpdate(GameContext& context, float deltaTime) override {
        if (!context.scene || gameOver || !started) {
            return;
        }

        Scene& scene = *context.scene;
        birdVelocity += Gravity * deltaTime;
        birdY += birdVelocity * deltaTime;
        updateBird(scene);
        updatePipes(scene, deltaTime);
        updateScore(scene);
        checkGameOver(scene);
    }

    std::vector<GameDebugStat> debugStats() const override {
        return {{"Score", score}};
    }

private:
    Entity bird = InvalidEntity;
    Entity scoreText = InvalidEntity;
    Entity messageText = InvalidEntity;
    std::vector<PipePair> pipes;
    TextureHandle birdTexture = InvalidTexture;
    TextureHandle pipeTexture = InvalidTexture;
    TextureHandle skyTexture = InvalidTexture;
    TextureHandle groundTexture = InvalidTexture;
    float birdY = 0.05f;
    float birdVelocity = 0.0f;
    int score = 0;
    int nextGapIndex = 0;
    bool gameOver = false;
    bool started = false;

    void registerPipePrefab(PrefabRegistry& prefabs) {
        Prefab pipe;
        pipe.name = "Pipe";
        pipe.transform = TransformComponent{WorldHalfWidth + PipeHalfWidth, 0.0f};
        pipe.sprite = SpriteComponent{PipeHalfWidth, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 20, pipeTexture};
        pipe.collider = ColliderComponent{PipeHalfWidth, 0.5f, false, true};
        pipe.tag = TagComponent{"Pipe"};
        prefabs.registerPrefab(pipe);
    }

    void resetState(GameContext& context) {
        Scene& scene = *context.scene;
        pipes.clear();
        birdY = 0.05f;
        birdVelocity = 0.0f;
        score = 0;
        nextGapIndex = 0;
        gameOver = false;
        started = false;

        createBackdrop(scene);
        createBird(scene);
        createHud(scene);

        for (int index = 0; index < 3; ++index) {
            spawnPipePair(context, 0.75f + static_cast<float>(index) * PipeSpacing);
        }
    }

    void createBackdrop(Scene& scene) {
        scene.createSprite(
            "Sky",
            {0.0f, 0.0f},
            {WorldHalfWidth, WorldHalfHeight, 1.0f, 1.0f, 1.0f, 1.0f, -100, skyTexture},
            "Background");

        scene.createSprite(
            "Ground",
            {0.0f, (GroundTop - WorldHalfHeight) * 0.5f},
            {WorldHalfWidth, (GroundTop + WorldHalfHeight) * 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 30, groundTexture},
            "Ground");
    }

    void createBird(Scene& scene) {
        bird = scene.createSprite(
            "Bird",
            {BirdX, birdY},
            {BirdHalfWidth, BirdHalfHeight, 1.0f, 1.0f, 1.0f, 1.0f, 40, birdTexture},
            "Player");
        scene.setCollider(bird, {BirdHalfWidth, BirdHalfHeight, false, true});
    }

    void createHud(Scene& scene) {
        scoreText = scene.createText(
            "ScoreText",
            {-0.96f, 0.90f},
            {"SCORE 0", 0.045f, 0.070f, 0.010f, 0.030f, 1.0f, 1.0f, 1.0f, 1.0f, 100, true},
            "UI");

        messageText = scene.createText(
            "MessageText",
            {-0.58f, -0.88f},
            {"SPACE TO FLAP", 0.042f, 0.066f, 0.010f, 0.030f, 1.0f, 0.95f, 0.70f, 1.0f, 100, true},
            "UI");
    }

    void spawnPipePair(GameContext& context, float x) {
        PipePair pair;
        pair.x = x;
        pair.gapY = gapYForIndex(nextGapIndex++);
        pair.top = context.instantiatePrefab("Pipe");
        pair.bottom = context.instantiatePrefab("Pipe");
        configurePipePair(*context.scene, pair);
        pipes.push_back(pair);
    }

    void configurePipePair(Scene& scene, const PipePair& pair) {
        const float gapTop = std::min(WorldHalfHeight - 0.12f, pair.gapY + PipeGapHeight * 0.5f);
        const float gapBottom = std::max(GroundTop + 0.12f, pair.gapY - PipeGapHeight * 0.5f);
        const float topHalfHeight = std::max(0.04f, (WorldHalfHeight - gapTop) * 0.5f);
        const float bottomHalfHeight = std::max(0.04f, (gapBottom - GroundTop) * 0.5f);

        configurePipeEntity(scene, pair.top, pair.x, gapTop + topHalfHeight, topHalfHeight);
        configurePipeEntity(scene, pair.bottom, pair.x, GroundTop + bottomHalfHeight, bottomHalfHeight);
    }

    void configurePipeEntity(Scene& scene, Entity entity, float x, float y, float halfHeight) {
        TransformComponent* transform = scene.getTransform(entity);
        SpriteComponent* sprite = scene.getSprite(entity);
        ColliderComponent* collider = scene.getCollider(entity);
        if (transform) {
            transform->x = x;
            transform->y = y;
        }
        if (sprite) {
            sprite->halfHeight = halfHeight;
        }
        if (collider) {
            collider->halfHeight = halfHeight;
        }
    }

    void updateBird(Scene& scene) {
        TransformComponent* transform = scene.getTransform(bird);
        if (transform) {
            transform->x = BirdX;
            transform->y = birdY;
        }
    }

    void updatePipes(Scene& scene, float deltaTime) {
        float rightmostX = -WorldHalfWidth;
        for (const PipePair& pair : pipes) {
            rightmostX = std::max(rightmostX, pair.x);
        }

        for (PipePair& pair : pipes) {
            pair.x -= ScrollSpeed * deltaTime;
            if (pair.x < -WorldHalfWidth - PipeHalfWidth) {
                pair.x = rightmostX + PipeSpacing;
                rightmostX = pair.x;
                pair.gapY = gapYForIndex(nextGapIndex++);
                pair.scored = false;
            }

            configurePipePair(scene, pair);
        }
    }

    void updateScore(Scene& scene) {
        for (PipePair& pair : pipes) {
            if (!pair.scored && pair.x + PipeHalfWidth < BirdX - BirdHalfWidth) {
                pair.scored = true;
                ++score;
            }
        }

        TextComponent* text = scene.getText(scoreText);
        if (text) {
            text->text = "SCORE " + std::to_string(score);
        }
    }

    void checkGameOver(Scene& scene) {
        if (birdY + BirdHalfHeight > WorldHalfHeight || birdY - BirdHalfHeight < GroundTop) {
            endGame(scene);
            return;
        }

        for (const PipePair& pair : pipes) {
            const bool overlapsX = std::abs(BirdX - pair.x) < BirdHalfWidth + PipeHalfWidth;
            if (!overlapsX) {
                continue;
            }

            const float gapTop = pair.gapY + PipeGapHeight * 0.5f;
            const float gapBottom = pair.gapY - PipeGapHeight * 0.5f;
            if (birdY + BirdHalfHeight > gapTop || birdY - BirdHalfHeight < gapBottom) {
                endGame(scene);
                return;
            }
        }
    }

    void endGame(Scene& scene) {
        gameOver = true;
        setMessageText(scene, "GAME OVER  SPACE OR R");
    }

    void setMessageText(Scene& scene, const std::string& value) {
        TextComponent* message = scene.getText(messageText);
        if (message) {
            message->text = value;
        }
    }
};

GameConfig makeDefaultConfig() {
    GameConfig config;
    config.windowTitle = "Flappy Bird Sample";
    config.windowWidth = 800;
    config.windowHeight = 600;
    config.editorEnabled = false;
    config.startPlaying = true;
    config.hotReload = false;
    config.assetRoot = "samples/flappy_bird/assets";
    config.prefabDirectory = "samples/flappy_bird/assets/prefabs";
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

    return runGameApp(std::make_unique<FlappyBirdGame>(), config);
}
