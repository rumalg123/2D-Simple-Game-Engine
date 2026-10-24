#include "SceneEditSession.h"

#include <utility>

bool SceneEditSession::begin(Scene& current) {
    if (authored) return false;
    authored = current; // deep copy of ECS component pools and stable GUIDs
    return true;
}

bool SceneEditSession::stop(Scene& current) {
    if (!authored) return false;
    current = std::move(*authored);
    authored.reset();
    return true;
}

void SceneHistory::record(const Scene& scene) {
    if (capacity == 0) return;
    if (undoStack.size() == capacity) undoStack.pop_front();
    undoStack.push_back(scene);
    redoStack.clear();
}

bool SceneHistory::undo(Scene& scene) {
    if (undoStack.empty()) return false;
    if (capacity != 0 && redoStack.size() == capacity) redoStack.pop_front();
    redoStack.push_back(std::move(scene));
    scene = std::move(undoStack.back());
    undoStack.pop_back();
    return true;
}

bool SceneHistory::redo(Scene& scene) {
    if (redoStack.empty()) return false;
    if (capacity != 0 && undoStack.size() == capacity) undoStack.pop_front();
    undoStack.push_back(std::move(scene));
    scene = std::move(redoStack.back());
    redoStack.pop_back();
    return true;
}

void SceneHistory::clear() {
    undoStack.clear();
    redoStack.clear();
}
