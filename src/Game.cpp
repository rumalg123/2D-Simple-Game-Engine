#include "Game.h"

#include "Prefab.h"
#include "Scene.h"

#include <exception>

void GameContext::reloadScene() const {
    if (reloadSceneRequest) {
        reloadSceneRequest();
    }
}

void GameContext::clearScene() const {
    if (clearSceneRequest) {
        clearSceneRequest();
    }
}

bool GameContext::loadScene(const std::string& path) const {
    return loadSceneRequest ? loadSceneRequest(path) : false;
}

Entity GameContext::instantiatePrefab(const std::string& prefabName) const {
    if (!scene || !prefabs) {
        return InvalidEntity;
    }

    try {
        return prefabs->instantiate(*scene, prefabName);
    } catch (const std::exception& exception) {
        if (editorStatus) {
            *editorStatus = exception.what();
        }
        return InvalidEntity;
    }
}

Entity GameContext::instantiatePrefab(const std::string& prefabName, TransformComponent transformOverride) const {
    if (!scene || !prefabs) {
        return InvalidEntity;
    }

    try {
        return prefabs->instantiate(*scene, prefabName, transformOverride);
    } catch (const std::exception& exception) {
        if (editorStatus) {
            *editorStatus = exception.what();
        }
        return InvalidEntity;
    }
}
