#pragma once

#include <cstddef>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

using Entity = std::size_t;
using TextureHandle = std::size_t;

constexpr Entity InvalidEntity = std::numeric_limits<Entity>::max();
constexpr TextureHandle InvalidTexture = std::numeric_limits<TextureHandle>::max();

struct TransformComponent {
    float x;
    float y;
};

struct NameComponent {
    std::string name;
};

struct TagComponent {
    std::string tag;
};

struct PhysicsComponent {
    float velocityX;
    float velocityY;
    float forceX;
    float forceY;
    float inverseMass;
    float drag;
    float restitution;
    bool isStatic;
    bool usesWorldBounds;
    bool bounceAtBounds;
};

struct SpriteComponent {
    float halfWidth;
    float halfHeight;
    float red;
    float green;
    float blue;
    float alpha;
    int layer;
    TextureHandle texture;
    float uMin = 0.0f;
    float vMin = 0.0f;
    float uMax = 1.0f;
    float vMax = 1.0f;
};

struct SpriteAnimationComponent {
    int columns = 1;
    int rows = 1;
    int firstFrame = 0;
    int frameCount = 1;
    int currentFrame = 0;
    float secondsPerFrame = 0.1f;
    float elapsedSeconds = 0.0f;
    bool playing = true;
    bool loop = true;
};

struct TextComponent {
    std::string text;
    float characterWidth = 0.08f;
    float characterHeight = 0.12f;
    float characterSpacing = 0.02f;
    float lineSpacing = 0.04f;
    float red = 1.0f;
    float green = 1.0f;
    float blue = 1.0f;
    float alpha = 1.0f;
    int layer = 100;
    bool screenSpace = true;
};

struct TilemapComponent {
    int columns = 1;
    int rows = 1;
    float tileWidth = 0.16f;
    float tileHeight = 0.16f;
    int atlasColumns = 1;
    int atlasRows = 1;
    int layer = -100;
    TextureHandle texture = InvalidTexture;
    float red = 1.0f;
    float green = 1.0f;
    float blue = 1.0f;
    float alpha = 1.0f;
    std::vector<int> tiles;
};

struct ColliderComponent {
    float halfWidth;
    float halfHeight;
    bool solid;
    bool trigger = false;
};

struct BoundsComponent {
    float minX;
    float maxX;
    float minY;
    float maxY;
};

struct InputComponent {
    bool enabled;
    std::string moveLeftAction;
    std::string moveRightAction;
    std::string moveUpAction;
    std::string moveDownAction;
    float moveForce;
};

struct ScriptComponent {
    std::string scriptName;
    bool active;
    float elapsedTime;
    std::unordered_map<std::string, float> parameters{};
};

struct LifetimeComponent {
    float remainingSeconds;
};

struct Camera {
    float x;
    float y;
    float halfWidth;
    float halfHeight;
    Entity target;
};
