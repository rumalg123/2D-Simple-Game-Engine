#include "Prefab.h"

#include <algorithm>
#include <stdexcept>

void PrefabRegistry::clear() {
    prefabs.clear();
}

void PrefabRegistry::registerPrefab(Prefab prefab) {
    prefabs[prefab.name] = prefab;
}

bool PrefabRegistry::hasPrefab(const std::string& name) const {
    return prefabs.find(name) != prefabs.end();
}

Entity PrefabRegistry::instantiate(Scene& scene, const std::string& name) const {
    const Prefab* prefab = getPrefab(name);
    if (!prefab) {
        throw std::runtime_error("Unknown prefab: " + name);
    }

    return instantiatePrefab(scene, *prefab, std::nullopt);
}

Entity PrefabRegistry::instantiate(Scene& scene, const std::string& name, TransformComponent transformOverride) const {
    const Prefab* prefab = getPrefab(name);
    if (!prefab) {
        throw std::runtime_error("Unknown prefab: " + name);
    }

    return instantiatePrefab(scene, *prefab, transformOverride);
}

const Prefab* PrefabRegistry::getPrefab(const std::string& name) const {
    const auto existing = prefabs.find(name);
    if (existing == prefabs.end()) {
        return nullptr;
    }

    return &existing->second;
}

std::vector<std::string> PrefabRegistry::getPrefabNames() const {
    std::vector<std::string> names;
    names.reserve(prefabs.size());
    for (const auto& entry : prefabs) {
        names.push_back(entry.first);
    }

    std::sort(names.begin(), names.end());
    return names;
}

std::size_t PrefabRegistry::prefabCount() const {
    return prefabs.size();
}

Entity PrefabRegistry::instantiatePrefab(
    Scene& scene,
    const Prefab& prefab,
    const std::optional<TransformComponent>& transformOverride) const {
    const Entity entity = scene.createEntity();
    scene.setName(entity, {prefab.name});

    if (transformOverride) {
        scene.setTransform(entity, *transformOverride);
    } else if (prefab.transform) {
        scene.setTransform(entity, *prefab.transform);
    }

    if (prefab.physics) {
        scene.setPhysics(entity, *prefab.physics);
    }
    if (prefab.sprite) {
        scene.setSprite(entity, *prefab.sprite);
    }
    if (prefab.spriteAnimation) {
        scene.setSpriteAnimation(entity, *prefab.spriteAnimation);
    }
    if (prefab.text) {
        scene.setText(entity, *prefab.text);
    }
    if (prefab.tilemap) {
        scene.setTilemap(entity, *prefab.tilemap);
    }
    if (prefab.collider) {
        scene.setCollider(entity, *prefab.collider);
    }
    if (prefab.bounds) {
        scene.setBounds(entity, *prefab.bounds);
    }
    if (prefab.input) {
        scene.setInput(entity, *prefab.input);
    }
    if (prefab.script) {
        scene.setScript(entity, *prefab.script);
    }
    if (prefab.tag) {
        scene.setTag(entity, *prefab.tag);
    }
    if (prefab.lifetime) {
        scene.setLifetime(entity, *prefab.lifetime);
    }

    return entity;
}
