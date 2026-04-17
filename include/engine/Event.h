#pragma once

#include "ECS.h"
#include "Memory.h"

#include <cstddef>
#include <vector>

enum class EventType {
    KeyChanged,
    Collision,
    Trigger
};

struct KeyChangedEvent {
    int key;
    bool pressed;
};

struct CollisionEvent {
    Entity first;
    Entity second;
    bool trigger;
};

struct Event {
    EventType type;
    KeyChangedEvent key;
    CollisionEvent collision;
};

class EventQueue {
public:
    void publishKeyChanged(int key, bool pressed) {
        events.push_back(eventPool.create(Event{EventType::KeyChanged, {key, pressed}, {0, 0, false}}));
    }

    void publishCollision(Entity first, Entity second) {
        events.push_back(eventPool.create(Event{EventType::Collision, {0, false}, {first, second, false}}));
    }

    void publishTrigger(Entity first, Entity second) {
        events.push_back(eventPool.create(Event{EventType::Trigger, {0, false}, {first, second, true}}));
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
