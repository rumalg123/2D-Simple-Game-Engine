#include "PluginSystem.h"


#include <algorithm>
#include <string>

void PluginManager::loadPlugin(std::unique_ptr<IPlugin> plugin, PluginContext& context) {
    plugin->onLoad(context);
    plugins.push_back(std::move(plugin));
}

void PluginManager::update(PluginContext& context, float deltaTime) {
    for (std::unique_ptr<IPlugin>& plugin : plugins) {
        plugin->onUpdate(context, deltaTime);
    }
}

void PluginManager::unloadAll(PluginContext& context) {
    for (auto plugin = plugins.rbegin(); plugin != plugins.rend(); ++plugin) {
        (*plugin)->onUnload(context);
    }
    plugins.clear();
}

std::size_t PluginManager::pluginCount() const {
    return plugins.size();
}

std::vector<std::string> PluginManager::pluginNames() const {
    std::vector<std::string> names;
    names.reserve(plugins.size());
    for (const std::unique_ptr<IPlugin>& plugin : plugins) {
        names.push_back(plugin->name());
    }

    std::sort(names.begin(), names.end());
    return names;
}
