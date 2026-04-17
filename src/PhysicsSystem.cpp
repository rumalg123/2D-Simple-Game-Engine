#include "PhysicsSystem.h"

#include <cmath>
#include <vector>

namespace {
float maxValue(float first, float second) {
    return first > second ? first : second;
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
}

void PhysicsSystem::update(Scene& scene, EventQueue& events, float deltaTime, JobSystem* jobSystem) {
    integrate(scene, deltaTime, jobSystem);
    resolveWorldBounds(scene);
    resolveCollisions(scene, events);
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

            if (!firstTransform || !secondTransform ||
                !overlaps(*firstTransform, firstCollider, *secondTransform, secondCollider)) {
                continue;
            }

            const bool isTrigger = firstCollider.trigger || secondCollider.trigger ||
                                   !firstCollider.solid || !secondCollider.solid;
            if (isTrigger) {
                events.publishTrigger(first, second);
                continue;
            }

            PhysicsComponent* firstPhysics = scene.getPhysics(first);
            PhysicsComponent* secondPhysics = scene.getPhysics(second);
            if (!firstPhysics || !secondPhysics || (firstPhysics->isStatic && secondPhysics->isStatic)) {
                continue;
            }

            events.publishCollision(first, second);

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
}
