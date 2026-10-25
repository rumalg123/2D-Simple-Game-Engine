#pragma once

#include "AssetManifest.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

// The cache is disposable and deliberately outside Assets so exports do not
// accidentally ship authoring artifacts. Its format is versioned; the
// authoritative metadata is still Assets/asset_manifest.json.
struct ImportedArtifact {
    std::string id;
    std::string guid;
    std::string cacheKey;
    std::filesystem::path file;
    std::size_t sourceBytes = 0;
    std::uintmax_t artifactBytes = 0;
    bool cacheHit = false;
    int width = 0;
    int height = 0;
};
struct ImportBuildReport {
    std::size_t built = 0;
    std::size_t cached = 0;
    std::uintmax_t outputBytes = 0;
    std::vector<ImportedArtifact> artifacts;
    std::vector<std::string> errors;
    bool ok() const { return errors.empty(); }
};

// Source files are never modified by an import. Each build derives a key from
// source content, importer version, settings, target platform and dependency
// keys. Changes to upstream resources invalidate dependent artifacts.
class AssetImportPipeline {
public:
    static constexpr const char* ImporterVersion = "simpleengine-import-v1";
    static std::filesystem::path cacheRoot(
        const std::filesystem::path& projectRoot,
        const std::string& platform = "windows-x64");
    static ImportBuildReport build(const std::filesystem::path& projectRoot,
                                  const std::string& platform = "windows-x64");
    static bool clean(const std::filesystem::path& projectRoot,
                      const std::string& platform,
                      std::string& error);
};
