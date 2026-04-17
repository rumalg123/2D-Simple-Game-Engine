#pragma once

#include "Scene.h"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct Prefab {
    std::string name;
    std::optional<TransformComponent> transform;
    std::optional<PhysicsComponent> physics;
    std::optional<SpriteComponent> sprite;
    std::optional<ColliderComponent> collider;
    std::optional<BoundsComponent> bounds;
    std::optional<InputComponent> input;
    std::optional<ScriptComponent> script;
    std::optional<TagComponent> tag;
    std::optional<LifetimeComponent> lifetime;
    std::optional<SpriteAnimationComponent> spriteAnimation;
    std::optional<TextComponent> text;
    std::optional<TilemapComponent> tilemap;
};

class PrefabRegistry {
public:
    void clear();
    void registerPrefab(Prefab prefab);
    bool hasPrefab(const std::string& name) const;
    Entity instantiate(Scene& scene, const std::string& name) const;
    Entity instantiate(Scene& scene, const std::string& name, TransformComponent transformOverride) const;
    const Prefab* getPrefab(const std::string& name) const;
    std::vector<std::string> getPrefabNames() const;
    std::size_t prefabCount() const;

private:
    Entity instantiatePrefab(Scene& scene, const Prefab& prefab, const std::optional<TransformComponent>& transformOverride) const;

    std::unordered_map<std::string, Prefab> prefabs;
};
