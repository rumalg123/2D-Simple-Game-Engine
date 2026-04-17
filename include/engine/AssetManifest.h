#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

enum class AssetType {
    Unknown,
    Texture,
    SpriteSheet,
    AnimationClip,
    Audio,
    Font,
    Prefab,
    Scene
};

struct SpriteSheetMetadata {
    int columns = 1;
    int rows = 1;
    float frameWidth = 0.0f;
    float frameHeight = 0.0f;
};

struct AnimationClipMetadata {
    std::string spriteSheetId;
    int firstFrame = 0;
    int frameCount = 1;
    float secondsPerFrame = 0.1f;
    bool loop = true;
};

struct FontMetadata {
    float glyphWidth = 0.0f;
    float glyphHeight = 0.0f;
};

struct AudioMetadata {
    bool streaming = false;
    float volume = 1.0f;
};

struct AssetManifestEntry {
    std::string id;
    AssetType type = AssetType::Unknown;
    std::string name;
    std::string path;
    SpriteSheetMetadata spriteSheet;
    AnimationClipMetadata animationClip;
    FontMetadata font;
    AudioMetadata audio;
};

class AssetManifest {
public:
    void clear();
    bool loadFromFile(const std::string& path, std::string& error);
    bool saveToFile(const std::string& path, std::string& error) const;

    AssetManifestEntry& importAsset(
        const std::filesystem::path& assetRoot,
        const std::filesystem::path& assetPath,
        AssetType type = AssetType::Unknown);
    std::size_t scanDirectory(const std::filesystem::path& assetRoot);

    const AssetManifestEntry* findById(const std::string& id) const;
    AssetManifestEntry* findById(const std::string& id);
    const AssetManifestEntry* findByPath(const std::string& path) const;
    AssetManifestEntry* findByPath(const std::string& path);
    const std::vector<AssetManifestEntry>& getEntries() const;
    std::size_t entryCount() const;

    static AssetType inferAssetType(const std::filesystem::path& path);
    static const char* assetTypeToString(AssetType type);
    static AssetType assetTypeFromString(const std::string& type);
    static std::filesystem::path defaultManifestPath(const std::filesystem::path& assetRoot);

private:
    std::vector<AssetManifestEntry> entries;
};
