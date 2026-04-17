#pragma once

#include "ECS.h"

#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

template <typename Component>
class ComponentPool {
public:
    static constexpr std::size_t InvalidIndex = std::numeric_limits<std::size_t>::max();

    void clear() {
        entityToDense.clear();
        entities.clear();
        components.clear();
    }

    void reserveEntities(std::size_t count) {
        if (entityToDense.size() < count) {
            entityToDense.resize(count, InvalidIndex);
        }
    }

    void set(Entity entity, Component component) {
        reserveEntities(entity + 1);

        if (has(entity)) {
            components[entityToDense[entity]] = std::move(component);
            return;
        }

        entityToDense[entity] = components.size();
        entities.push_back(entity);
        components.push_back(std::move(component));
    }

    void remove(Entity entity) {
        if (!has(entity)) {
            return;
        }

        const std::size_t denseIndex = entityToDense[entity];
        const std::size_t lastIndex = components.size() - 1;

        if (denseIndex != lastIndex) {
            components[denseIndex] = std::move(components[lastIndex]);
            entities[denseIndex] = entities[lastIndex];
            entityToDense[entities[denseIndex]] = denseIndex;
        }

        components.pop_back();
        entities.pop_back();
        entityToDense[entity] = InvalidIndex;
    }

    bool has(Entity entity) const {
        return entity < entityToDense.size() && entityToDense[entity] != InvalidIndex;
    }

    Component* get(Entity entity) {
        if (!has(entity)) {
            return nullptr;
        }

        return &components[entityToDense[entity]];
    }

    const Component* get(Entity entity) const {
        if (!has(entity)) {
            return nullptr;
        }

        return &components[entityToDense[entity]];
    }

    std::size_t size() const {
        return components.size();
    }

    bool empty() const {
        return components.empty();
    }

    Entity entityAt(std::size_t denseIndex) const {
        return entities[denseIndex];
    }

    Component& componentAt(std::size_t denseIndex) {
        return components[denseIndex];
    }

    const Component& componentAt(std::size_t denseIndex) const {
        return components[denseIndex];
    }

    std::vector<Entity>& entityData() {
        return entities;
    }

    const std::vector<Entity>& entityData() const {
        return entities;
    }

    std::vector<Component>& componentData() {
        return components;
    }

    const std::vector<Component>& componentData() const {
        return components;
    }

private:
    std::vector<std::size_t> entityToDense;
    std::vector<Entity> entities;
    std::vector<Component> components;
};
