#pragma once

#include "AssetManifest.h"
#include "ProjectManager.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

struct AssetAudit {
    std::size_t indexed = 0;
    std::size_t changed = 0;
    std::vector<std::string> errors;
    bool valid() const { return errors.empty(); }
};

// Workspace catalog is stored at Assets/asset_manifest.json. No additional
// hidden sidecars, caches, or binary import artifacts are required to open it.
class AssetWorkspace {
public:
    explicit AssetWorkspace(std::filesystem::path root);
    bool open(std::string& error);
    bool sync(std::size_t& imported, std::string& error);
    AssetAudit audit() const;
    bool move(const std::string& id, const std::string& relativeDestination,
              std::string& error);
    const AssetManifest& index() const { return catalog_; }
private:
    std::filesystem::path root_;
    AssetManifest catalog_;
};
