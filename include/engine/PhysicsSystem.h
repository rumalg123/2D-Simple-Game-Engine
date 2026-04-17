#pragma once

#include "Event.h"
#include "JobSystem.h"
#include "Scene.h"

class PhysicsSystem {
public:
    void update(Scene& scene, EventQueue& events, float deltaTime, JobSystem* jobSystem = nullptr);

private:
    void integrate(Scene& scene, float deltaTime, JobSystem* jobSystem);
    void resolveWorldBounds(Scene& scene);
    void resolveCollisions(Scene& scene, EventQueue& events);
};
