#include "Memory.h"

#include <algorithm>
#include <stdexcept>

namespace {
std::size_t alignForward(std::size_t value, std::size_t alignment) {
    const std::size_t mask = alignment - 1;
    return (value + mask) & ~mask;
}
}

StackAllocator::StackAllocator(std::size_t capacityBytes) {
    init(capacityBytes);
}

void StackAllocator::init(std::size_t capacityBytes) {
    buffer.resize(capacityBytes);
    offset = 0;
}

void StackAllocator::reset() {
    offset = 0;
}

std::size_t StackAllocator::marker() const {
    return offset;
}

void StackAllocator::freeToMarker(std::size_t nextMarker) {
    if (nextMarker > buffer.size()) {
        throw std::out_of_range("StackAllocator marker is outside the buffer");
    }

    offset = nextMarker;
}

void* StackAllocator::allocate(std::size_t sizeBytes, std::size_t alignment) {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        throw std::invalid_argument("StackAllocator alignment must be a power of two");
    }

    const std::uintptr_t baseAddress = reinterpret_cast<std::uintptr_t>(buffer.data());
    const std::uintptr_t currentAddress = baseAddress + offset;
    const std::uintptr_t alignedAddress = alignForward(currentAddress, alignment);
    const std::size_t nextOffset = static_cast<std::size_t>(alignedAddress - baseAddress) + sizeBytes;

    if (nextOffset > buffer.size()) {
        throw std::bad_alloc();
    }

    offset = nextOffset;
    return reinterpret_cast<void*>(alignedAddress);
}

std::size_t StackAllocator::capacity() const {
    return buffer.size();
}

std::size_t StackAllocator::used() const {
    return offset;
}
