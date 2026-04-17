#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

class StackAllocator {
public:
    StackAllocator() = default;
    explicit StackAllocator(std::size_t capacityBytes);

    void init(std::size_t capacityBytes);
    void reset();
    std::size_t marker() const;
    void freeToMarker(std::size_t marker);
    void* allocate(std::size_t sizeBytes, std::size_t alignment);

    template <typename T>
    T* allocateArray(std::size_t count) {
        if (count == 0) {
            return nullptr;
        }

        return static_cast<T*>(allocate(sizeof(T) * count, alignof(T)));
    }

    std::size_t capacity() const;
    std::size_t used() const;

private:
    std::vector<std::uint8_t> buffer;
    std::size_t offset = 0;
};

template <typename T>
class PoolAllocator {
public:
    PoolAllocator() = default;
    explicit PoolAllocator(std::size_t capacity) {
        init(capacity);
    }

    ~PoolAllocator() {
        reset();
    }

    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    void init(std::size_t capacity) {
        reset();
        slots.clear();
        slots.resize(capacity);
        alive.assign(capacity, false);
        freeList.clear();
        freeList.reserve(capacity);

        for (std::size_t index = 0; index < capacity; ++index) {
            freeList.push_back(capacity - index - 1);
        }
    }

    template <typename... Args>
    T* create(Args&&... args) {
        if (freeList.empty()) {
            throw std::bad_alloc();
        }

        const std::size_t index = freeList.back();
        freeList.pop_back();
        alive[index] = true;
        return new (slots[index].storage) T(std::forward<Args>(args)...);
    }

    void destroy(T* object) {
        if (!object || slots.empty()) {
            return;
        }

        const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(slots.data());
        const std::uintptr_t target = reinterpret_cast<std::uintptr_t>(object);
        const std::size_t stride = sizeof(Slot);
        const std::size_t index = (target - base) / stride;

        if (index >= slots.size() || !alive[index]) {
            return;
        }

        object->~T();
        alive[index] = false;
        freeList.push_back(index);
    }

    void reset() {
        for (std::size_t index = 0; index < slots.size(); ++index) {
            if (alive[index]) {
                reinterpret_cast<T*>(slots[index].storage)->~T();
                alive[index] = false;
            }
        }

        freeList.clear();
        freeList.reserve(slots.size());
        for (std::size_t index = 0; index < slots.size(); ++index) {
            freeList.push_back(slots.size() - index - 1);
        }
    }

    std::size_t capacity() const {
        return slots.size();
    }

    std::size_t used() const {
        return slots.size() - freeList.size();
    }

private:
    struct Slot {
        alignas(T) unsigned char storage[sizeof(T)];
    };

    std::vector<Slot> slots;
    std::vector<bool> alive;
    std::vector<std::size_t> freeList;
};
