#include "Scene.h"

#include <stdexcept>
#include <utility>

void Scene::clear() {
    nextEntity = 0;
    alive.clear();
    names.clear();
    tags.clear();
    transforms.clear();
    physicsBodies.clear();
    sprites.clear();
    spriteAnimations.clear();
    texts.clear();
    tilemaps.clear();
    colliders.clear();
    bounds.clear();
    inputs.clear();
    scripts.clear();
    lifetimes.clear();
    playerEntity = InvalidEntity;
    camera = {0.0f, 0.0f, 1.0f, 1.0f, InvalidEntity};
}

Entity Scene::createEntity() {
    const Entity entity = nextEntity++;
    alive.push_back(true);
    reserveComponentSlots();
    return entity;
}

void Scene::destroyEntity(Entity entity) {
    if (!isValidEntity(entity)) {
        return;
    }

    names.remove(entity);
    tags.remove(entity);
    transforms.remove(entity);
    physicsBodies.remove(entity);
    sprites.remove(entity);
    spriteAnimations.remove(entity);
    texts.remove(entity);
    tilemaps.remove(entity);
    colliders.remove(entity);
    bounds.remove(entity);
    inputs.remove(entity);
    scripts.remove(entity);
    lifetimes.remove(entity);
    alive[entity] = false;

    if (playerEntity == entity) {
        playerEntity = InvalidEntity;
    }
    if (camera.target == entity) {
        camera.target = InvalidEntity;
    }
}

Entity Scene::entityCount() const {
    return nextEntity;
}

Entity Scene::livingEntityCount() const {
    Entity count = 0;
    for (bool entityAlive : alive) {
        if (entityAlive) {
            ++count;
        }
    }

    return count;
}

bool Scene::isValidEntity(Entity entity) const {
    return entity < alive.size() && alive[entity];
}

void Scene::setName(Entity entity, NameComponent name) {
    ensureEntityInRange(entity);
    names.set(entity, std::move(name));
}

void Scene::setTag(Entity entity, TagComponent tag) {
    ensureEntityInRange(entity);
    tags.set(entity, std::move(tag));
}

void Scene::setTransform(Entity entity, TransformComponent transform) {
    ensureEntityInRange(entity);
    transforms.set(entity, transform);
}

void Scene::setPhysics(Entity entity, PhysicsComponent physics) {
    ensureEntityInRange(entity);
    physicsBodies.set(entity, physics);
}

void Scene::setSprite(Entity entity, SpriteComponent sprite) {
    ensureEntityInRange(entity);
    sprites.set(entity, sprite);
}

void Scene::setSpriteAnimation(Entity entity, SpriteAnimationComponent animation) {
    ensureEntityInRange(entity);
    spriteAnimations.set(entity, animation);
}

void Scene::setText(Entity entity, TextComponent text) {
    ensureEntityInRange(entity);
    texts.set(entity, std::move(text));
}

void Scene::setTilemap(Entity entity, TilemapComponent tilemap) {
    ensureEntityInRange(entity);
    tilemaps.set(entity, std::move(tilemap));
}

void Scene::setCollider(Entity entity, ColliderComponent collider) {
    ensureEntityInRange(entity);
    colliders.set(entity, collider);
}

void Scene::setBounds(Entity entity, BoundsComponent entityBounds) {
    ensureEntityInRange(entity);
    bounds.set(entity, entityBounds);
}

void Scene::setInput(Entity entity, InputComponent input) {
    ensureEntityInRange(entity);
    inputs.set(entity, input);
}

void Scene::setScript(Entity entity, ScriptComponent script) {
    ensureEntityInRange(entity);
    scripts.set(entity, script);
}

void Scene::setLifetime(Entity entity, LifetimeComponent lifetime) {
    ensureEntityInRange(entity);
    lifetimes.set(entity, lifetime);
}

NameComponent* Scene::getName(Entity entity) {
    return names.get(entity);
}

TagComponent* Scene::getTag(Entity entity) {
    return tags.get(entity);
}

TransformComponent* Scene::getTransform(Entity entity) {
    return transforms.get(entity);
}

PhysicsComponent* Scene::getPhysics(Entity entity) {
    return physicsBodies.get(entity);
}

SpriteComponent* Scene::getSprite(Entity entity) {
    return sprites.get(entity);
}

SpriteAnimationComponent* Scene::getSpriteAnimation(Entity entity) {
    return spriteAnimations.get(entity);
}

TextComponent* Scene::getText(Entity entity) {
    return texts.get(entity);
}

TilemapComponent* Scene::getTilemap(Entity entity) {
    return tilemaps.get(entity);
}

ColliderComponent* Scene::getCollider(Entity entity) {
    return colliders.get(entity);
}

BoundsComponent* Scene::getBounds(Entity entity) {
    return bounds.get(entity);
}

InputComponent* Scene::getInput(Entity entity) {
    return inputs.get(entity);
}

ScriptComponent* Scene::getScript(Entity entity) {
    return scripts.get(entity);
}

LifetimeComponent* Scene::getLifetime(Entity entity) {
    return lifetimes.get(entity);
}

const NameComponent* Scene::getName(Entity entity) const {
    return names.get(entity);
}

