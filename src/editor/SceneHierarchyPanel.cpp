#include "Engine.h"
#include "imgui.h"

#include <cstdio>

void Engine::renderSceneHierarchyPanel() {
    ImGui::Begin("Scene Hierarchy");

    if (ImGui::Button("Create Empty")) {
        createEmptyEntity();
    }
    ImGui::SameLine();
    if (ImGui::Button("Duplicate") && selectedEntity != InvalidEntity) {
        duplicateEntity(selectedEntity);
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete") && selectedEntity != InvalidEntity) {
        deleteEntity(selectedEntity);
    }

    ImGui::Separator();

    const Entity player = scene.getPlayerEntity();
    for (Entity entity = 0; entity < scene.entityCount(); ++entity) {
        if (!scene.isValidEntity(entity)) {
            continue;
        }

        char label[64];
        const NameComponent* name = scene.getName(entity);
        const char* displayName = name ? name->name.c_str() : "Entity";
        if (entity == player) {
            std::snprintf(label, sizeof(label), "%s %zu (Player)", displayName, entity);
        } else {
            std::snprintf(label, sizeof(label), "%s %zu", displayName, entity);
        }

        const bool isSelected = selectedEntity == entity;
        if (ImGui::Selectable(label, isSelected)) {
            selectedEntity = entity;
        }
    }

    ImGui::End();
}
