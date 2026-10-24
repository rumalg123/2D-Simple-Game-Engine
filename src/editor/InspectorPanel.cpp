#include "Engine.h"
#include "imgui.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <vector>

namespace {
void editStringField(const char* label, std::string& value) {
    std::array<char, 64> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
    if (ImGui::InputText(label, buffer.data(), buffer.size())) {
        value = buffer.data();
    }
}
}

void Engine::renderInspectorPanel() {
    ImGui::Begin("Object Inspector");

    if (selectedEntity == InvalidEntity || !scene.isValidEntity(selectedEntity)) {
        ImGui::Text("No entity selected.");
        ImGui::End();
        return;
    }

    ImGui::Text("Entity %zu", selectedEntity);
    ImGui::TextWrapped("GUID: %s", scene.guid(selectedEntity).c_str());
    if (ImGui::SmallButton("Copy GUID")) {
        ImGui::SetClipboardText(scene.guid(selectedEntity).c_str());
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Undo checkpoint") && !playSession.active()) {
        recordSceneEdit();
        editorStatus = "Recorded undo checkpoint for inspector edits.";
    }
    if (playSession.active()) {
        ImGui::TextDisabled("Play mode: changes to this entity will be discarded on Stop.");
    }

    NameComponent* name = scene.getName(selectedEntity);
    TagComponent* tag = scene.getTag(selectedEntity);
    TransformComponent* transform = scene.getTransform(selectedEntity);
    PhysicsComponent* physics = scene.getPhysics(selectedEntity);
    SpriteComponent* sprite = scene.getSprite(selectedEntity);
    SpriteAnimationComponent* spriteAnimation = scene.getSpriteAnimation(selectedEntity);
    TextComponent* text = scene.getText(selectedEntity);
    TilemapComponent* tilemap = scene.getTilemap(selectedEntity);
    ColliderComponent* collider = scene.getCollider(selectedEntity);
    BoundsComponent* entityBounds = scene.getBounds(selectedEntity);
    InputComponent* input = scene.getInput(selectedEntity);
    ScriptComponent* script = scene.getScript(selectedEntity);
    LifetimeComponent* lifetime = scene.getLifetime(selectedEntity);

    if (ImGui::Button("Set Player")) {
        recordSceneEdit();
        scene.setPlayerEntity(selectedEntity);
        scene.setTag(selectedEntity, {"Player"});
        tag = scene.getTag(selectedEntity);
        editorStatus = "Entity " + std::to_string(selectedEntity) + " is now the player.";
    }
    ImGui::SameLine();
    if (ImGui::Button("Set Camera Target")) {
        recordSceneEdit();
        Camera camera = scene.getCamera();
        camera.target = selectedEntity;
        scene.setCamera(camera);
        editorStatus = "Camera now follows entity " + std::to_string(selectedEntity);
    }
    ImGui::SameLine();
    if (ImGui::Button("Duplicate")) {
        duplicateEntity(selectedEntity);
        ImGui::End();
        return;
    }

    if (ImGui::Button("Save As Prefab")) {
        saveSelectedPrefab();
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete Entity")) {
        deleteEntity(selectedEntity);
        ImGui::End();
        return;
    }

    ImGui::SeparatorText("Add Components");
    if (!name && ImGui::Button("Add Name")) {
        recordSceneEdit();
        scene.setName(selectedEntity, {"Entity " + std::to_string(selectedEntity)});
        name = scene.getName(selectedEntity);
    }
    if (!tag && ImGui::Button("Add Tag")) {
        recordSceneEdit();
        scene.setTag(selectedEntity, {"Untagged"});
        tag = scene.getTag(selectedEntity);
    }
    if (!transform && ImGui::Button("Add Transform")) {
        recordSceneEdit();
        scene.setTransform(selectedEntity, {0.0f, 0.0f});
        transform = scene.getTransform(selectedEntity);
    }
    if (!sprite && ImGui::Button("Add Sprite")) {
        recordSceneEdit();
        scene.setSprite(selectedEntity, {0.10f, 0.10f, 1.0f, 1.0f, 1.0f, 1.0f, 10, getEditorDefaultTexture()});
        sprite = scene.getSprite(selectedEntity);
    }
    if (!spriteAnimation && ImGui::Button("Add Sprite Animation")) {
        recordSceneEdit();
        scene.setSpriteAnimation(selectedEntity, {4, 1, 0, 4, 0, 0.10f, 0.0f, true, true});
        spriteAnimation = scene.getSpriteAnimation(selectedEntity);
    }
    if (!text && ImGui::Button("Add Text")) {
        recordSceneEdit();
        scene.setText(selectedEntity, {"TEXT", 0.08f, 0.12f, 0.02f, 0.04f, 1.0f, 1.0f, 1.0f, 1.0f, 100, true});
        text = scene.getText(selectedEntity);
    }
    if (!tilemap && ImGui::Button("Add Tilemap")) {
        recordSceneEdit();
        scene.setTilemap(
            selectedEntity,
            {8, 8, 0.16f, 0.16f, 1, 1, -100, getEditorDefaultTexture(), 1.0f, 1.0f, 1.0f, 1.0f,
             std::vector<int>(64, 0)});
        tilemap = scene.getTilemap(selectedEntity);
    }
    if (!physics && ImGui::Button("Add Physics")) {
        recordSceneEdit();
        scene.setPhysics(selectedEntity, {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, false, false, false});
        physics = scene.getPhysics(selectedEntity);
    }
    if (!collider && ImGui::Button("Add Collider")) {
        recordSceneEdit();
        scene.setCollider(selectedEntity, {0.10f, 0.10f, true, false});
        collider = scene.getCollider(selectedEntity);
    }
    if (!entityBounds && ImGui::Button("Add Bounds")) {
        recordSceneEdit();
        scene.setBounds(selectedEntity, {-1.0f, 1.0f, -1.0f, 1.0f});
        entityBounds = scene.getBounds(selectedEntity);
    }
    if (!input && ImGui::Button("Add Input")) {
        recordSceneEdit();
        scene.setInput(selectedEntity, {true, "MoveLeft", "MoveRight", "MoveUp", "MoveDown", 6.0f});
        input = scene.getInput(selectedEntity);
    }
    if (!script && ImGui::Button("Add Script")) {
        recordSceneEdit();
        scene.setScript(selectedEntity, {"", true, 0.0f});
        script = scene.getScript(selectedEntity);
    }
    if (!lifetime && ImGui::Button("Add Lifetime")) {
        recordSceneEdit();
        scene.setLifetime(selectedEntity, {5.0f});
        lifetime = scene.getLifetime(selectedEntity);
    }

    if (name && ImGui::CollapsingHeader("Name", ImGuiTreeNodeFlags_DefaultOpen)) {
        editStringField("Display Name", name->name);
        if (ImGui::Button("Remove Name")) {
            recordSceneEdit();
            scene.getNamePool().remove(selectedEntity);
            name = nullptr;
        }
    }

    if (tag && ImGui::CollapsingHeader("Tag")) {
        editStringField("Tag", tag->tag);
        if (ImGui::Button("Remove Tag")) {
            recordSceneEdit();
            scene.getTagPool().remove(selectedEntity);
            tag = nullptr;
        }
    }

    if (transform && ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("X", &transform->x, 0.01f);
        ImGui::DragFloat("Y", &transform->y, 0.01f);
        if (ImGui::Button("Remove Transform")) {
            recordSceneEdit();
            scene.getTransformPool().remove(selectedEntity);
            transform = nullptr;
        }
    }

    if (sprite && ImGui::CollapsingHeader("Sprite", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Half Width", &sprite->halfWidth, 0.01f, 0.01f, 5.0f);
        ImGui::DragFloat("Half Height", &sprite->halfHeight, 0.01f, 0.01f, 5.0f);
        ImGui::ColorEdit4("Tint", &sprite->red);
        ImGui::InputInt("Layer", &sprite->layer);
        ImGui::DragFloat("U Min", &sprite->uMin, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("V Min", &sprite->vMin, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("U Max", &sprite->uMax, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("V Max", &sprite->vMax, 0.01f, 0.0f, 1.0f);
        ImGui::Text("Texture handle: %zu", sprite->texture);
        if (ImGui::Button("Reset UVs")) {
            sprite->uMin = 0.0f;
            sprite->vMin = 0.0f;
            sprite->uMax = 1.0f;
            sprite->vMax = 1.0f;
        }
        if (ImGui::Button("Use Default White Texture")) {
            sprite->texture = getEditorDefaultTexture();
        }
        if (ImGui::Button("Remove Sprite")) {
            recordSceneEdit();
            scene.getSpritePool().remove(selectedEntity);
            sprite = nullptr;
        }
    }

    if (spriteAnimation && ImGui::CollapsingHeader("Sprite Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Playing", &spriteAnimation->playing);
        ImGui::Checkbox("Loop", &spriteAnimation->loop);
        ImGui::InputInt("Columns", &spriteAnimation->columns);
        ImGui::InputInt("Rows", &spriteAnimation->rows);
        ImGui::InputInt("First Frame", &spriteAnimation->firstFrame);
        ImGui::InputInt("Frame Count", &spriteAnimation->frameCount);
        ImGui::InputInt("Current Frame", &spriteAnimation->currentFrame);
        ImGui::DragFloat("Seconds Per Frame", &spriteAnimation->secondsPerFrame, 0.01f, 0.01f, 10.0f);
        ImGui::DragFloat("Elapsed Seconds", &spriteAnimation->elapsedSeconds, 0.01f, 0.0f, 10.0f);
        if (ImGui::Button("Restart Animation")) {
            spriteAnimation->currentFrame = 0;
            spriteAnimation->elapsedSeconds = 0.0f;
            spriteAnimation->playing = true;
        }
        if (ImGui::Button("Remove Sprite Animation")) {
            recordSceneEdit();
            scene.getSpriteAnimationPool().remove(selectedEntity);
            spriteAnimation = nullptr;
        }
    }

    if (text && ImGui::CollapsingHeader("Text", ImGuiTreeNodeFlags_DefaultOpen)) {
        editStringField("Text", text->text);
        ImGui::DragFloat("Character Width", &text->characterWidth, 0.01f, 0.01f, 2.0f);
        ImGui::DragFloat("Character Height", &text->characterHeight, 0.01f, 0.01f, 2.0f);
        ImGui::DragFloat("Character Spacing", &text->characterSpacing, 0.005f, 0.0f, 1.0f);
        ImGui::DragFloat("Line Spacing", &text->lineSpacing, 0.005f, 0.0f, 1.0f);
        ImGui::ColorEdit4("Text Color", &text->red);
        ImGui::InputInt("Text Layer", &text->layer);
        ImGui::Checkbox("Screen Space", &text->screenSpace);
        if (ImGui::Button("Remove Text")) {
            recordSceneEdit();
            scene.getTextPool().remove(selectedEntity);
            text = nullptr;
        }
    }

    if (tilemap && ImGui::CollapsingHeader("Tilemap", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputInt("Columns", &tilemap->columns);
        ImGui::InputInt("Rows", &tilemap->rows);
        ImGui::DragFloat("Tile Width", &tilemap->tileWidth, 0.01f, 0.01f, 5.0f);
        ImGui::DragFloat("Tile Height", &tilemap->tileHeight, 0.01f, 0.01f, 5.0f);
        ImGui::InputInt("Atlas Columns", &tilemap->atlasColumns);
        ImGui::InputInt("Atlas Rows", &tilemap->atlasRows);
        ImGui::InputInt("Tile Layer", &tilemap->layer);
        ImGui::ColorEdit4("Tile Tint", &tilemap->red);
        ImGui::Checkbox("Collision Enabled", &tilemap->collisionEnabled);
        ImGui::Checkbox("Collision Solid", &tilemap->collisionSolid);
        ImGui::Checkbox("Collision Trigger", &tilemap->collisionTrigger);
        int collisionLayer = static_cast<int>(tilemap->collisionLayer);
        if (ImGui::InputInt("Tile Collision Layer Bits", &collisionLayer)) {
            tilemap->collisionLayer = static_cast<unsigned int>(std::max(0, collisionLayer));
        }
        int collisionMask = static_cast<int>(tilemap->collisionMask);
        if (ImGui::InputInt("Tile Collision Mask Bits", &collisionMask)) {
            tilemap->collisionMask = static_cast<unsigned int>(collisionMask);
        }
        ImGui::Text("Texture handle: %zu", tilemap->texture);
        if (ImGui::Button("Use Default Tile Texture")) {
            tilemap->texture = getEditorDefaultTexture();
        }
        ImGui::SameLine();
        if (ImGui::Button("Resize Tiles")) {
            tilemap->columns = std::max(1, tilemap->columns);
            tilemap->rows = std::max(1, tilemap->rows);
            tilemap->tiles.resize(static_cast<std::size_t>(tilemap->columns * tilemap->rows), 0);
        }
        if (ImGui::Button("Fill Tile 0")) {
            tilemap->columns = std::max(1, tilemap->columns);
            tilemap->rows = std::max(1, tilemap->rows);
            tilemap->tiles.assign(static_cast<std::size_t>(tilemap->columns * tilemap->rows), 0);
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Tiles")) {
            tilemap->columns = std::max(1, tilemap->columns);
            tilemap->rows = std::max(1, tilemap->rows);
            tilemap->tiles.assign(static_cast<std::size_t>(tilemap->columns * tilemap->rows), -1);
        }
        ImGui::SameLine();
        if (ImGui::Button("Checker")) {
            tilemap->columns = std::max(1, tilemap->columns);
            tilemap->rows = std::max(1, tilemap->rows);
            tilemap->tiles.resize(static_cast<std::size_t>(tilemap->columns * tilemap->rows), 0);
            const int tileChoices = std::max(1, tilemap->atlasColumns * tilemap->atlasRows);
            for (int row = 0; row < tilemap->rows; ++row) {
                for (int column = 0; column < tilemap->columns; ++column) {
                    tilemap->tiles[static_cast<std::size_t>(row * tilemap->columns + column)] =
                        (row + column) % std::min(2, tileChoices);
                }
            }
        }

        tilemap->columns = std::max(1, tilemap->columns);
        tilemap->rows = std::max(1, tilemap->rows);
        const int editableRows = std::min(tilemap->rows, 8);
        const int editableColumns = std::min(tilemap->columns, 16);
        if (static_cast<int>(tilemap->tiles.size()) < tilemap->columns * tilemap->rows) {
            tilemap->tiles.resize(static_cast<std::size_t>(tilemap->columns * tilemap->rows), 0);
        }
        ImGui::Text("Tile indices (-1 hides tile). Showing %d x %d.", editableColumns, editableRows);
        ImGui::PushItemWidth(42.0f);
        for (int row = 0; row < editableRows; ++row) {
            for (int column = 0; column < editableColumns; ++column) {
                const std::size_t tileIndex = static_cast<std::size_t>(row * tilemap->columns + column);
                ImGui::PushID(static_cast<int>(tileIndex));
                ImGui::InputInt("##tile", &tilemap->tiles[tileIndex], 0, 0);
                ImGui::PopID();
                if (column + 1 < editableColumns) {
                    ImGui::SameLine();
                }
            }
        }
        ImGui::PopItemWidth();

        if (ImGui::Button("Remove Tilemap")) {
            recordSceneEdit();
            scene.getTilemapPool().remove(selectedEntity);
            tilemap = nullptr;
        }
    }

    if (physics && ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Velocity X", &physics->velocityX, 0.01f);
        ImGui::DragFloat("Velocity Y", &physics->velocityY, 0.01f);
        ImGui::DragFloat("Force X", &physics->forceX, 0.01f);
        ImGui::DragFloat("Force Y", &physics->forceY, 0.01f);
        ImGui::DragFloat("Inverse Mass", &physics->inverseMass, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("Drag", &physics->drag, 0.01f, 0.0f, 20.0f);
        ImGui::DragFloat("Restitution", &physics->restitution, 0.01f, 0.0f, 1.0f);
        ImGui::Checkbox("Static", &physics->isStatic);
        ImGui::Checkbox("Use World Bounds", &physics->usesWorldBounds);
        ImGui::Checkbox("Bounce At Bounds", &physics->bounceAtBounds);
        if (ImGui::Button("Remove Physics")) {
            recordSceneEdit();
            scene.getPhysicsPool().remove(selectedEntity);
            physics = nullptr;
        }
    }

    if (collider && ImGui::CollapsingHeader("Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Collider Half Width", &collider->halfWidth, 0.01f, 0.01f, 5.0f);
        ImGui::DragFloat("Collider Half Height", &collider->halfHeight, 0.01f, 0.01f, 5.0f);
        ImGui::Checkbox("Solid", &collider->solid);
        ImGui::Checkbox("Trigger", &collider->trigger);
        int layer = static_cast<int>(collider->layer);
        if (ImGui::InputInt("Collision Layer Bits", &layer)) {
            collider->layer = static_cast<unsigned int>(std::max(0, layer));
        }
        int mask = static_cast<int>(collider->mask);
        if (ImGui::InputInt("Collision Mask Bits", &mask)) {
            collider->mask = static_cast<unsigned int>(mask);
        }
        if (ImGui::Button("Remove Collider")) {
            recordSceneEdit();
            scene.getColliderPool().remove(selectedEntity);
            collider = nullptr;
        }
    }

    if (entityBounds && ImGui::CollapsingHeader("Bounds")) {
        ImGui::DragFloat("Min X", &entityBounds->minX, 0.01f);
        ImGui::DragFloat("Max X", &entityBounds->maxX, 0.01f);
        ImGui::DragFloat("Min Y", &entityBounds->minY, 0.01f);
        ImGui::DragFloat("Max Y", &entityBounds->maxY, 0.01f);
        if (ImGui::Button("Remove Bounds")) {
            recordSceneEdit();
            scene.getBoundsPool().remove(selectedEntity);
            entityBounds = nullptr;
        }
    }

    if (input && ImGui::CollapsingHeader("Input")) {
        ImGui::Checkbox("Enabled", &input->enabled);
        ImGui::DragFloat("Move Force", &input->moveForce, 0.1f, 0.0f, 50.0f);
        editStringField("Left Action", input->moveLeftAction);
        editStringField("Right Action", input->moveRightAction);
        editStringField("Up Action", input->moveUpAction);
        editStringField("Down Action", input->moveDownAction);
        if (ImGui::Button("Remove Input")) {
            recordSceneEdit();
            scene.getInputPool().remove(selectedEntity);
            input = nullptr;
        }
    }

    if (script && ImGui::CollapsingHeader("Script")) {
        ImGui::Checkbox("Active", &script->active);
        editStringField("Script Name", script->scriptName);
        scriptSystem.applyDefaultParameters(*script);
        const char* currentScriptName = script->scriptName.empty() ? "<none>" : script->scriptName.c_str();
        if (ImGui::BeginCombo("Registered Script", currentScriptName)) {
            for (const std::string& scriptName : scriptSystem.getScriptNames()) {
                const bool selected = script->scriptName == scriptName;
                if (ImGui::Selectable(scriptName.c_str(), selected)) {
                    script->scriptName = scriptName;
                    scriptSystem.applyDefaultParameters(*script);
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::Text("Elapsed: %.2f", script->elapsedTime);
        if (ImGui::Button("Reset Script Time")) {
            script->elapsedTime = 0.0f;
        }

        const std::vector<ScriptParameterDefinition>* parameterDefinitions =
            scriptSystem.getParameterDefinitions(script->scriptName);
        if (parameterDefinitions && !parameterDefinitions->empty()) {
            ImGui::SeparatorText("Parameters");
            if (ImGui::Button("Apply Missing Defaults")) {
                scriptSystem.applyDefaultParameters(*script);
            }

            for (const ScriptParameterDefinition& parameter : *parameterDefinitions) {
                float& value = script->parameters[parameter.name];
                ImGui::DragFloat(parameter.name.c_str(), &value, 0.01f, parameter.minValue, parameter.maxValue);
            }
        }

        // Lua scripts do not require C++ parameter definitions. Show all
        // serialized numeric script parameters in the Inspector.
        {
            std::vector<std::string> customParameters;
            for (const auto& parameter : script->parameters) {
                const bool defined = parameterDefinitions && std::any_of(
                    parameterDefinitions->begin(),
                    parameterDefinitions->end(),
                    [&parameter](const ScriptParameterDefinition& definition) {
                        return definition.name == parameter.first;
                    });
                if (!defined) {
                    customParameters.push_back(parameter.first);
                }
            }
            std::sort(customParameters.begin(), customParameters.end());

            if (!customParameters.empty()) {
                ImGui::SeparatorText("Custom Parameters");
                std::string parameterToRemove;
                for (const std::string& parameterName : customParameters) {
                    ImGui::PushID(parameterName.c_str());
                    ImGui::DragFloat(parameterName.c_str(), &script->parameters[parameterName], 0.01f);
                    ImGui::SameLine();
                    if (ImGui::Button("Remove")) {
                        parameterToRemove = parameterName;
                    }
                    ImGui::PopID();
                }
                if (!parameterToRemove.empty()) {
                    recordSceneEdit();
                    script->parameters.erase(parameterToRemove);
                }
            }

            static char parameterName[64] = "speed";
            ImGui::InputText("New Numeric Parameter", parameterName,
                             sizeof(parameterName));
            if (ImGui::Button("Add Numeric Parameter")) {
                const std::string name(parameterName);
                if (!name.empty() && name.find_first_not_of(
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_") ==
                        std::string::npos) {
                    recordSceneEdit();
                    script->parameters.emplace(name, 0.0f);
                } else {
                    editorStatus = "Parameter names may contain only letters, digits and underscores.";
                }
            }
        }

        if (ImGui::Button("Remove Script")) {
            recordSceneEdit();
            scene.getScriptPool().remove(selectedEntity);
            script = nullptr;
        }
    }

    if (lifetime && ImGui::CollapsingHeader("Lifetime")) {
        ImGui::DragFloat("Remaining Seconds", &lifetime->remainingSeconds, 0.05f, 0.0f, 60.0f);
        if (ImGui::Button("Remove Lifetime")) {
            recordSceneEdit();
            scene.getLifetimePool().remove(selectedEntity);
            lifetime = nullptr;
        }
    }

    ImGui::End();
}
