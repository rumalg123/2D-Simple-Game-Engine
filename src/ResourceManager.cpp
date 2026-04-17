#include "ResourceManager.h"

#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {
std::string nextPpmToken(std::istream& stream) {
    std::string token;
    while (stream >> token) {
        if (!token.empty() && token[0] == '#') {
            std::string ignored;
            std::getline(stream, ignored);
            continue;
        }

        return token;
    }

    return "";
}

bool loadTextPpm(const std::string& path, TextureResource& texture) {
    std::ifstream file(path);
    if (!file) {
        return false;
    }

    if (nextPpmToken(file) != "P3") {
        return false;
    }

    const int width = std::stoi(nextPpmToken(file));
    const int height = std::stoi(nextPpmToken(file));
    const int maxValue = std::stoi(nextPpmToken(file));
    if (width <= 0 || height <= 0 || maxValue <= 0) {
        return false;
    }

    texture.width = width;
    texture.height = height;
    texture.pixels.clear();
    texture.pixels.reserve(width * height * 4);

    for (int pixel = 0; pixel < width * height; ++pixel) {
        const int red = std::stoi(nextPpmToken(file));
        const int green = std::stoi(nextPpmToken(file));
        const int blue = std::stoi(nextPpmToken(file));
        texture.pixels.push_back(static_cast<unsigned char>((red * 255) / maxValue));
        texture.pixels.push_back(static_cast<unsigned char>((green * 255) / maxValue));
        texture.pixels.push_back(static_cast<unsigned char>((blue * 255) / maxValue));
        texture.pixels.push_back(255);
    }

    return true;
}

bool loadPixelsFromFile(const std::string& path, TextureResource& texture, std::string& error) {
    int width = 0;
    int height = 0;
    int channelCount = 0;
    unsigned char* loadedPixels = stbi_load(path.c_str(), &width, &height, &channelCount, STBI_rgb_alpha);

    if (loadedPixels) {
        texture.width = width;
        texture.height = height;
        texture.pixels.assign(loadedPixels, loadedPixels + (width * height * 4));
        stbi_image_free(loadedPixels);
        return true;
    }

    if (loadTextPpm(path, texture)) {
        return true;
    }

    error = std::string("Failed to load texture: ") + path;
    return false;
}
}

TextureHandle ResourceManager::createSolidColorTexture(
    const std::string& name,
    unsigned char red,
    unsigned char green,
    unsigned char blue,
    unsigned char alpha) {
    const auto existing = textureHandlesByName.find(name);
    if (existing != textureHandlesByName.end()) {
        return existing->second;
    }

    TextureResource texture;
    texture.name = name;
    texture.sourcePath = "";
    texture.lastWriteTime = {};
    texture.width = 1;
    texture.height = 1;
    texture.version = 1;
    texture.fileBacked = false;
    texture.pixels = {red, green, blue, alpha};

    const TextureHandle handle = textures.size();
    textures.push_back(texture);
    textureHandlesByName[name] = handle;
    return handle;
}

TextureHandle ResourceManager::loadTextureFromFile(const std::string& name, const std::string& path) {
    const auto existing = textureHandlesByName.find(name);
    if (existing != textureHandlesByName.end()) {
        return existing->second;
    }

    TextureResource texture;
    texture.name = name;
    texture.sourcePath = path;
    texture.version = 1;
    texture.fileBacked = true;

    std::string error;
    if (!loadPixelsFromFile(path, texture, error)) {
        throw std::runtime_error(error);
    }
    texture.lastWriteTime = std::filesystem::last_write_time(path);

    const TextureHandle handle = textures.size();
    textures.push_back(texture);
    textureHandlesByName[name] = handle;
    return handle;
}

bool ResourceManager::reloadTexture(TextureHandle handle, std::string& error) {
    if (handle >= textures.size()) {
        error = "Invalid texture handle";
        return false;
    }

    TextureResource& texture = textures[handle];
    if (!texture.fileBacked) {
        return true;
    }

    TextureResource reloaded = texture;
    if (!loadPixelsFromFile(texture.sourcePath, reloaded, error)) {
        return false;
    }

    reloaded.lastWriteTime = std::filesystem::last_write_time(texture.sourcePath);
    reloaded.version = texture.version + 1;
    texture = reloaded;
    return true;
}

bool ResourceManager::reloadChangedTextures(std::string& error) {
    for (TextureHandle handle = 0; handle < textures.size(); ++handle) {
        const TextureResource& texture = textures[handle];
        if (!texture.fileBacked || !std::filesystem::exists(texture.sourcePath)) {
            continue;
        }

        const std::filesystem::file_time_type currentWriteTime =
            std::filesystem::last_write_time(texture.sourcePath);
        if (currentWriteTime != texture.lastWriteTime && !reloadTexture(handle, error)) {
            return false;
        }
    }

    return true;
}

const TextureResource& ResourceManager::getTexture(TextureHandle handle) const {
    if (handle >= textures.size()) {
        throw std::out_of_range("Invalid texture handle");
    }

    return textures[handle];
}

TextureHandle ResourceManager::getTextureHandle(const std::string& name) const {
    const auto existing = textureHandlesByName.find(name);
    if (existing == textureHandlesByName.end()) {
        return InvalidTexture;
    }

    return existing->second;
}

std::size_t ResourceManager::textureCount() const {
    return textures.size();
}

void ResourceManager::clear() {
    textures.clear();
    textureHandlesByName.clear();
}
