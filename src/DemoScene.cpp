#include "DemoScene.h"

#include <vector>

namespace {
constexpr float ViewHalfWidth = 1.0f;
constexpr float ViewHalfHeight = 1.0f;
}

void loadDemoScene(Scene& scene, ResourceManager& resources, PrefabRegistry& prefabs) {
    resources.clear();
    scene.clear();
    prefabs.clear();

    const TextureHandle playerTexture = resources.loadTextureFromFile("player_texture", "assets/player.ppm");
    const TextureHandle enemyTexture = resources.createSolidColorTexture("enemy_red", 242, 64, 64, 255);
    const TextureHandle tealTexture = resources.createSolidColorTexture("enemy_teal", 51, 217, 199, 255);
    const TextureHandle greenTexture = resources.createSolidColorTexture("enemy_green", 140, 217, 76, 255);
    const TextureHandle wallTexture = resources.createSolidColorTexture("wall", 217, 217, 242, 255);
    const TextureHandle blockTexture = resources.createSolidColorTexture("block", 190, 135, 240, 255);
    const TextureHandle birdTexture = resources.createSolidColorTexture("flappy_bird", 255, 221, 64, 255);
    const TextureHandle pipeTexture = resources.createSolidColorTexture("flappy_pipe", 76, 196, 91, 255);
    const TextureHandle triggerTexture = resources.createSolidColorTexture("score_trigger", 255, 255, 255, 80);
    const TextureHandle animatedTexture = resources.loadTextureFromFile("animated_square", "assets/animated_square.ppm");
    const TextureHandle tileTexture = resources.loadTextureFromFile("demo_tiles", "assets/tiles.ppm");

    const BoundsComponent worldBounds{-2.0f, 2.0f, -2.0f, 2.0f};
    constexpr int TilemapColumns = 16;
    constexpr int TilemapRows = 16;
    std::vector<int> backgroundTiles(TilemapColumns * TilemapRows, 0);
    for (int row = 0; row < TilemapRows; ++row) {
        for (int column = 0; column < TilemapColumns; ++column) {
            int tile = 0;
            if (row == 0 || row == TilemapRows - 1 || column == 0 || column == TilemapColumns - 1) {
                tile = 1;
            } else if ((row >= 10 && column >= 2 && column <= 5) || (row >= 3 && row <= 6 && column == 12)) {
                tile = 2;
            } else if ((row + column * 2) % 17 == 0) {
                tile = 3;
            }
            backgroundTiles[static_cast<std::size_t>(row * TilemapColumns + column)] = tile;
        }
    }

    prefabs.registerPrefab({
        "DemoTilemap",
        TransformComponent{-2.0f, 2.0f},
        std::nullopt,
        std::nullopt,
        std::nullopt,
        std::nullopt,
        std::nullopt,
        std::nullopt,
        TagComponent{"Tilemap"},
        std::nullopt,
        std::nullopt,
        std::nullopt,
        TilemapComponent{TilemapColumns, TilemapRows, 0.25f, 0.25f, 4, 1, -20, tileTexture, 1.0f, 1.0f, 1.0f, 1.0f, backgroundTiles}
    });

    prefabs.registerPrefab({
        "Player",
        TransformComponent{0.0f, 0.0f},
        PhysicsComponent{0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 8.0f, 0.0f, false, true, false},
        SpriteComponent{0.08f, 0.08f, 1.0f, 1.0f, 1.0f, 1.0f, 10, playerTexture},
        ColliderComponent{0.08f, 0.08f, true},
        worldBounds,
        InputComponent{true, "MoveLeft", "MoveRight", "MoveUp", "MoveDown", 7.2f},
        std::nullopt,
        TagComponent{"Player"},
        std::nullopt
    });

    prefabs.registerPrefab({
        "EnemyRed",
        TransformComponent{-0.85f, 0.45f},
        PhysicsComponent{0.35f, 0.22f, 0.0f, 0.0f, 1.0f, 0.0f, 0.95f, false, true, true},
        SpriteComponent{0.08f, 0.08f, 1.0f, 1.0f, 1.0f, 1.0f, 10, enemyTexture},
        ColliderComponent{0.08f, 0.08f, true},
        worldBounds,
        std::nullopt,
        ScriptComponent{"pulse_alpha", true, 0.0f},
        TagComponent{"Obstacle"},
        std::nullopt
    });

    prefabs.registerPrefab({
        "EnemyTeal",
        TransformComponent{0.90f, -0.55f},
        PhysicsComponent{-0.25f, 0.38f, 0.0f, 0.0f, 1.0f, 0.0f, 0.95f, false, true, true},
        SpriteComponent{0.10f, 0.10f, 1.0f, 1.0f, 1.0f, 1.0f, 10, tealTexture},
        ColliderComponent{0.10f, 0.10f, true},
        worldBounds,
        std::nullopt,
        ScriptComponent{"wander_force", true, 0.0f},
        TagComponent{"Obstacle"},
        std::nullopt
    });

    prefabs.registerPrefab({
        "EnemyGreen",
        TransformComponent{1.35f, 0.75f},
        PhysicsComponent{-0.42f, -0.18f, 0.0f, 0.0f, 1.0f, 0.0f, 0.95f, false, true, true},
        SpriteComponent{0.06f, 0.06f, 1.0f, 1.0f, 1.0f, 1.0f, 10, greenTexture},
        ColliderComponent{0.06f, 0.06f, true},
        worldBounds,
        std::nullopt,
        std::nullopt,
        TagComponent{"Obstacle"},
        std::nullopt
    });

    prefabs.registerPrefab({
        "Block",
        TransformComponent{0.45f, 0.05f},
        PhysicsComponent{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, true, false, false},
        SpriteComponent{0.16f, 0.22f, 1.0f, 1.0f, 1.0f, 1.0f, 5, blockTexture},
        ColliderComponent{0.16f, 0.22f, true},
        std::nullopt,
        std::nullopt,
        std::nullopt,
        TagComponent{"Obstacle"},
        std::nullopt
    });

    prefabs.registerPrefab({
        "HorizontalWall",
        TransformComponent{0.0f, 2.03f},
        std::nullopt,
        SpriteComponent{2.05f, 0.03f, 1.0f, 1.0f, 1.0f, 1.0f, 0, wallTexture},
        ColliderComponent{2.05f, 0.03f, true},
        std::nullopt,
        std::nullopt,
        std::nullopt,
        TagComponent{"Wall"},
        std::nullopt
    });

    prefabs.registerPrefab({
        "VerticalWall",
        TransformComponent{2.03f, 0.0f},
        std::nullopt,
        SpriteComponent{0.03f, 2.05f, 1.0f, 1.0f, 1.0f, 1.0f, 0, wallTexture},
        ColliderComponent{0.03f, 2.05f, true},
        std::nullopt,
        std::nullopt,
        std::nullopt,
        TagComponent{"Wall"},
        std::nullopt
    });

    prefabs.registerPrefab({
        "FlappyBird",
        TransformComponent{-0.45f, 0.0f},
        PhysicsComponent{0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, false, true, false},
        SpriteComponent{0.08f, 0.06f, 1.0f, 1.0f, 1.0f, 1.0f, 20, birdTexture},
        ColliderComponent{0.07f, 0.055f, true},
        BoundsComponent{-1.0f, 1.0f, -0.95f, 0.95f},
        std::nullopt,
        ScriptComponent{"flappy_bird", true, 0.0f},
        TagComponent{"Player"},
        std::nullopt
    });

    prefabs.registerPrefab({
        "FlappyPipeTop",
        TransformComponent{1.55f, 0.82f},
        PhysicsComponent{-0.82f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, false, false, false},
        SpriteComponent{0.12f, 0.72f, 1.0f, 1.0f, 1.0f, 1.0f, 8, pipeTexture},
        ColliderComponent{0.12f, 0.72f, true},
        std::nullopt,
        std::nullopt,
        ScriptComponent{"flappy_pipe_mover", true, 0.0f},
        TagComponent{"Obstacle"},
        LifetimeComponent{4.4f}
    });

    prefabs.registerPrefab({
        "FlappyPipeBottom",
        TransformComponent{1.55f, -0.82f},
        PhysicsComponent{-0.82f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, false, false, false},
        SpriteComponent{0.12f, 0.72f, 1.0f, 1.0f, 1.0f, 1.0f, 8, pipeTexture},
        ColliderComponent{0.12f, 0.72f, true},
        std::nullopt,
        std::nullopt,
        ScriptComponent{"flappy_pipe_mover", true, 0.0f},
        TagComponent{"Obstacle"},
        LifetimeComponent{4.4f}
    });

    prefabs.registerPrefab({
        "FlappyScoreZone",
        TransformComponent{1.55f, 0.0f},
        PhysicsComponent{-0.82f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, false, false, false},
        SpriteComponent{0.02f, 0.28f, 1.0f, 1.0f, 1.0f, 0.22f, 7, triggerTexture},
        ColliderComponent{0.02f, 0.28f, false, true},
        std::nullopt,
        std::nullopt,
        ScriptComponent{"flappy_pipe_mover", true, 0.0f},
        TagComponent{"ScoreZone"},
        LifetimeComponent{4.4f}
    });

    prefabs.registerPrefab({
        "FlappyPipeSpawner",
        TransformComponent{0.0f, 0.0f},
        std::nullopt,
        std::nullopt,
        std::nullopt,
        std::nullopt,
        std::nullopt,
        ScriptComponent{"flappy_pipe_spawner", true, 1.1f},
        TagComponent{"Spawner"},
        std::nullopt
    });

    prefabs.registerPrefab({
        "AnimatedSquare",
        TransformComponent{-1.35f, -0.85f},
        std::nullopt,
        SpriteComponent{0.10f, 0.10f, 1.0f, 1.0f, 1.0f, 1.0f, 15, animatedTexture, 0.0f, 0.0f, 0.25f, 1.0f},
        std::nullopt,
        std::nullopt,
        std::nullopt,
        std::nullopt,
        TagComponent{"Animated"},
        std::nullopt,
        SpriteAnimationComponent{4, 1, 0, 4, 0, 0.16f, 0.0f, true, true}
    });

    prefabs.registerPrefab({
        "ScoreHud",
        TransformComponent{-0.94f, 0.88f},
        std::nullopt,
        std::nullopt,
        std::nullopt,
        std::nullopt,
        std::nullopt,
        std::nullopt,
        TagComponent{"ScoreHud"},
        std::nullopt,
        std::nullopt,
        TextComponent{"SCORE 0", 0.055f, 0.085f, 0.010f, 0.030f, 1.0f, 0.96f, 0.50f, 1.0f, 120, true}
    });

    const Entity player = prefabs.instantiate(scene, "Player");
    scene.setPlayerEntity(player);
    scene.setCamera({0.0f, 0.0f, ViewHalfWidth, ViewHalfHeight, player});

    prefabs.instantiate(scene, "DemoTilemap");
    prefabs.instantiate(scene, "EnemyRed");
    prefabs.instantiate(scene, "EnemyTeal");
    prefabs.instantiate(scene, "EnemyGreen");
    prefabs.instantiate(scene, "Block");
    prefabs.instantiate(scene, "AnimatedSquare");
    prefabs.instantiate(scene, "ScoreHud");
    prefabs.instantiate(scene, "HorizontalWall", {0.0f, 2.03f});
    prefabs.instantiate(scene, "HorizontalWall", {0.0f, -2.03f});
    prefabs.instantiate(scene, "VerticalWall", {2.03f, 0.0f});
    prefabs.instantiate(scene, "VerticalWall", {-2.03f, 0.0f});
}
