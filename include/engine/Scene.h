#pragma once

#include "ComponentPool.h"
#include "ECS.h"

#include <vector>

class Scene {
public:
    void clear();
    Entity createEntity();
    Entity createEntity(std::string name, TransformComponent transform = {0.0f, 0.0f});
    Entity createSprite(
        std::string name,
        TransformComponent transform,
        SpriteComponent sprite,
        std::string tag = {});
    Entity createText(
        std::string name,
        TransformComponent transform,
        TextComponent text,
        std::string tag = {});
    Entity createCollider(
        std::string name,
        TransformComponent transform,
        ColliderComponent collider,
        std::string tag = {});
    void destroyEntity(Entity entity);

    Entity entityCount() const;
    Entity livingEntityCount() const;
    bool isValidEntity(Entity entity) const;
    Entity findEntityByName(const std::string& name) const;
    Entity findEntityByTag(const std::string& tag) const;
    std::vector<Entity> findEntitiesByTag(const std::string& tag) const;

    void setName(Entity entity, NameComponent name);
    void setTag(Entity entity, TagComponent tag);
    void setTransform(Entity entity, TransformComponent transform);
    void setPhysics(Entity entity, PhysicsComponent physics);
    void setSprite(Entity entity, SpriteComponent sprite);
    void setSpriteAnimation(Entity entity, SpriteAnimationComponent animation);
    void setText(Entity entity, TextComponent text);
    void setTilemap(Entity entity, TilemapComponent tilemap);
    void setCollider(Entity entity, ColliderComponent collider);
    void setBounds(Entity entity, BoundsComponent bounds);
    void setInput(Entity entity, InputComponent input);
    void setScript(Entity entity, ScriptComponent script);
    void setLifetime(Entity entity, LifetimeComponent lifetime);

    NameComponent* getName(Entity entity);
    TagComponent* getTag(Entity entity);
    TransformComponent* getTransform(Entity entity);
    PhysicsComponent* getPhysics(Entity entity);
    SpriteComponent* getSprite(Entity entity);
    SpriteAnimationComponent* getSpriteAnimation(Entity entity);
    TextComponent* getText(Entity entity);
    TilemapComponent* getTilemap(Entity entity);
    ColliderComponent* getCollider(Entity entity);
    BoundsComponent* getBounds(Entity entity);
    InputComponent* getInput(Entity entity);
    ScriptComponent* getScript(Entity entity);
    LifetimeComponent* getLifetime(Entity entity);

    const NameComponent* getName(Entity entity) const;
    const TagComponent* getTag(Entity entity) const;
    const TransformComponent* getTransform(Entity entity) const;
    const PhysicsComponent* getPhysics(Entity entity) const;
    const SpriteComponent* getSprite(Entity entity) const;
    const SpriteAnimationComponent* getSpriteAnimation(Entity entity) const;
    const TextComponent* getText(Entity entity) const;
    const TilemapComponent* getTilemap(Entity entity) const;
    const ColliderComponent* getCollider(Entity entity) const;
    const BoundsComponent* getBounds(Entity entity) const;
    const InputComponent* getInput(Entity entity) const;
    const ScriptComponent* getScript(Entity entity) const;
    const LifetimeComponent* getLifetime(Entity entity) const;

    ComponentPool<NameComponent>& getNamePool();
    ComponentPool<TagComponent>& getTagPool();
    ComponentPool<TransformComponent>& getTransformPool();
    ComponentPool<PhysicsComponent>& getPhysicsPool();
    ComponentPool<SpriteComponent>& getSpritePool();
    ComponentPool<SpriteAnimationComponent>& getSpriteAnimationPool();
    ComponentPool<TextComponent>& getTextPool();
    ComponentPool<TilemapComponent>& getTilemapPool();
    ComponentPool<ColliderComponent>& getColliderPool();
    ComponentPool<BoundsComponent>& getBoundsPool();
    ComponentPool<InputComponent>& getInputPool();
    ComponentPool<ScriptComponent>& getScriptPool();
    ComponentPool<LifetimeComponent>& getLifetimePool();

    const ComponentPool<NameComponent>& getNamePool() const;
    const ComponentPool<TagComponent>& getTagPool() const;
    const ComponentPool<TransformComponent>& getTransformPool() const;
    const ComponentPool<PhysicsComponent>& getPhysicsPool() const;
    const ComponentPool<SpriteComponent>& getSpritePool() const;
    const ComponentPool<SpriteAnimationComponent>& getSpriteAnimationPool() const;
    const ComponentPool<TextComponent>& getTextPool() const;
    const ComponentPool<TilemapComponent>& getTilemapPool() const;
    const ComponentPool<ColliderComponent>& getColliderPool() const;
    const ComponentPool<BoundsComponent>& getBoundsPool() const;
    const ComponentPool<InputComponent>& getInputPool() const;
    const ComponentPool<ScriptComponent>& getScriptPool() const;
    const ComponentPool<LifetimeComponent>& getLifetimePool() const;

    Camera& getCamera();
    const Camera& getCamera() const;
    void setCamera(Camera nextCamera);

    Entity getPlayerEntity() const;
    void setPlayerEntity(Entity entity);

private:
    void ensureEntityInRange(Entity entity) const;
    void reserveComponentSlots();

    Entity nextEntity = 0;
    std::vector<bool> alive;
    ComponentPool<NameComponent> names;
    ComponentPool<TagComponent> tags;
    ComponentPool<TransformComponent> transforms;
    ComponentPool<PhysicsComponent> physicsBodies;
    ComponentPool<SpriteComponent> sprites;
    ComponentPool<SpriteAnimationComponent> spriteAnimations;
    ComponentPool<TextComponent> texts;
    ComponentPool<TilemapComponent> tilemaps;
    ComponentPool<ColliderComponent> colliders;
    ComponentPool<BoundsComponent> bounds;
    ComponentPool<InputComponent> inputs;
    ComponentPool<ScriptComponent> scripts;
    ComponentPool<LifetimeComponent> lifetimes;
    Entity playerEntity = InvalidEntity;
    Camera camera{0.0f, 0.0f, 1.0f, 1.0f, InvalidEntity};
};
