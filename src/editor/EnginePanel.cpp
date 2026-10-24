#include "Engine.h"
#include "imgui.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {
constexpr float FixedDeltaTime = 1.0f / 60.0f;

std::string describeActionBindings(const InputAction& action) {
    std::string description;
    auto append = [&description](const std::string& value) {
        if (!description.empty()) {
            description += ", ";
        }
        description += value;
    };

    for (int key : action.keys) {
        append("key " + std::to_string(key));
    }
    for (int button : action.gamepadButtons) {
        append("pad button " + std::to_string(button));
    }
    for (const GamepadAxisBinding& binding : action.gamepadAxes) {
        append(
            "pad axis " + std::to_string(binding.axis) +
            (binding.direction == GamepadAxisDirection::Negative ? " -" : " +"));
    }

    return description.empty() ? "unbound" : description;
}
}

void Engine::renderPlayPausePanel() {
    ImGui::Begin("Engine");
    ImGui::Text("Simulation: %s", isPlaying ? "Playing" : "Paused");

    if (ImGui::Button(isPlaying ? "Pause" : "Play")) {
        isPlaying = !isPlaying;
    }
    ImGui::SameLine();
    if (ImGui::Button("Step")) {
        inputSystem();
        spriteAnimationSystem(FixedDeltaTime);
        physicsSystem.update(scene, eventQueue, FixedDeltaTime, &jobSystem);
        cameraSystem();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Scene")) {
        initScene();
    }

    if (ImGui::Button("New Empty Scene")) {
        newEmptyScene();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save Scene JSON")) {
        saveCurrentScene();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Scene JSON")) {
        loadSavedScene();
    }

    ImGui::SameLine();
    ImGui::Checkbox("Hot Reload", &hotReloadEnabled);

    if (ImGui::CollapsingHeader("Physics Settings")) {
        PhysicsSettings physicsSettings = physicsSystem.getSettings();
        bool changed = false;
        changed = ImGui::DragFloat("Gravity X", &physicsSettings.gravityX, 0.01f, -100.0f, 100.0f) || changed;
        changed = ImGui::DragFloat("Gravity Y", &physicsSettings.gravityY, 0.01f, -100.0f, 100.0f) || changed;
        changed = ImGui::Checkbox("Emit Stay Events", &physicsSettings.emitStayEvents) || changed;
        changed = ImGui::Checkbox("Emit Exit Events", &physicsSettings.emitExitEvents) || changed;
        if (changed) {
            physicsSystem.setSettings(physicsSettings);
        }
        if (ImGui::Button("Clear Physics Contacts")) {
            physicsSystem.clearContacts();
            editorStatus = "Cleared physics contacts.";
        }
    }

    ImGui::Separator();
    ImGui::Text("Game: %s", game ? game->name() : "None");
    if (game) {
        for (const GameDebugStat& stat : game->debugStats()) {
            ImGui::Text("%s: %d", stat.name.c_str(), stat.value);
        }
    }
    ImGui::Text("Entities: %zu", scene.entityCount());
    ImGui::Text("Alive entities: %zu", scene.livingEntityCount());
    ImGui::Text("Prefabs: %zu", prefabs.prefabCount());
    ImGui::Text("Scripts: %zu", scriptSystem.scriptCount());
    ImGui::Text("Plugins: %zu", pluginManager.pluginCount());
    ImGui::Text("Job workers: %zu", jobSystem.workerCount());
    ImGui::Text("Audio clips: %zu", audioSystem.clipCount());
    ImGui::Text("Audio voices: %zu", audioSystem.activeVoiceCount());
    if (game && game->controlsHelp()[0] != '\0') {
        ImGui::Text("Controls: %s", game->controlsHelp());
    }

    if (ImGui::CollapsingHeader("Renderer")) {
        const RenderStats& renderStats = renderer.getStats();
        RenderLight& light = renderer.getLight();
        ImGui::Text("Sprites: %zu", renderStats.spriteCount);
        ImGui::Text("Shadows: %zu", renderStats.shadowCount);
        ImGui::Text("Collider overlays: %zu", renderStats.colliderCount);
        ImGui::Text("Batches: %zu", renderStats.batches);
        ImGui::Text("Draw calls: %zu", renderStats.drawCalls);
        ImGui::Text("Rendered frame: %zu", renderStats.frameIndex);
        ImGui::Text("Texture metadata: %zu", renderStats.textureCount);
        ImGui::Text("GPU textures: %zu", renderStats.gpuTextureCount);
        ImGui::Text("Render passes: %zu", renderStats.renderPasses);
        ImGui::Text(
            "Instancing: %s%s",
            renderStats.instancingUsed ? "active" : "inactive",
            renderer.isInstancingAvailable() ? "" : " (unsupported)");
        const GpuResourceStats& gpuStats = renderer.getGpuResourceStats();
        ImGui::Text("Texture uploads: %zu", gpuStats.textureUploads);
        ImGui::Text("Texture deletes: %zu", gpuStats.textureDeletes);
        ImGui::Text("Render frames submitted: %zu", renderCommands.submittedFrameCount());
        ImGui::Text("Render commands last frame: %zu", renderCommands.lastExecutedCommandCount());
        ImGui::Text("Render commands queued: %zu", renderCommands.queuedCommandCount());
        ImGui::Text("Render thread: %s", renderCommands.isRunning() ? "running" : "stopped");
        ImGui::Text("Render thread id hash: %zu", renderCommands.renderThreadHash());
        ImGui::Text("GL owner id hash: %zu", glContext.ownerThreadHash());
        ImGui::Text(
            "Main thread owns GL: %s",
            glContext.isCurrentThreadOwner() ? "yes" : "no");
        ImGui::Text("Frame allocator: %zu / %zu bytes", renderer.frameScratchUsed(), renderer.frameScratchCapacity());
        ImGui::Text("Event pool: %zu / %zu", eventQueue.eventPoolUsed(), eventQueue.eventPoolCapacity());
        bool instancingEnabled = renderer.isInstancingEnabled();
        if (ImGui::Checkbox("Use Instancing", &instancingEnabled)) {
            renderer.setInstancingEnabled(instancingEnabled);
        }
        ImGui::Checkbox("Lighting", &light.enabled);
        ImGui::Checkbox("2D Shadows", &light.shadowsEnabled);
        ImGui::DragFloat2("Light Position", &light.x, 0.01f);
        ImGui::ColorEdit3("Light Color", &light.red);
        ImGui::DragFloat("Ambient", &light.ambient, 0.01f, 0.0f, 2.0f);
        ImGui::DragFloat("Radius", &light.radius, 0.01f, 0.05f, 10.0f);
        ImGui::DragFloat2("Shadow Offset", &light.shadowOffsetX, 0.01f);
        ImGui::DragFloat("Shadow Alpha", &light.shadowAlpha, 0.01f, 0.0f, 1.0f);
    }

    if (ImGui::CollapsingHeader("ECS Storage")) {
        ImGui::Text("Transforms: %zu", scene.getTransformPool().size());
        ImGui::Text("Physics bodies: %zu", scene.getPhysicsPool().size());
        ImGui::Text("Sprites: %zu", scene.getSpritePool().size());
        ImGui::Text("Sprite animations: %zu", scene.getSpriteAnimationPool().size());
        ImGui::Text("Texts: %zu", scene.getTextPool().size());
        ImGui::Text("Tilemaps: %zu", scene.getTilemapPool().size());
        ImGui::Text("Colliders: %zu", scene.getColliderPool().size());
        ImGui::Text("Inputs: %zu", scene.getInputPool().size());
        ImGui::Text("Scripts: %zu", scene.getScriptPool().size());
        ImGui::Text("Tags: %zu", scene.getTagPool().size());
        ImGui::Text("Lifetimes: %zu", scene.getLifetimePool().size());
    }

    if (ImGui::CollapsingHeader("Input Mapping")) {
        const std::string gamepadStatus = activeGamepadName.empty() ? "" : " (" + activeGamepadName + ")";
        ImGui::Text("Gamepads: %d%s", connectedGamepadCount, gamepadStatus.c_str());
        std::vector<std::string> actionNames;
        for (const auto& action : inputMap.getActions()) {
            actionNames.push_back(action.first);
        }
        std::sort(actionNames.begin(), actionNames.end());

        for (const std::string& actionName : actionNames) {
            const InputAction& action = inputMap.getActions().at(actionName);
            const std::string bindings = describeActionBindings(action);
            ImGui::Text("%s -> %s [%s]", action.name.c_str(), bindings.c_str(), action.down ? "down" : "up");
        }
    }
    if (!editorStatus.empty()) {
        ImGui::TextWrapped("%s", editorStatus.c_str());
    }
    ImGui::End();
}
