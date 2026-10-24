#pragma once

#include "Scene.h"

#include <cstddef>
#include <deque>
#include <optional>

// The authored scene stays untouched while a runtime copy is simulated.
// Temporary runtime edits are discarded at Stop; saving while playing is
// deliberately disallowed by the editor until Apply Changes is implemented.
class SceneEditSession {
public:
    bool begin(Scene& current);
    bool stop(Scene& current);
    bool active() const { return authored.has_value(); }
    const Scene& authoredScene(const Scene& current) const {
        return authored ? *authored : current;
    }
private:
    std::optional<Scene> authored;
};

// Bounded full-scene undo history for editor authoring operations.
// Recording before an operation keeps deleted entities/components recoverable.
class SceneHistory {
public:
    explicit SceneHistory(std::size_t capacity = 32) : capacity(capacity) {}
    void record(const Scene& scene);
    bool undo(Scene& scene);
    bool redo(Scene& scene);
    void clear();
    bool canUndo() const { return !undoStack.empty(); }
    bool canRedo() const { return !redoStack.empty(); }
private:
    std::size_t capacity;
    std::deque<Scene> undoStack;
    std::deque<Scene> redoStack;
};
