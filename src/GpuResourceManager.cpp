#include <glad/glad.h>

#include "GpuResourceManager.h"

void GpuResourceManager::uploadTexture(TextureHandle handle, const TextureResource& texture) {
    if (handle >= textures.size()) {
        textures.resize(handle + 1);
    }

    GpuTexture& gpuTexture = textures[handle];
    if (gpuTexture.id == 0) {
        glGenTextures(1, &gpuTexture.id);
    }

    glBindTexture(GL_TEXTURE_2D, gpuTexture.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        texture.width,
        texture.height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        texture.pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    gpuTexture.version = texture.version;
    gpuTexture.width = texture.width;
    gpuTexture.height = texture.height;

    ++stats.textureUploads;
    stats.textureSlots = textures.size();
    stats.residentTextures = residentTextureCount();
}

bool GpuResourceManager::bindTexture(TextureHandle handle) const {
    if (!hasTexture(handle)) {
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, textures[handle].id);
    return true;
}

TextureHandle GpuResourceManager::resolveTexture(TextureHandle requestedHandle) const {
    if (hasTexture(requestedHandle)) {
        return requestedHandle;
    }

    if (hasTexture(0)) {
        return 0;
    }

    return InvalidTexture;
}

bool GpuResourceManager::hasTexture(TextureHandle handle) const {
    return handle < textures.size() && textures[handle].id != 0;
}

void GpuResourceManager::clear() {
    for (GpuTexture& texture : textures) {
        if (texture.id != 0) {
            glDeleteTextures(1, &texture.id);
            texture.id = 0;
            ++stats.textureDeletes;
        }
    }

    textures.clear();
    stats.textureSlots = 0;
    stats.residentTextures = 0;
}

std::size_t GpuResourceManager::textureSlotCount() const {
    return textures.size();
}

std::size_t GpuResourceManager::residentTextureCount() const {
    std::size_t count = 0;
    for (const GpuTexture& texture : textures) {
        if (texture.id != 0) {
            ++count;
        }
    }

    return count;
}

const GpuResourceStats& GpuResourceManager::getStats() const {
    return stats;
}
