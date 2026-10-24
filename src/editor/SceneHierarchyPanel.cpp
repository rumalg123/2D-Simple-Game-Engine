#include "Engine.h"
#include "imgui.h"

#include <cstdio>
#include <algorithm>
#include <string>

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

    ImGui::SeparatorText("Tilemap Layers");
    if(ImGui::Button("Add Tile Layer") && !playSession.active()) {
        recordSceneEdit();
        TilemapComponent next;
        TransformComponent at{0.0f,0.0f};
        if(scene.isValidEntity(selectedEntity)) {
            if(const auto* map=scene.getTilemap(selectedEntity)) {
                next=*map;
                std::fill(next.tiles.begin(),next.tiles.end(),-1);
                if(next.terrainEnabled)
                    std::fill(next.terrainCells.begin(),next.terrainCells.end(),0);
                next.layer=map->layer+1;
                if(const auto* pos=scene.getTransform(selectedEntity)) at=*pos;
            }
        }
        const Entity created=scene.createEntity("Tile Layer",at);
        scene.setTilemap(created,std::move(next));
        selectedEntity=created;
    }
    if(scene.isValidEntity(selectedEntity)) {
        if(TilemapComponent* map=scene.getTilemap(selectedEntity)) {
            ImGui::Text("Selected tile draw layer: %d",map->layer);
            if(ImGui::SmallButton("Layer Down") && !playSession.active()) {
                recordSceneEdit();
                --map->layer;
            }
            ImGui::SameLine();
            if(ImGui::SmallButton("Layer Up") && !playSession.active()) {
                recordSceneEdit();
                ++map->layer;
            }
        }
    }
    ImGui::TextDisabled("Each tile layer is a separate entity with its own draw order.");

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
