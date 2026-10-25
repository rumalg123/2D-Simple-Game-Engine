#include "Engine.h"
#include "AssetWorkspace.h"
#include "AssetImportPipeline.h"
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

    ImGui::SeparatorText("Compiled Asset Pipeline");
    const auto projectRoot = assetRoot.parent_path();
    const bool independentProject =
        std::filesystem::is_regular_file(projectRoot / "project.simple.json");
    ImGui::BeginDisabled(!independentProject || playSession.active());
    if(ImGui::Button("Build / Refresh Import Cache")) {
        const auto report=AssetImportPipeline::build(projectRoot);
        if(report.ok()) {
            assetManifest.loadFromFile(manifestPath.string(),editorStatus);
            editorStatus="Imported assets: "+std::to_string(report.built)+
                " rebuilt, "+std::to_string(report.cached)+" reused; "+
                std::to_string(report.outputBytes)+" artifact bytes.";
        } else editorStatus="Asset import failed: "+report.errors.front();
    }
    ImGui::SameLine();
    if(ImGui::Button("Clean Import Cache")) {
        std::string error;
        if(AssetImportPipeline::clean(projectRoot,"windows-x64",error))
            editorStatus="Removed disposable Windows import cache.";
        else editorStatus=error;
    }
    ImGui::EndDisabled();
    ImGui::TextDisabled("Cached artifacts are outside Assets and regenerated as needed.");

    ImGui::SeparatorText("Asset Database");
    if (ImGui::Button("Audit Asset Database")) {
        AssetWorkspace workspace(assetRoot);
        std::string auditError;
        if (!workspace.open(auditError)) {
            editorStatus = auditError;
        } else {
            const AssetAudit report = workspace.audit();
            editorStatus = report.valid()
                ? ("Asset audit passed: " + std::to_string(report.indexed) +
                   " indexed assets.")
                : ("Asset audit detected " + std::to_string(report.errors.size()) +
                   " issue(s): " + report.errors.front());
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("IDs/GUIDs stay stable across managed file moves.");

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
        ImGui::PushID(entry.id.c_str());

        ImGui::Text(
            "%s [%s]",
            entry.id.c_str(),
            AssetManifest::assetTypeToString(entry.type));
        ImGui::TextDisabled("%s", entry.path.c_str());
        ImGui::TextDisabled("GUID: %s  Hash: %s  Bytes: %llu",
                            entry.guid.c_str(), entry.contentHash.c_str(),
                            static_cast<unsigned long long>(entry.sourceBytes));
        if(entry.type==AssetType::Texture) {
            if(ImGui::TreeNode("Texture Import Settings")) {
                static std::string editingGuid;
                static int maxDimension=4096;
                static bool flipY=false, premultiply=false;
                if(editingGuid!=entry.guid) {
                    editingGuid=entry.guid;
                    maxDimension=entry.textureImport.maxDimension;
                    flipY=entry.textureImport.flipY;
                    premultiply=entry.textureImport.premultiplyAlpha;
                }
                ImGui::InputInt("Max Dimension",&maxDimension);
                ImGui::Checkbox("Flip Image Vertically",&flipY);
                ImGui::Checkbox("Premultiply Alpha",&premultiply);
                if(ImGui::Button("Save Import Settings") && !playSession.active()) {
                    AssetWorkspace workspace(assetRoot);
                    std::string error;
                    TextureImportSettings settings{
                        maxDimension,flipY,premultiply
                    };
                    if(workspace.open(error) &&
                       workspace.setTextureSettings(entry.id,settings,error)) {
                        assetManifest=workspace.index();
                        editorStatus="Saved texture importer settings for "+entry.id+
                            ". Rebuild cache to apply.";
                    } else editorStatus=error;
                }
                ImGui::TreePop();
            }
        }
        if (ImGui::SmallButton("Copy GUID"))
            ImGui::SetClipboardText(entry.guid.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Move / Rename") && !playSession.active()) {
            ImGui::OpenPopup("Move Asset");
        }
        if (ImGui::BeginPopupModal("Move Asset", nullptr,
                                  ImGuiWindowFlags_AlwaysAutoResize)) {
            static char newPath[512] = {};
            ImGui::Text("Asset: %s", entry.id.c_str());
            ImGui::TextDisabled("Only supported file-backed assets; destination directory must exist.");
            ImGui::InputText("New relative asset path", newPath, sizeof(newPath));
            if(ImGui::Button("Move") && !playSession.active()) {
                AssetWorkspace workspace(assetRoot);
                std::string error;
                if(workspace.open(error) &&
                   workspace.move(entry.id,std::string(newPath),error)) {
                    assetManifest=workspace.index();
                    editorStatus="Asset moved; reopen your scene to reload moved textures.";
                    newPath[0]='\0';
                    ImGui::CloseCurrentPopup();
                } else editorStatus="Asset move rejected: "+error;
            }
            ImGui::SameLine();
            if(ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }


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
        ImGui::PopID();
    }

    ImGui::End();
}
