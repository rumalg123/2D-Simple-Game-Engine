#include "Engine.h"
#include "imgui.h"

#include <algorithm>
#include <exception>
#include <fstream>
#include <system_error>
#include <filesystem>
#include <string>
#include <vector>

void Engine::renderAssetBrowserPanel() {
    ImGui::Begin("Asset Browser");
    const std::filesystem::path assetRoot = config.assetRoot;
    const std::filesystem::path manifestPath = AssetManifest::defaultManifestPath(assetRoot);

    if (ImGui::Button("Save Scene")) {
        saveCurrentScene();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Scene")) {
        loadSavedScene();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save Selected Prefab")) {
        saveSelectedPrefab();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Prefab Assets")) {
        reloadPrefabAssets();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload File Textures")) {
        std::string error;
        bool success = true;
        for (TextureHandle handle = 0; handle < resources.textureCount(); ++handle) {
            success = resources.reloadTexture(handle, error) && success;
        }
        editorStatus = success ? "Reloaded file-backed textures." : error;
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Asset Manifest")) {
        std::string error;
        if (assetManifest.loadFromFile(manifestPath.generic_string(), error)) {
            editorStatus = "Loaded asset manifest: " + manifestPath.generic_string();
        } else {
            editorStatus = error;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Scan Assets")) {
        const std::size_t added = assetManifest.scanDirectory(assetRoot);
        std::string error;
        if (assetManifest.saveToFile(manifestPath.generic_string(), error)) {
            editorStatus = "Scanned asset manifest. Added " + std::to_string(added) + " asset(s).";
        } else {
            editorStatus = error;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save Asset Manifest")) {
        std::string error;
        if (assetManifest.saveToFile(manifestPath.generic_string(), error)) {
            editorStatus = "Saved asset manifest: " + manifestPath.generic_string();
        } else {
            editorStatus = error;
        }
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto Reload", &hotReloadEnabled);

    ImGui::SeparatorText("Lua Gameplay Scripts");
    static char newScriptName[64] = "NewBehaviour";
    ImGui::InputText("Script Name", newScriptName, sizeof(newScriptName));
    if (ImGui::Button("Create Lua Script")) {
        const std::string name(newScriptName);
        if (playSession.active()) {
            editorStatus = "Stop Play Mode before creating and attaching scripts.";
        } else if (name.empty() ||
                   name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos ||
                   (name[0] >= '0' && name[0] <= '9')) {
            editorStatus = "Script names must start with a letter/underscore and contain only letters, digits and underscores.";
        } else {
            const auto scriptsDirectory = assetRoot / "Scripts";
            const auto scriptFile = scriptsDirectory / (name + ".lua");
            std::error_code ec;
            std::filesystem::create_directories(scriptsDirectory, ec);
            if (ec) {
                editorStatus = "Cannot create Scripts folder: " + ec.message();
            } else if (std::filesystem::exists(scriptFile)) {
                editorStatus = "Script already exists; refusing to overwrite " + scriptFile.string();
            } else {
                std::ofstream out(scriptFile, std::ios::binary | std::ios::trunc);
                if (!out) {
                    editorStatus = "Cannot create Lua script: " + scriptFile.string();
                } else {
                    out << "-- SimpleEngine behaviour: " << name << "\n"
                        << "function on_start()\n"
                        << "    engine.log(\"Script started\")\n"
                        << "end\n\n"
                        << "function on_update(dt)\n"
                        << "    -- engine.translate(1.0 * dt, 0)\n"
                        << "end\n";
                    out.close();
                    if (!out) {
                        editorStatus = "Failed to finish writing Lua script.";
                    } else {
                        reloadLuaScripts();
                        if (scene.isValidEntity(selectedEntity)) {
                            recordSceneEdit();
                            scene.setScript(selectedEntity, {name, true, 0.0f});
                        }
                        editorStatus = "Created script: " + scriptFile.string();
                    }
                }
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload Scripts")) reloadLuaScripts();
    ImGui::TextDisabled("Add a Script component in the Inspector to assign behaviour.");

    ImGui::SeparatorText("Loaded Textures");
    for (TextureHandle handle = 0; handle < resources.textureCount(); ++handle) {
        const TextureResource& texture = resources.getTexture(handle);
        ImGui::Text(
            "%zu: %s (%dx%d, v%u)",
            handle,
            texture.name.c_str(),
            texture.width,
            texture.height,
            texture.version);
        if (!texture.sourcePath.empty()) {
            ImGui::TextDisabled("%s", texture.sourcePath.c_str());
        }
    }

    ImGui::SeparatorText("Asset Manifest");
    ImGui::Text("Manifest: %s", manifestPath.generic_string().c_str());
    ImGui::Text("Catalog entries: %zu", assetManifest.entryCount());

    if (!std::filesystem::exists(assetRoot)) {
        ImGui::Text("No assets folder.");
        ImGui::End();
        return;
    }

    std::vector<AssetManifestEntry> entries = assetManifest.getEntries();
    std::sort(entries.begin(), entries.end(), [](const AssetManifestEntry& first, const AssetManifestEntry& second) {
        return first.id < second.id;
    });

    if (entries.empty()) {
        ImGui::Text("No cataloged assets. Scan the asset folder to import supported files.");
    }

    for (const AssetManifestEntry& entry : entries) {
        ImGui::Text(
            "%s [%s]",
            entry.id.c_str(),
            AssetManifest::assetTypeToString(entry.type));
        ImGui::TextDisabled("%s", entry.path.c_str());

        const std::string assetPath = (assetRoot / entry.path).generic_string();

        if (entry.type == AssetType::Texture || entry.type == AssetType::SpriteSheet) {
            ImGui::SameLine();
            ImGui::PushID(entry.id.c_str());
            SpriteComponent* selectedSprite =
                selectedEntity != InvalidEntity ? scene.getSprite(selectedEntity) : nullptr;
            if (ImGui::Button("Use On Selected") && selectedSprite) {
                try {
                    selectedSprite->texture = resources.loadTextureFromFile(entry.id, assetPath);
                    syncGpuTextures(false);
                    editorStatus = "Assigned texture asset: " + entry.id;
                } catch (const std::exception& exception) {
                    editorStatus = exception.what();
                }
            }
            ImGui::PopID();
        } else if (entry.type == AssetType::Scene) {
            ImGui::SameLine();
            ImGui::PushID(entry.id.c_str());
            if (ImGui::Button("Load Scene")) {
                loadSceneFromPath(assetPath);
            }
            ImGui::PopID();
        } else if (entry.type == AssetType::Prefab) {
            ImGui::SameLine();
            ImGui::PushID(entry.id.c_str());
            if (ImGui::Button("Load Prefab")) {
                loadPrefabAssetFromPath(assetPath);
            }
            ImGui::SameLine();
            if (ImGui::Button("Spawn")) {
                spawnPrefabAssetFromPath(assetPath);
            }
            ImGui::PopID();
        }
    }

    ImGui::End();
}
