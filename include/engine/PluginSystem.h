#pragma once

#include "InputMap.h"
#include "Prefab.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "ScriptSystem.h"
#include <memory>
#include <string>
#include <vector>

struct PluginContext {
    Scene* scene;
    ResourceManager* resources;
    PrefabRegistry* prefabs;
    ScriptSystem* scripts;
    InputMap* inputMap;
    std::string* editorStatus;
};

class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual const char* name() const = 0;
    virtual void onLoad(PluginContext&) {}
    virtual void onUnload(PluginContext&) {}
    virtual void onUpdate(PluginContext&, float) {}
    virtual void onRenderUi(PluginContext&) {}
};

class PluginManager {
public:
    void loadPlugin(std::unique_ptr<IPlugin> plugin, PluginContext& context);
    void loadDefaultPlugins(PluginContext& context);
    void update(PluginContext& context, float deltaTime);
    void renderUi(PluginContext& context);
    void unloadAll(PluginContext& context);
    std::size_t pluginCount() const;
    std::vector<std::string> pluginNames() const;

private:
    std::vector<std::unique_ptr<IPlugin>> plugins;
};
