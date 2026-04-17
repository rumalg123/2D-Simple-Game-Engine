#pragma once

#include "Event.h"
#include "JobSystem.h"
#include "Scene.h"

#include <cstdint>
#include <unordered_set>
#include <vector>

struct PhysicsSettings {
    bool emitStayEvents = true;
    bool emitExitEvents = true;
};

class PhysicsSystem {
public:
    void update(Scene& scene, EventQueue& events, float deltaTime, JobSystem* jobSystem = nullptr);
    std::vector<Entity> queryAabb(
        const Scene& scene,
        const TransformComponent& transform,
        const ColliderComponent& collider) const;
    void setSettings(PhysicsSettings nextSettings);
    const PhysicsSettings& getSettings() const;
    void clearContacts();

private:
    void integrate(Scene& scene, float deltaTime, JobSystem* jobSystem);
    void resolveWorldBounds(Scene& scene);
    void resolveCollisions(Scene& scene, EventQueue& events);

    PhysicsSettings settings;
    std::unordered_set<std::uint64_t> previousCollisionPairs;
    std::unordered_set<std::uint64_t> previousTriggerPairs;
};
