#include "PluginSystem.h"
#include "imgui.h"

#include <exception>
#include <memory>
#include <string>

namespace {
class EditorPrefabPlugin final : public IPlugin {
public:
    const char* name() const override {
        return "Editor Prefab Tools";
    }

    void onRenderUi(PluginContext& context) override {
        ImGui::Begin("Plugins");
        ImGui::Text("Loaded plugin: %s", name());
        ImGui::Text("Prefabs: %zu", context.prefabs->prefabCount());
        ImGui::Text("Assets: %zu", context.assets->entryCount());
        ImGui::Text("Scripts: %zu", context.scripts->scriptCount());
        ImGui::Text("Input actions: %zu", context.inputMap->getActions().size());

        ImGui::SeparatorText("Spawn Prefab");
        for (const std::string& prefabName : context.prefabs->getPrefabNames()) {
            ImGui::PushID(prefabName.c_str());
            if (ImGui::Button(prefabName.c_str())) {
                try {
                    const Entity entity = context.prefabs->instantiate(*context.scene, prefabName, {0.0f, 0.0f});
                    if (context.editorStatus) {
                        *context.editorStatus = "Spawned prefab '" + prefabName + "' as entity " + std::to_string(entity);
                    }
                } catch (const std::exception& exception) {
                    if (context.editorStatus) {
                        *context.editorStatus = exception.what();
                    }
                }
            }
            ImGui::PopID();
        }

        ImGui::End();
    }
};
}

void PluginManager::loadDefaultPlugins(PluginContext& context) {
    loadPlugin(std::make_unique<EditorPrefabPlugin>(), context);
}

void PluginManager::renderUi(PluginContext& context) {
    for (std::unique_ptr<IPlugin>& plugin : plugins) {
        plugin->onRenderUi(context);
    }
}

