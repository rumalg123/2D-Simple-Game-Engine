#pragma once

#include "ECS.h"
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

struct TextureResource {
    std::string name;
    std::string sourcePath;
    std::filesystem::file_time_type lastWriteTime;
    int width;
    int height;
    unsigned int version;
    bool fileBacked;
    std::vector<unsigned char> pixels;
};

class ResourceManager {
public:
    TextureHandle createSolidColorTexture(
        const std::string& name,
        unsigned char red,
        unsigned char green,
        unsigned char blue,
        unsigned char alpha);
    TextureHandle loadTextureFromFile(const std::string& name, const std::string& path);
    bool reloadTexture(TextureHandle handle, std::string& error);
    bool reloadChangedTextures(std::string& error);

    const TextureResource& getTexture(TextureHandle handle) const;
    TextureHandle getTextureHandle(const std::string& name) const;
    std::size_t textureCount() const;
    void clear();

private:
    std::vector<TextureResource> textures;
    std::unordered_map<std::string, TextureHandle> textureHandlesByName;
};
