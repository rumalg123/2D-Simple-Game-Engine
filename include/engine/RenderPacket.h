#pragma once

#include "ECS.h"

#include <cstddef>
#include <vector>

struct RenderTextureMetadata {
    TextureHandle handle = InvalidTexture;
    unsigned int version = 0;
    int width = 0;
    int height = 0;
};

struct RenderSpriteCommand {
    Entity entity = InvalidEntity;
    TransformComponent transform{};
    SpriteComponent sprite{};
};

struct RenderColliderCommand {
    Entity entity = InvalidEntity;
    TransformComponent transform{};
    ColliderComponent collider{};
};

struct RenderPacket {
    std::size_t frameIndex = 0;
    Camera camera{0.0f, 0.0f, 1.0f, 1.0f, InvalidEntity};
    std::vector<RenderSpriteCommand> sprites;
    std::vector<RenderColliderCommand> colliders;
    std::vector<RenderTextureMetadata> textures;
};
