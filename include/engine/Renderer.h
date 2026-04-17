#pragma once

#include "GpuResourceManager.h"
#include "Memory.h"
#include "RenderPacket.h"
#include "RenderGraph.h"

#include <cstddef>
#include <vector>

struct RenderStats {
    std::size_t frameIndex = 0;
    std::size_t textureCount = 0;
    std::size_t gpuTextureCount = 0;
    std::size_t spriteCount = 0;
    std::size_t shadowCount = 0;
    std::size_t colliderCount = 0;
    std::size_t renderPasses = 0;
    std::size_t batches = 0;
    std::size_t drawCalls = 0;
    bool instancingUsed = false;
};

struct RenderLight {
    bool enabled = true;
    bool shadowsEnabled = true;
    float x = 0.0f;
    float y = 0.45f;
    float red = 1.0f;
    float green = 0.95f;
    float blue = 0.78f;
    float ambient = 0.35f;
    float radius = 1.85f;
    float shadowOffsetX = 0.06f;
    float shadowOffsetY = -0.08f;
    float shadowAlpha = 0.28f;
};

class Renderer {
public:
    bool init(int initialWidth, int initialHeight);
    void setViewport(int width, int height);
    void setDebugCollidersEnabled(bool enabled);
    void uploadTexture(TextureHandle handle, const TextureResource& texture);
    void clearTextureCache();
    void render(const RenderPacket& packet);
    void cleanup();

    RenderLight& getLight();
    const RenderLight& getLight() const;
    const RenderStats& getStats() const;
    bool isInstancingAvailable() const;
    bool isInstancingEnabled() const;
    void setInstancingEnabled(bool enabled);
    const GpuResourceStats& getGpuResourceStats() const;
    std::size_t frameScratchUsed() const;
    std::size_t frameScratchCapacity() const;

private:
    unsigned int shaderProgram = 0;
    unsigned int instanceShaderProgram = 0;
    unsigned int quadVbo = 0;
    unsigned int batchVbo = 0;
    unsigned int instanceVbo = 0;
    GpuResourceManager gpuResources;
    RenderGraph renderGraph;
    int viewportWidth = 1;
    int viewportHeight = 1;
    bool debugCollidersEnabled = true;
    int positionAttributeLocation = -1;
    int textureCoordAttributeLocation = -1;
    int colorAttributeLocation = -1;
    int cameraLocation = -1;
    int cameraHalfSizeLocation = -1;
    int textureLocation = -1;
    int lightingEnabledLocation = -1;
    int lightPositionLocation = -1;
    int lightColorLocation = -1;
    int ambientLocation = -1;
    int lightRadiusLocation = -1;
    int instanceQuadPositionLocation = -1;
    int instanceQuadTextureCoordLocation = -1;
    int instanceOffsetLocation = -1;
    int instanceScaleLocation = -1;
    int instanceColorLocation = -1;
    int instanceUvOffsetLocation = -1;
    int instanceUvScaleLocation = -1;
    int instanceCameraLocation = -1;
    int instanceCameraHalfSizeLocation = -1;
    int instanceTextureLocation = -1;
    int instanceLightingEnabledLocation = -1;
    int instanceLightPositionLocation = -1;
    int instanceLightColorLocation = -1;
    int instanceAmbientLocation = -1;
    int instanceLightRadiusLocation = -1;
    bool instancingAvailable = false;
    bool instancingEnabled = true;
    StackAllocator frameScratch{4u * 1024u * 1024u};
    RenderLight light;
    RenderStats stats;
};
