#include "PhysicsSystem.h"

#include <cmath>
#include <cstdint>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
struct ContactPair {
    Entity first = InvalidEntity;
    Entity second = InvalidEntity;
};

float maxValue(float first, float second) {
    return first > second ? first : second;
}

std::uint64_t pairKey(Entity first, Entity second) {
    if (first > second) {
        std::swap(first, second);
    }
    return (static_cast<std::uint64_t>(first) << 32u) ^ static_cast<std::uint64_t>(second);
}

ContactPair pairFromKey(std::uint64_t key) {
    return {
        static_cast<Entity>(key >> 32u),
        static_cast<Entity>(key & 0xFFFFFFFFu)
    };
}

bool layersCanCollide(const ColliderComponent& first, const ColliderComponent& second) {
    return (first.mask & second.layer) != 0u && (second.mask & first.layer) != 0u;
}

bool overlaps(
    const TransformComponent& firstTransform,
    const ColliderComponent& firstCollider,
    const TransformComponent& secondTransform,
    const ColliderComponent& secondCollider) {
    return std::abs(firstTransform.x - secondTransform.x) <
               (firstCollider.halfWidth + secondCollider.halfWidth) &&
           std::abs(firstTransform.y - secondTransform.y) <
               (firstCollider.halfHeight + secondCollider.halfHeight);
}

CollisionPhase phaseForPair(const std::unordered_set<std::uint64_t>& previousPairs, std::uint64_t key) {
    return previousPairs.find(key) == previousPairs.end() ? CollisionPhase::Enter : CollisionPhase::Stay;
}
}

void PhysicsSystem::update(Scene& scene, EventQueue& events, float deltaTime, JobSystem* jobSystem) {
    integrate(scene, deltaTime, jobSystem);
    resolveWorldBounds(scene);
    resolveCollisions(scene, events);
}

std::vector<Entity> PhysicsSystem::queryAabb(
    const Scene& scene,
    const TransformComponent& transform,
    const ColliderComponent& collider) const {
    const ComponentPool<ColliderComponent>& colliders = scene.getColliderPool();
    std::vector<Entity> hits;

    for (std::size_t index = 0; index < colliders.size(); ++index) {
        const Entity entity = colliders.entityAt(index);
        const TransformComponent* otherTransform = scene.getTransform(entity);
        const ColliderComponent& otherCollider = colliders.componentAt(index);
        if (!otherTransform || !layersCanCollide(collider, otherCollider)) {
            continue;
        }

        if (overlaps(transform, collider, *otherTransform, otherCollider)) {
            hits.push_back(entity);
        }
    }

    return hits;
}

void PhysicsSystem::setSettings(PhysicsSettings nextSettings) {
    settings = nextSettings;
}

const PhysicsSettings& PhysicsSystem::getSettings() const {
    return settings;
}

void PhysicsSystem::clearContacts() {
    previousCollisionPairs.clear();
    previousTriggerPairs.clear();
}

void PhysicsSystem::integrate(Scene& scene, float deltaTime, JobSystem* jobSystem) {
    ComponentPool<PhysicsComponent>& physicsBodies = scene.getPhysicsPool();

    auto integrateRange = [&scene, &physicsBodies, deltaTime](std::size_t begin, std::size_t end) {
        for (std::size_t index = begin; index < end; ++index) {
            const Entity entity = physicsBodies.entityAt(index);
            TransformComponent* transform = scene.getTransform(entity);
            if (!transform) {
                continue;
            }

            PhysicsComponent& physics = physicsBodies.componentAt(index);
            if (physics.isStatic || physics.inverseMass <= 0.0f) {
                physics.forceX = 0.0f;
                physics.forceY = 0.0f;
                continue;
            }

            physics.velocityX += physics.forceX * physics.inverseMass * deltaTime;
            physics.velocityY += physics.forceY * physics.inverseMass * deltaTime;

            const float damping = maxValue(0.0f, 1.0f - physics.drag * deltaTime);
            physics.velocityX *= damping;
            physics.velocityY *= damping;

            transform->x += physics.velocityX * deltaTime;
            transform->y += physics.velocityY * deltaTime;

            physics.forceX = 0.0f;
            physics.forceY = 0.0f;
        }
    };

    if (jobSystem && physicsBodies.size() >= 64) {
        jobSystem->parallelFor(physicsBodies.size(), 32, integrateRange);
    } else {
        integrateRange(0, physicsBodies.size());
    }
}

void PhysicsSystem::resolveWorldBounds(Scene& scene) {
    ComponentPool<PhysicsComponent>& physicsBodies = scene.getPhysicsPool();

    for (std::size_t index = 0; index < physicsBodies.size(); ++index) {
        const Entity entity = physicsBodies.entityAt(index);
        TransformComponent* transform = scene.getTransform(entity);
        ColliderComponent* collider = scene.getCollider(entity);
        BoundsComponent* entityBounds = scene.getBounds(entity);
        if (!transform || !collider || !entityBounds) {
            continue;
        }

        PhysicsComponent& physics = physicsBodies.componentAt(index);
        if (!physics.usesWorldBounds || physics.isStatic) {
            continue;
        }

        if (transform->x - collider->halfWidth < entityBounds->minX) {
            transform->x = entityBounds->minX + collider->halfWidth;
            physics.velocityX = physics.bounceAtBounds ? std::abs(physics.velocityX) * physics.restitution : 0.0f;
        }

        if (transform->x + collider->halfWidth > entityBounds->maxX) {
            transform->x = entityBounds->maxX - collider->halfWidth;
            physics.velocityX = physics.bounceAtBounds ? -std::abs(physics.velocityX) * physics.restitution : 0.0f;
        }

        if (transform->y - collider->halfHeight < entityBounds->minY) {
            transform->y = entityBounds->minY + collider->halfHeight;
            physics.velocityY = physics.bounceAtBounds ? std::abs(physics.velocityY) * physics.restitution : 0.0f;
        }

        if (transform->y + collider->halfHeight > entityBounds->maxY) {
            transform->y = entityBounds->maxY - collider->halfHeight;
            physics.velocityY = physics.bounceAtBounds ? -std::abs(physics.velocityY) * physics.restitution : 0.0f;
        }
    }
}

