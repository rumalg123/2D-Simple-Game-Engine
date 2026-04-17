#pragma once

#include "Prefab.h"
#include "ResourceManager.h"
#include "Scene.h"
#include <cstddef>
#include <string>

bool saveSceneToJson(const Scene& scene, const ResourceManager& resources, const std::string& path, std::string& error);
bool saveEntityPrefabToJson(
    const Scene& scene,
    const ResourceManager& resources,
    Entity entity,
    const std::string& path,
    std::string& error);
bool loadSceneFromJson(Scene& scene, ResourceManager& resources, const std::string& path, std::string& error);
bool loadPrefabFromJson(
    PrefabRegistry& prefabs,
    ResourceManager& resources,
    const std::string& path,
    std::string& error);
bool loadPrefabDirectoryFromJson(
    PrefabRegistry& prefabs,
    ResourceManager& resources,
    const std::string& directory,
    std::size_t& loadedCount,
    std::string& error);
