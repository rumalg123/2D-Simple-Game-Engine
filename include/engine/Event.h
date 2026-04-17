#pragma once

#include "ECS.h"
#include "Memory.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

enum class EventType {
    KeyChanged,
    ActionChanged,
    Collision,
    Trigger
};

enum class CollisionPhase {
    Enter,
    Stay,
    Exit
};

struct KeyChangedEvent {
    int key = 0;
    bool pressed = false;
};

struct ActionChangedEvent {
    std::string actionName;
    bool pressed = false;
};

struct CollisionEvent {
    Entity first = InvalidEntity;
    Entity second = InvalidEntity;
    bool trigger = false;
    CollisionPhase phase = CollisionPhase::Enter;
};

struct Event {
    EventType type = EventType::KeyChanged;
    KeyChangedEvent key;
    ActionChangedEvent action;
    CollisionEvent collision;
};

class EventQueue {
public:
    void publishKeyChanged(int key, bool pressed) {
        Event event;
        event.type = EventType::KeyChanged;
        event.key = {key, pressed};
        events.push_back(eventPool.create(std::move(event)));
    }

    void publishActionChanged(const std::string& actionName, bool pressed) {
        Event event;
        event.type = EventType::ActionChanged;
        event.action = {actionName, pressed};
        events.push_back(eventPool.create(std::move(event)));
    }

    void publishCollision(Entity first, Entity second, CollisionPhase phase = CollisionPhase::Enter) {
        Event event;
        event.type = EventType::Collision;
        event.collision = {first, second, false, phase};
        events.push_back(eventPool.create(std::move(event)));
    }

    void publishTrigger(Entity first, Entity second, CollisionPhase phase = CollisionPhase::Enter) {
        Event event;
        event.type = EventType::Trigger;
        event.collision = {first, second, true, phase};
        events.push_back(eventPool.create(std::move(event)));
    }

    std::vector<Event> drain() {
        std::vector<Event> drained;
        drained.reserve(events.size());
        for (Event* event : events) {
            drained.push_back(*event);
            eventPool.destroy(event);
        }

        events.clear();
        return drained;
    }

    std::size_t queuedEventCount() const {
        return events.size();
    }

    std::size_t eventPoolUsed() const {
        return eventPool.used();
    }

    std::size_t eventPoolCapacity() const {
        return eventPool.capacity();
    }

private:
    PoolAllocator<Event> eventPool{1024};
    std::vector<Event*> events;
};
