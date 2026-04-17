#pragma once

#include "ECS.h"
#include "ResourceManager.h"

#include <cstddef>
#include <vector>

struct GpuResourceStats {
    std::size_t textureSlots = 0;
    std::size_t residentTextures = 0;
    std::size_t textureUploads = 0;
    std::size_t textureDeletes = 0;
};

class GpuResourceManager {
public:
    void uploadTexture(TextureHandle handle, const TextureResource& texture);
    bool bindTexture(TextureHandle handle) const;
    TextureHandle resolveTexture(TextureHandle requestedHandle) const;
    bool hasTexture(TextureHandle handle) const;
    void clear();

    std::size_t textureSlotCount() const;
    std::size_t residentTextureCount() const;
    const GpuResourceStats& getStats() const;

private:
    struct GpuTexture {
        unsigned int id = 0;
        unsigned int version = 0;
        int width = 0;
        int height = 0;
    };

    std::vector<GpuTexture> textures;
    GpuResourceStats stats;
};