void PhysicsSystem::resolveCollisions(Scene& scene, EventQueue& events) {
    const ComponentPool<ColliderComponent>& colliders = scene.getColliderPool();
    std::vector<Entity> collisionEntities;
    collisionEntities.reserve(colliders.size());
    std::unordered_set<std::uint64_t> currentCollisionPairs;
    std::unordered_set<std::uint64_t> currentTriggerPairs;

    for (std::size_t index = 0; index < colliders.size(); ++index) {
        const Entity entity = colliders.entityAt(index);
        if (scene.getTransform(entity)) {
            collisionEntities.push_back(entity);
        }
    }

    for (std::size_t firstIndex = 0; firstIndex < collisionEntities.size(); ++firstIndex) {
        const Entity first = collisionEntities[firstIndex];

        for (std::size_t secondIndex = firstIndex + 1; secondIndex < collisionEntities.size(); ++secondIndex) {
            const Entity second = collisionEntities[secondIndex];

            TransformComponent* firstTransform = scene.getTransform(first);
            TransformComponent* secondTransform = scene.getTransform(second);
            const ColliderComponent& firstCollider = *scene.getCollider(first);
            const ColliderComponent& secondCollider = *scene.getCollider(second);

            if (!firstTransform || !secondTransform || !layersCanCollide(firstCollider, secondCollider) ||
                !overlaps(*firstTransform, firstCollider, *secondTransform, secondCollider)) {
                continue;
            }

            const std::uint64_t key = pairKey(first, second);
            const bool isTrigger = firstCollider.trigger || secondCollider.trigger ||
                                   !firstCollider.solid || !secondCollider.solid;
            if (isTrigger) {
                currentTriggerPairs.insert(key);
                const CollisionPhase phase = phaseForPair(previousTriggerPairs, key);
                if (phase == CollisionPhase::Enter || settings.emitStayEvents) {
                    events.publishTrigger(first, second, phase);
                }
                continue;
            }

            PhysicsComponent* firstPhysics = scene.getPhysics(first);
            PhysicsComponent* secondPhysics = scene.getPhysics(second);
            if (!firstPhysics || !secondPhysics || (firstPhysics->isStatic && secondPhysics->isStatic)) {
                continue;
            }

            currentCollisionPairs.insert(key);
            const CollisionPhase phase = phaseForPair(previousCollisionPairs, key);
            if (phase == CollisionPhase::Enter || settings.emitStayEvents) {
                events.publishCollision(first, second, phase);
            }

            const float deltaX = firstTransform->x - secondTransform->x;
            const float deltaY = firstTransform->y - secondTransform->y;
            const float overlapX = firstCollider.halfWidth + secondCollider.halfWidth - std::abs(deltaX);
            const float overlapY = firstCollider.halfHeight + secondCollider.halfHeight - std::abs(deltaY);
            const float firstMoveShare = secondPhysics->isStatic ? 1.0f : 0.5f;
            const float secondMoveShare = firstPhysics->isStatic ? 1.0f : 0.5f;

            if (overlapX < overlapY) {
                const float direction = deltaX < 0.0f ? -1.0f : 1.0f;

                if (!firstPhysics->isStatic) {
                    firstTransform->x += direction * overlapX * firstMoveShare;
                    firstPhysics->velocityX =
                        direction * std::abs(firstPhysics->velocityX) * firstPhysics->restitution;
                }

                if (!secondPhysics->isStatic) {
                    secondTransform->x -= direction * overlapX * secondMoveShare;
                    secondPhysics->velocityX =
                        -direction * std::abs(secondPhysics->velocityX) * secondPhysics->restitution;
                }
            } else {
                const float direction = deltaY < 0.0f ? -1.0f : 1.0f;

                if (!firstPhysics->isStatic) {
                    firstTransform->y += direction * overlapY * firstMoveShare;
                    firstPhysics->velocityY =
                        direction * std::abs(firstPhysics->velocityY) * firstPhysics->restitution;
                }

                if (!secondPhysics->isStatic) {
                    secondTransform->y -= direction * overlapY * secondMoveShare;
                    secondPhysics->velocityY =
                        -direction * std::abs(secondPhysics->velocityY) * secondPhysics->restitution;
                }
            }
        }
    }

    if (settings.emitExitEvents) {
        for (std::uint64_t key : previousCollisionPairs) {
            if (currentCollisionPairs.find(key) != currentCollisionPairs.end()) {
                continue;
            }

            const ContactPair pair = pairFromKey(key);
            if (scene.isValidEntity(pair.first) && scene.isValidEntity(pair.second)) {
                events.publishCollision(pair.first, pair.second, CollisionPhase::Exit);
            }
        }

        for (std::uint64_t key : previousTriggerPairs) {
            if (currentTriggerPairs.find(key) != currentTriggerPairs.end()) {
                continue;
            }

            const ContactPair pair = pairFromKey(key);
            if (scene.isValidEntity(pair.first) && scene.isValidEntity(pair.second)) {
                events.publishTrigger(pair.first, pair.second, CollisionPhase::Exit);
            }
        }
    }

    previousCollisionPairs = std::move(currentCollisionPairs);
    previousTriggerPairs = std::move(currentTriggerPairs);
}
