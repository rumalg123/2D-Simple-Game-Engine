#pragma once

#include "ECS.h"

#include <functional>
#include <string>
#include <vector>

class InputMap;
class PluginManager;
class PrefabRegistry;
class ResourceManager;
class AssetManifest;
class Scene;
class ScriptSystem;
class AudioSystem;
struct Event;

struct GameContext {
    Scene* scene = nullptr;
    ResourceManager* resources = nullptr;
    AssetManifest* assets = nullptr;
    PrefabRegistry* prefabs = nullptr;
    ScriptSystem* scripts = nullptr;
    InputMap* inputMap = nullptr;
    AudioSystem* audio = nullptr;
    std::string* editorStatus = nullptr;
    std::function<void()> reloadSceneRequest;
    std::function<void()> clearSceneRequest;
    std::function<bool(const std::string&)> loadSceneRequest;

    void reloadScene() const;
    void clearScene() const;
    bool loadScene(const std::string& path) const;
    Entity instantiatePrefab(const std::string& prefabName) const;
    Entity instantiatePrefab(const std::string& prefabName, TransformComponent transformOverride) const;
};

struct GameDebugStat {
    std::string name;
    int value = 0;
};

class IGame {
public:
    virtual ~IGame() = default;

    virtual const char* name() const = 0;
    virtual const char* controlsHelp() const {
        return "";
    }
    virtual void configureInput(GameContext&) {}
    virtual void registerScripts(GameContext&) {}
    virtual void loadInitialScene(GameContext&) {}
    virtual void registerPlugins(GameContext&, PluginManager&) {}
    virtual void onSceneLoaded(GameContext&) {}
    virtual void onFixedUpdate(GameContext&, float) {}
    virtual void onEvent(GameContext&, const Event&) {}
    virtual std::vector<GameDebugStat> debugStats() const {
        return {};
    }
};
