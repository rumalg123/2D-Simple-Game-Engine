#include "Scene.h"
#include "GameConfig.h"
#include "PluginSystem.h"

#include <iostream>
#include <memory>
#include <string>

// Compiles and links only against se_runtime, not engine_core or se_editor_ui.
// The scene and its components must remain usable outside the editor.
int main() {
    Scene scene;
    const Entity player = scene.createEntity("Player", {1.0f, 2.0f});

    if (!scene.isValidEntity(player)) {
        std::cerr << "Created entity is invalid.\n";
        return 1;
    }

    const NameComponent* name = scene.getName(player);
    const TransformComponent* position = scene.getTransform(player);
    if (!name || name->name != "Player" || !position ||
        position->x != 1.0f || position->y != 2.0f) {
        std::cerr << "Runtime-only scene did not preserve components.\n";
        return 1;
    }

    scene.setTransform(player, {3.0f, 4.0f});
    position = scene.getTransform(player);
    if (!position || position->x != 3.0f || position->y != 4.0f) {
        std::cerr << "Runtime-only component modification failed.\n";
        return 1;
    }

    scene.destroyEntity(player);
    if (scene.isValidEntity(player)) {
        std::cerr << "Runtime-only entity destruction failed.\n";
        return 1;
    }

    // Generic plugins must be usable by the runtime without editor/ImGui symbols.
    struct TestPlugin final : public IPlugin {
        int& loads;
        int& updates;
        int& unloads;
        TestPlugin(int& loadCount, int& updateCount, int& unloadCount)
            : loads(loadCount), updates(updateCount), unloads(unloadCount) {}
        const char* name() const override { return "Runtime Test Plugin"; }
        void onLoad(PluginContext&) override { ++loads; }
        void onUpdate(PluginContext&, float) override { ++updates; }
        void onUnload(PluginContext&) override { ++unloads; }
    };

    int loads = 0;
    int updates = 0;
    int unloads = 0;
    PluginContext context{};
    PluginManager plugins;
    plugins.loadPlugin(std::make_unique<TestPlugin>(loads, updates, unloads), context);
    if (plugins.pluginCount() != 1 || loads != 1 ||
        plugins.pluginNames().front() != "Runtime Test Plugin") {
        std::cerr << "Runtime-only plugin registration failed.\\n";
        return 1;
    }
    plugins.update(context, 0.016f);
    plugins.unloadAll(context);
    if (updates != 1 || unloads != 1 || plugins.pluginCount() != 0) {
        std::cerr << "Runtime-only plugin lifecycle failed.\\n";
        return 1;
    }

    std::cout << "Runtime-only link and scene lifecycle passed.\n";
    return 0;
}