const TagComponent* Scene::getTag(Entity entity) const {
    return tags.get(entity);
}

const TransformComponent* Scene::getTransform(Entity entity) const {
    return transforms.get(entity);
}

const PhysicsComponent* Scene::getPhysics(Entity entity) const {
    return physicsBodies.get(entity);
}

const SpriteComponent* Scene::getSprite(Entity entity) const {
    return sprites.get(entity);
}

const SpriteAnimationComponent* Scene::getSpriteAnimation(Entity entity) const {
    return spriteAnimations.get(entity);
}

const TextComponent* Scene::getText(Entity entity) const {
    return texts.get(entity);
}

const TilemapComponent* Scene::getTilemap(Entity entity) const {
    return tilemaps.get(entity);
}

const ColliderComponent* Scene::getCollider(Entity entity) const {
    return colliders.get(entity);
}

const BoundsComponent* Scene::getBounds(Entity entity) const {
    return bounds.get(entity);
}

const InputComponent* Scene::getInput(Entity entity) const {
    return inputs.get(entity);
}

const ScriptComponent* Scene::getScript(Entity entity) const {
    return scripts.get(entity);
}

const LifetimeComponent* Scene::getLifetime(Entity entity) const {
    return lifetimes.get(entity);
}

ComponentPool<NameComponent>& Scene::getNamePool() {
    return names;
}

ComponentPool<TagComponent>& Scene::getTagPool() {
    return tags;
}

ComponentPool<TransformComponent>& Scene::getTransformPool() {
    return transforms;
}

ComponentPool<PhysicsComponent>& Scene::getPhysicsPool() {
    return physicsBodies;
}

ComponentPool<SpriteComponent>& Scene::getSpritePool() {
    return sprites;
}

ComponentPool<SpriteAnimationComponent>& Scene::getSpriteAnimationPool() {
    return spriteAnimations;
}

ComponentPool<TextComponent>& Scene::getTextPool() {
    return texts;
}

ComponentPool<TilemapComponent>& Scene::getTilemapPool() {
    return tilemaps;
}

ComponentPool<ColliderComponent>& Scene::getColliderPool() {
    return colliders;
}

ComponentPool<BoundsComponent>& Scene::getBoundsPool() {
    return bounds;
}

ComponentPool<InputComponent>& Scene::getInputPool() {
    return inputs;
}

ComponentPool<ScriptComponent>& Scene::getScriptPool() {
    return scripts;
}

ComponentPool<LifetimeComponent>& Scene::getLifetimePool() {
    return lifetimes;
}

const ComponentPool<NameComponent>& Scene::getNamePool() const {
    return names;
}

const ComponentPool<TagComponent>& Scene::getTagPool() const {
    return tags;
}

const ComponentPool<TransformComponent>& Scene::getTransformPool() const {
    return transforms;
}

const ComponentPool<PhysicsComponent>& Scene::getPhysicsPool() const {
    return physicsBodies;
}

const ComponentPool<SpriteComponent>& Scene::getSpritePool() const {
    return sprites;
}

const ComponentPool<SpriteAnimationComponent>& Scene::getSpriteAnimationPool() const {
    return spriteAnimations;
}

const ComponentPool<TextComponent>& Scene::getTextPool() const {
    return texts;
}

const ComponentPool<TilemapComponent>& Scene::getTilemapPool() const {
    return tilemaps;
}

const ComponentPool<ColliderComponent>& Scene::getColliderPool() const {
    return colliders;
}

const ComponentPool<BoundsComponent>& Scene::getBoundsPool() const {
    return bounds;
}

const ComponentPool<InputComponent>& Scene::getInputPool() const {
    return inputs;
}

const ComponentPool<ScriptComponent>& Scene::getScriptPool() const {
    return scripts;
}

const ComponentPool<LifetimeComponent>& Scene::getLifetimePool() const {
    return lifetimes;
}

Camera& Scene::getCamera() {
    return camera;
}

const Camera& Scene::getCamera() const {
    return camera;
}

void Scene::setCamera(Camera nextCamera) {
    camera = nextCamera;
}

Entity Scene::getPlayerEntity() const {
    return playerEntity;
}

void Scene::setPlayerEntity(Entity entity) {
    if (entity != InvalidEntity) {
        ensureEntityInRange(entity);
    }

    playerEntity = entity;
}

void Scene::ensureEntityInRange(Entity entity) const {
    if (!isValidEntity(entity)) {
        throw std::out_of_range("Invalid entity");
    }
}

void Scene::reserveComponentSlots() {
    names.reserveEntities(nextEntity);
    tags.reserveEntities(nextEntity);
    transforms.reserveEntities(nextEntity);
    physicsBodies.reserveEntities(nextEntity);
    sprites.reserveEntities(nextEntity);
    spriteAnimations.reserveEntities(nextEntity);
    texts.reserveEntities(nextEntity);
    tilemaps.reserveEntities(nextEntity);
    colliders.reserveEntities(nextEntity);
    bounds.reserveEntities(nextEntity);
    inputs.reserveEntities(nextEntity);
    scripts.reserveEntities(nextEntity);
    lifetimes.reserveEntities(nextEntity);
}
