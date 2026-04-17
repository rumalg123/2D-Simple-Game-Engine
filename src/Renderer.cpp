#include <glad/glad.h>

#include "Renderer.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <new>

namespace {
struct BatchVertex {
    float x;
    float y;
    float u;
    float v;
    float r;
    float g;
    float b;
    float a;
};

struct Drawable {
    std::size_t spriteIndex;
    TextureHandle texture;
    int layer;
};

struct InstanceData {
    float offsetX;
    float offsetY;
    float scaleX;
    float scaleY;
    float r;
    float g;
    float b;
    float a;
    float uOffset;
    float vOffset;
    float uScale;
    float vScale;
};

struct DrawCommand {
    TextureHandle texture;
    std::size_t firstItem;
    std::size_t itemCount;
};

struct BatchBuffer {
    BatchVertex* vertices = nullptr;
    std::size_t vertexCount = 0;
    std::size_t vertexCapacity = 0;
    DrawCommand* commands = nullptr;
    std::size_t commandCount = 0;
    std::size_t commandCapacity = 0;
};

struct InstanceBuffer {
    InstanceData* instances = nullptr;
    std::size_t instanceCount = 0;
    std::size_t instanceCapacity = 0;
    DrawCommand* commands = nullptr;
    std::size_t commandCount = 0;
    std::size_t commandCapacity = 0;
};

unsigned int compileShader(unsigned int type, const char* source, const char* name) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Failed to compile " << name << " shader:\n" << infoLog << '\n';
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

float cameraAspectSafe(float halfWidth, float halfHeight) {
    return halfHeight != 0.0f ? halfWidth / halfHeight : 1.0f;
}

void appendVertex(BatchBuffer& batch, float x, float y, float u, float v, float r, float g, float b, float a) {
    if (batch.vertexCount >= batch.vertexCapacity) {
        throw std::bad_alloc();
    }

    batch.vertices[batch.vertexCount++] = {x, y, u, v, r, g, b, a};
}

void beginCommand(BatchBuffer& batch, TextureHandle texture) {
    if (batch.commandCount > 0) {
        DrawCommand& current = batch.commands[batch.commandCount - 1];
        if (current.texture == texture) {
            return;
        }
    }

    if (batch.commandCount >= batch.commandCapacity) {
        throw std::bad_alloc();
    }

    batch.commands[batch.commandCount++] = {texture, batch.vertexCount, 0};
}

void beginCommand(InstanceBuffer& batch, TextureHandle texture) {
    if (batch.commandCount > 0) {
        DrawCommand& current = batch.commands[batch.commandCount - 1];
        if (current.texture == texture) {
            return;
        }
    }

    if (batch.commandCount >= batch.commandCapacity) {
        throw std::bad_alloc();
    }

    batch.commands[batch.commandCount++] = {texture, batch.instanceCount, 0};
}

void appendQuad(
    BatchBuffer& batch,
    TextureHandle texture,
    const TransformComponent& transform,
    float halfWidth,
    float halfHeight,
    float uMin,
    float vMin,
    float uMax,
    float vMax,
    float red,
    float green,
    float blue,
    float alpha,
    float offsetX = 0.0f,
    float offsetY = 0.0f) {
    beginCommand(batch, texture);

    const float left = transform.x - halfWidth + offsetX;
    const float right = transform.x + halfWidth + offsetX;
    const float bottom = transform.y - halfHeight + offsetY;
    const float top = transform.y + halfHeight + offsetY;

    appendVertex(batch, left, bottom, uMin, vMin, red, green, blue, alpha);
    appendVertex(batch, right, bottom, uMax, vMin, red, green, blue, alpha);
    appendVertex(batch, right, top, uMax, vMax, red, green, blue, alpha);
    appendVertex(batch, left, bottom, uMin, vMin, red, green, blue, alpha);
    appendVertex(batch, right, top, uMax, vMax, red, green, blue, alpha);
    appendVertex(batch, left, top, uMin, vMax, red, green, blue, alpha);

    batch.commands[batch.commandCount - 1].itemCount += 6;
}

void appendInstance(
    InstanceBuffer& batch,
    TextureHandle texture,
    const TransformComponent& transform,
    float halfWidth,
    float halfHeight,
    float uMin,
    float vMin,
    float uMax,
    float vMax,
    float red,
    float green,
    float blue,
    float alpha,
    float offsetX = 0.0f,
    float offsetY = 0.0f) {
    beginCommand(batch, texture);
    if (batch.instanceCount >= batch.instanceCapacity) {
        throw std::bad_alloc();
    }

    batch.instances[batch.instanceCount++] = {
        transform.x + offsetX,
        transform.y + offsetY,
        halfWidth,
        halfHeight,
        red,
        green,
        blue,
        alpha,
        uMin,
        vMin,
        uMax - uMin,
        vMax - vMin};
    batch.commands[batch.commandCount - 1].itemCount += 1;
}
}

bool Renderer::init(int initialWidth, int initialHeight) {
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD.\n";
        return false;
    }

    setViewport(initialWidth, initialHeight);

    const char* vertexShaderSource =
        "#version 120\n"
        "attribute vec2 aPosition;\n"
        "attribute vec2 aTexCoord;\n"
        "attribute vec4 aColor;\n"
        "uniform vec2 uCamera;\n"
        "uniform vec2 uCameraHalfSize;\n"
        "varying vec2 vTexCoord;\n"
        "varying vec4 vColor;\n"
        "varying vec2 vWorldPosition;\n"
        "void main() {\n"
        "    gl_Position = vec4((aPosition - uCamera) / uCameraHalfSize, 0.0, 1.0);\n"
        "    vTexCoord = aTexCoord;\n"
        "    vColor = aColor;\n"
        "    vWorldPosition = aPosition;\n"
        "}\n";

    const char* fragmentShaderSource =
        "#version 120\n"
        "uniform sampler2D uTexture;\n"
        "uniform int uLightingEnabled;\n"
        "uniform vec2 uLightPosition;\n"
        "uniform vec3 uLightColor;\n"
        "uniform float uAmbient;\n"
        "uniform float uLightRadius;\n"
        "varying vec2 vTexCoord;\n"
        "varying vec4 vColor;\n"
        "varying vec2 vWorldPosition;\n"
        "void main() {\n"
        "    vec4 baseColor = texture2D(uTexture, vTexCoord) * vColor;\n"
        "    if (uLightingEnabled != 0) {\n"
        "        float distanceToLight = distance(vWorldPosition, uLightPosition);\n"
        "        float attenuation = clamp(1.0 - (distanceToLight / max(uLightRadius, 0.0001)), 0.0, 1.0);\n"
        "        vec3 lightAmount = vec3(uAmbient) + (uLightColor * attenuation);\n"
        "        baseColor.rgb *= lightAmount;\n"
        "    }\n"
        "    gl_FragColor = baseColor;\n"
        "}\n";

    const char* instanceVertexShaderSource =
        "#version 120\n"
        "attribute vec2 aQuadPosition;\n"
        "attribute vec2 aQuadTexCoord;\n"
        "attribute vec2 aInstanceOffset;\n"
        "attribute vec2 aInstanceScale;\n"
        "attribute vec4 aInstanceColor;\n"
        "attribute vec2 aInstanceUvOffset;\n"
        "attribute vec2 aInstanceUvScale;\n"
        "uniform vec2 uCamera;\n"
        "uniform vec2 uCameraHalfSize;\n"
        "varying vec2 vTexCoord;\n"
        "varying vec4 vColor;\n"
        "varying vec2 vWorldPosition;\n"
        "void main() {\n"
        "    vec2 worldPosition = (aQuadPosition * aInstanceScale) + aInstanceOffset;\n"
        "    gl_Position = vec4((worldPosition - uCamera) / uCameraHalfSize, 0.0, 1.0);\n"
        "    vTexCoord = aInstanceUvOffset + (aQuadTexCoord * aInstanceUvScale);\n"
        "    vColor = aInstanceColor;\n"
        "    vWorldPosition = worldPosition;\n"
        "}\n";

    unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource, "vertex");
    unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource, "fragment");
    if (!vertexShader || !fragmentShader) {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return false;
    }

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    int success = 0;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "Failed to link shader program:\n" << infoLog << '\n';
        glDeleteProgram(shaderProgram);
        shaderProgram = 0;
        return false;
    }

    positionAttributeLocation = glGetAttribLocation(shaderProgram, "aPosition");
    textureCoordAttributeLocation = glGetAttribLocation(shaderProgram, "aTexCoord");
    colorAttributeLocation = glGetAttribLocation(shaderProgram, "aColor");
    cameraLocation = glGetUniformLocation(shaderProgram, "uCamera");
    cameraHalfSizeLocation = glGetUniformLocation(shaderProgram, "uCameraHalfSize");
    textureLocation = glGetUniformLocation(shaderProgram, "uTexture");
    lightingEnabledLocation = glGetUniformLocation(shaderProgram, "uLightingEnabled");
    lightPositionLocation = glGetUniformLocation(shaderProgram, "uLightPosition");
    lightColorLocation = glGetUniformLocation(shaderProgram, "uLightColor");
    ambientLocation = glGetUniformLocation(shaderProgram, "uAmbient");
    lightRadiusLocation = glGetUniformLocation(shaderProgram, "uLightRadius");
    if (positionAttributeLocation < 0 || textureCoordAttributeLocation < 0 || colorAttributeLocation < 0 ||
        cameraLocation < 0 || cameraHalfSizeLocation < 0 || textureLocation < 0 || lightingEnabledLocation < 0 ||
        lightPositionLocation < 0 || lightColorLocation < 0 || ambientLocation < 0 || lightRadiusLocation < 0) {
        std::cerr << "Failed to find required shader inputs.\n";
        glDeleteProgram(shaderProgram);
        shaderProgram = 0;
        return false;
    }

    instancingAvailable = GLAD_GL_VERSION_3_3 != 0;
    if (instancingAvailable) {
        unsigned int instanceVertexShader =
            compileShader(GL_VERTEX_SHADER, instanceVertexShaderSource, "instance vertex");
        unsigned int instanceFragmentShader =
            compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource, "instance fragment");

        if (instanceVertexShader && instanceFragmentShader) {
            instanceShaderProgram = glCreateProgram();
            glAttachShader(instanceShaderProgram, instanceVertexShader);
            glAttachShader(instanceShaderProgram, instanceFragmentShader);
            glLinkProgram(instanceShaderProgram);

            glGetProgramiv(instanceShaderProgram, GL_LINK_STATUS, &success);
            if (!success) {
                char infoLog[512];
                glGetProgramInfoLog(instanceShaderProgram, 512, nullptr, infoLog);
                std::cerr << "Failed to link instance shader program:\n" << infoLog << '\n';
                glDeleteProgram(instanceShaderProgram);
                instanceShaderProgram = 0;
                instancingAvailable = false;
            }
        } else {
            instancingAvailable = false;
        }

        glDeleteShader(instanceVertexShader);
        glDeleteShader(instanceFragmentShader);
    }

    if (instancingAvailable) {
        instanceQuadPositionLocation = glGetAttribLocation(instanceShaderProgram, "aQuadPosition");
        instanceQuadTextureCoordLocation = glGetAttribLocation(instanceShaderProgram, "aQuadTexCoord");
        instanceOffsetLocation = glGetAttribLocation(instanceShaderProgram, "aInstanceOffset");
        instanceScaleLocation = glGetAttribLocation(instanceShaderProgram, "aInstanceScale");
        instanceColorLocation = glGetAttribLocation(instanceShaderProgram, "aInstanceColor");
        instanceUvOffsetLocation = glGetAttribLocation(instanceShaderProgram, "aInstanceUvOffset");
        instanceUvScaleLocation = glGetAttribLocation(instanceShaderProgram, "aInstanceUvScale");
        instanceCameraLocation = glGetUniformLocation(instanceShaderProgram, "uCamera");
        instanceCameraHalfSizeLocation = glGetUniformLocation(instanceShaderProgram, "uCameraHalfSize");
        instanceTextureLocation = glGetUniformLocation(instanceShaderProgram, "uTexture");
        instanceLightingEnabledLocation = glGetUniformLocation(instanceShaderProgram, "uLightingEnabled");
        instanceLightPositionLocation = glGetUniformLocation(instanceShaderProgram, "uLightPosition");
        instanceLightColorLocation = glGetUniformLocation(instanceShaderProgram, "uLightColor");
        instanceAmbientLocation = glGetUniformLocation(instanceShaderProgram, "uAmbient");
        instanceLightRadiusLocation = glGetUniformLocation(instanceShaderProgram, "uLightRadius");

        if (instanceQuadPositionLocation < 0 || instanceQuadTextureCoordLocation < 0 ||
            instanceOffsetLocation < 0 || instanceScaleLocation < 0 || instanceColorLocation < 0 ||
            instanceUvOffsetLocation < 0 || instanceUvScaleLocation < 0 || instanceCameraLocation < 0 ||
            instanceCameraHalfSizeLocation < 0 || instanceTextureLocation < 0 ||
            instanceLightingEnabledLocation < 0 || instanceLightPositionLocation < 0 ||
            instanceLightColorLocation < 0 || instanceAmbientLocation < 0 || instanceLightRadiusLocation < 0) {
            std::cerr << "Failed to find required instance shader inputs. Falling back to CPU batching.\n";
            glDeleteProgram(instanceShaderProgram);
            instanceShaderProgram = 0;
            instancingAvailable = false;
        }
    }

    glGenBuffers(1, &batchVbo);
    glBindBuffer(GL_ARRAY_BUFFER, batchVbo);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    const float quadVertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f
    };

    glGenBuffers(1, &quadVbo);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glGenBuffers(1, &instanceVbo);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    return true;
}

void Renderer::setViewport(int width, int height) {
    viewportWidth = width > 0 ? width : 1;
    viewportHeight = height > 0 ? height : 1;
    glViewport(0, 0, viewportWidth, viewportHeight);
}

void Renderer::setDebugCollidersEnabled(bool enabled) {
    debugCollidersEnabled = enabled;
}

void Renderer::uploadTexture(TextureHandle handle, const TextureResource& texture) {
    gpuResources.uploadTexture(handle, texture);
}

void Renderer::clearTextureCache() {
    gpuResources.clear();
}

void Renderer::render(const RenderPacket& packet) {
    frameScratch.reset();
    renderGraph.clear();
    stats = {};
    stats.frameIndex = packet.frameIndex;
    stats.textureCount = packet.textures.size();
    stats.gpuTextureCount = gpuResources.residentTextureCount();

    renderGraph.addPass("Clear backbuffer", []() {
        glClearColor(0.1f, 0.1f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    });

    if (gpuResources.residentTextureCount() == 0) {
        renderGraph.execute();
        stats.renderPasses = renderGraph.getStats().executedPasses;
        return;
    }

    const std::size_t spriteCapacity = packet.sprites.size();
    Drawable* drawables = frameScratch.allocateArray<Drawable>(spriteCapacity);
    std::size_t drawableCount = 0;

    for (std::size_t index = 0; index < packet.sprites.size(); ++index) {
        const SpriteComponent& sprite = packet.sprites[index].sprite;
        drawables[drawableCount++] = {index, gpuResources.resolveTexture(sprite.texture), sprite.layer};
    }

    if (drawableCount > 1) {
        std::sort(drawables, drawables + drawableCount, [](const Drawable& first, const Drawable& second) {
            if (first.layer != second.layer) {
                return first.layer < second.layer;
            }

            return first.texture < second.texture;
        });
    }

    const Camera& camera = packet.camera;
    float cameraHalfWidth = camera.halfWidth;
    float cameraHalfHeight = camera.halfHeight;
    const float viewportAspect = static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);
    const float cameraAspect = cameraAspectSafe(cameraHalfWidth, cameraHalfHeight);
    if (viewportAspect > cameraAspect) {
        cameraHalfWidth = cameraHalfHeight * viewportAspect;
    } else {
        cameraHalfHeight = cameraHalfWidth / viewportAspect;
    }

    if (instancingAvailable && instancingEnabled) {
        stats.instancingUsed = true;

        glUseProgram(instanceShaderProgram);
        glUniform2f(instanceCameraLocation, camera.x, camera.y);
        glUniform2f(instanceCameraHalfSizeLocation, cameraHalfWidth, cameraHalfHeight);
        glUniform1i(instanceTextureLocation, 0);
        glUniform2f(instanceLightPositionLocation, light.x, light.y);
        glUniform3f(instanceLightColorLocation, light.red, light.green, light.blue);
        glUniform1f(instanceAmbientLocation, light.ambient);
        glUniform1f(instanceLightRadiusLocation, light.radius);

        const unsigned int quadPositionAttribute = static_cast<unsigned int>(instanceQuadPositionLocation);
        const unsigned int quadTexCoordAttribute = static_cast<unsigned int>(instanceQuadTextureCoordLocation);
        const unsigned int offsetAttribute = static_cast<unsigned int>(instanceOffsetLocation);
        const unsigned int scaleAttribute = static_cast<unsigned int>(instanceScaleLocation);
        const unsigned int colorAttribute = static_cast<unsigned int>(instanceColorLocation);
        const unsigned int uvOffsetAttribute = static_cast<unsigned int>(instanceUvOffsetLocation);
        const unsigned int uvScaleAttribute = static_cast<unsigned int>(instanceUvScaleLocation);

        glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
        glEnableVertexAttribArray(quadPositionAttribute);
        glEnableVertexAttribArray(quadTexCoordAttribute);
        glVertexAttribPointer(quadPositionAttribute, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
        glVertexAttribPointer(
            quadTexCoordAttribute,
            2,
            GL_FLOAT,
            GL_FALSE,
            4 * sizeof(float),
            reinterpret_cast<void*>(2 * sizeof(float)));

        glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
        glEnableVertexAttribArray(offsetAttribute);
        glEnableVertexAttribArray(scaleAttribute);
        glEnableVertexAttribArray(colorAttribute);
        glEnableVertexAttribArray(uvOffsetAttribute);
        glEnableVertexAttribArray(uvScaleAttribute);
        glVertexAttribDivisor(offsetAttribute, 1);
        glVertexAttribDivisor(scaleAttribute, 1);
        glVertexAttribDivisor(colorAttribute, 1);
        glVertexAttribDivisor(uvOffsetAttribute, 1);
        glVertexAttribDivisor(uvScaleAttribute, 1);

        auto bindInstancePointers =
            [offsetAttribute, scaleAttribute, colorAttribute, uvOffsetAttribute, uvScaleAttribute](
                std::size_t firstInstance) {
            const std::size_t baseOffset = firstInstance * sizeof(InstanceData);
            glVertexAttribPointer(
                offsetAttribute,
                2,
                GL_FLOAT,
                GL_FALSE,
                sizeof(InstanceData),
                reinterpret_cast<void*>(baseOffset + offsetof(InstanceData, offsetX)));
            glVertexAttribPointer(
                scaleAttribute,
                2,
                GL_FLOAT,
                GL_FALSE,
                sizeof(InstanceData),
                reinterpret_cast<void*>(baseOffset + offsetof(InstanceData, scaleX)));
            glVertexAttribPointer(
                colorAttribute,
                4,
                GL_FLOAT,
                GL_FALSE,
                sizeof(InstanceData),
                reinterpret_cast<void*>(baseOffset + offsetof(InstanceData, r)));
            glVertexAttribPointer(
                uvOffsetAttribute,
                2,
                GL_FLOAT,
                GL_FALSE,
                sizeof(InstanceData),
                reinterpret_cast<void*>(baseOffset + offsetof(InstanceData, uOffset)));
            glVertexAttribPointer(
                uvScaleAttribute,
                2,
                GL_FLOAT,
                GL_FALSE,
                sizeof(InstanceData),
                reinterpret_cast<void*>(baseOffset + offsetof(InstanceData, uScale)));
        };

        auto drawInstancedBatch = [this, &bindInstancePointers](InstanceBuffer& batch, bool enableLighting) {
            if (batch.instanceCount == 0 || batch.commandCount == 0) {
                return;
            }

            glUniform1i(instanceLightingEnabledLocation, enableLighting ? 1 : 0);
            glBufferData(
                GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(batch.instanceCount * sizeof(InstanceData)),
                batch.instances,
                GL_DYNAMIC_DRAW);

            for (std::size_t index = 0; index < batch.commandCount; ++index) {
                const DrawCommand& command = batch.commands[index];
                if (command.texture == InvalidTexture || !gpuResources.hasTexture(command.texture)) {
                    continue;
                }

                bindInstancePointers(command.firstItem);
                gpuResources.bindTexture(command.texture);
                glDrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(command.itemCount));
                ++stats.drawCalls;
            }

            stats.batches += batch.commandCount;
        };

        if (light.shadowsEnabled) {
            renderGraph.addPass("Instanced shadow pass", [&]() {
            InstanceBuffer shadowBatch;
            shadowBatch.instanceCapacity = drawableCount;
            shadowBatch.commandCapacity = drawableCount == 0 ? 0 : drawableCount;
            shadowBatch.instances = frameScratch.allocateArray<InstanceData>(shadowBatch.instanceCapacity);
            shadowBatch.commands = frameScratch.allocateArray<DrawCommand>(shadowBatch.commandCapacity);

            for (std::size_t index = 0; index < drawableCount; ++index) {
                const Drawable& drawable = drawables[index];
                const RenderSpriteCommand& renderSprite = packet.sprites[drawable.spriteIndex];
                const TransformComponent& transform = renderSprite.transform;
                const SpriteComponent& sprite = renderSprite.sprite;
                appendInstance(
                    shadowBatch,
                    drawable.texture,
                    transform,
                    sprite.halfWidth,
                    sprite.halfHeight,
                    sprite.uMin,
                    sprite.vMin,
                    sprite.uMax,
                    sprite.vMax,
                    0.0f,
                    0.0f,
                    0.0f,
                    light.shadowAlpha,
                    light.shadowOffsetX,
                    light.shadowOffsetY);
                ++stats.shadowCount;
            }

            drawInstancedBatch(shadowBatch, false);
            });
        }

        renderGraph.addPass("Instanced sprite pass", [&]() {
            InstanceBuffer spriteBatch;
            spriteBatch.instanceCapacity = drawableCount;
            spriteBatch.commandCapacity = drawableCount == 0 ? 0 : drawableCount;
            spriteBatch.instances = frameScratch.allocateArray<InstanceData>(spriteBatch.instanceCapacity);
            spriteBatch.commands = frameScratch.allocateArray<DrawCommand>(spriteBatch.commandCapacity);

            for (std::size_t index = 0; index < drawableCount; ++index) {
                const Drawable& drawable = drawables[index];
                const RenderSpriteCommand& renderSprite = packet.sprites[drawable.spriteIndex];
                const TransformComponent& transform = renderSprite.transform;
                const SpriteComponent& sprite = renderSprite.sprite;
                appendInstance(
                    spriteBatch,
                    drawable.texture,
                    transform,
                    sprite.halfWidth,
                    sprite.halfHeight,
                    sprite.uMin,
                    sprite.vMin,
                    sprite.uMax,
                    sprite.vMax,
                    sprite.red,
                    sprite.green,
                    sprite.blue,
                    sprite.alpha);
                ++stats.spriteCount;
            }

            drawInstancedBatch(spriteBatch, light.enabled);
        });

        if (debugCollidersEnabled) {
            renderGraph.addPass("Instanced debug collider pass", [&]() {
                InstanceBuffer colliderBatch;
                colliderBatch.instanceCapacity = packet.colliders.size();
                colliderBatch.commandCapacity = packet.colliders.size();
                colliderBatch.instances = frameScratch.allocateArray<InstanceData>(colliderBatch.instanceCapacity);
                colliderBatch.commands = frameScratch.allocateArray<DrawCommand>(colliderBatch.commandCapacity);

                for (std::size_t index = 0; index < packet.colliders.size(); ++index) {
                    const RenderColliderCommand& renderCollider = packet.colliders[index];
                    if (!renderCollider.collider.solid) {
                        continue;
                    }

                    appendInstance(
                        colliderBatch,
                        0,
                        renderCollider.transform,
                        renderCollider.collider.halfWidth,
                        renderCollider.collider.halfHeight,
                        0.0f,
                        0.0f,
                        1.0f,
                        1.0f,
                        1.0f,
                        0.0f,
                        0.0f,
                        0.18f);
                    ++stats.colliderCount;
                }

                drawInstancedBatch(colliderBatch, false);
            });
        }

        renderGraph.execute();
        stats.renderPasses = renderGraph.getStats().executedPasses;

        glBindTexture(GL_TEXTURE_2D, 0);
        glVertexAttribDivisor(uvScaleAttribute, 0);
        glVertexAttribDivisor(uvOffsetAttribute, 0);
        glVertexAttribDivisor(colorAttribute, 0);
        glVertexAttribDivisor(scaleAttribute, 0);
        glVertexAttribDivisor(offsetAttribute, 0);
        glDisableVertexAttribArray(uvScaleAttribute);
        glDisableVertexAttribArray(uvOffsetAttribute);
        glDisableVertexAttribArray(colorAttribute);
        glDisableVertexAttribArray(scaleAttribute);
        glDisableVertexAttribArray(offsetAttribute);
        glDisableVertexAttribArray(quadTexCoordAttribute);
        glDisableVertexAttribArray(quadPositionAttribute);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        return;
    }

    glUseProgram(shaderProgram);
    glUniform2f(cameraLocation, camera.x, camera.y);
    glUniform2f(cameraHalfSizeLocation, cameraHalfWidth, cameraHalfHeight);
    glUniform1i(textureLocation, 0);
    glUniform2f(lightPositionLocation, light.x, light.y);
    glUniform3f(lightColorLocation, light.red, light.green, light.blue);
    glUniform1f(ambientLocation, light.ambient);
    glUniform1f(lightRadiusLocation, light.radius);

    glBindBuffer(GL_ARRAY_BUFFER, batchVbo);
    const unsigned int positionAttribute = static_cast<unsigned int>(positionAttributeLocation);
    const unsigned int textureCoordAttribute = static_cast<unsigned int>(textureCoordAttributeLocation);
    const unsigned int colorAttribute = static_cast<unsigned int>(colorAttributeLocation);
    glEnableVertexAttribArray(positionAttribute);
    glEnableVertexAttribArray(textureCoordAttribute);
    glEnableVertexAttribArray(colorAttribute);
    glVertexAttribPointer(
        positionAttribute,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(BatchVertex),
        reinterpret_cast<void*>(offsetof(BatchVertex, x)));
    glVertexAttribPointer(
        textureCoordAttribute,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(BatchVertex),
        reinterpret_cast<void*>(offsetof(BatchVertex, u)));
    glVertexAttribPointer(
        colorAttribute,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(BatchVertex),
        reinterpret_cast<void*>(offsetof(BatchVertex, r)));

    auto drawBatch = [this](BatchBuffer& batch, bool enableLighting) {
        if (batch.vertexCount == 0 || batch.commandCount == 0) {
            return;
        }

        glUniform1i(lightingEnabledLocation, enableLighting ? 1 : 0);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(batch.vertexCount * sizeof(BatchVertex)),
            batch.vertices,
            GL_DYNAMIC_DRAW);

        for (std::size_t index = 0; index < batch.commandCount; ++index) {
            const DrawCommand& command = batch.commands[index];
            if (command.texture == InvalidTexture || !gpuResources.hasTexture(command.texture)) {
                continue;
            }

            gpuResources.bindTexture(command.texture);
            glDrawArrays(
                GL_TRIANGLES,
                static_cast<GLint>(command.firstItem),
                static_cast<GLsizei>(command.itemCount));
            ++stats.drawCalls;
        }

        stats.batches += batch.commandCount;
    };

    if (light.shadowsEnabled) {
        renderGraph.addPass("Batched shadow pass", [&]() {
            BatchBuffer shadowBatch;
            shadowBatch.vertexCapacity = drawableCount * 6;
            shadowBatch.commandCapacity = drawableCount == 0 ? 0 : drawableCount;
            shadowBatch.vertices = frameScratch.allocateArray<BatchVertex>(shadowBatch.vertexCapacity);
            shadowBatch.commands = frameScratch.allocateArray<DrawCommand>(shadowBatch.commandCapacity);

            for (std::size_t index = 0; index < drawableCount; ++index) {
                const Drawable& drawable = drawables[index];
                const RenderSpriteCommand& renderSprite = packet.sprites[drawable.spriteIndex];
                const TransformComponent& transform = renderSprite.transform;
                const SpriteComponent& sprite = renderSprite.sprite;
                appendQuad(
                    shadowBatch,
                    drawable.texture,
                    transform,
                    sprite.halfWidth,
                    sprite.halfHeight,
                    sprite.uMin,
                    sprite.vMin,
                    sprite.uMax,
                    sprite.vMax,
                    0.0f,
                    0.0f,
                    0.0f,
                    light.shadowAlpha,
                    light.shadowOffsetX,
                    light.shadowOffsetY);
                ++stats.shadowCount;
            }

            drawBatch(shadowBatch, false);
        });
    }

    renderGraph.addPass("Batched sprite pass", [&]() {
        BatchBuffer spriteBatch;
        spriteBatch.vertexCapacity = drawableCount * 6;
        spriteBatch.commandCapacity = drawableCount == 0 ? 0 : drawableCount;
        spriteBatch.vertices = frameScratch.allocateArray<BatchVertex>(spriteBatch.vertexCapacity);
        spriteBatch.commands = frameScratch.allocateArray<DrawCommand>(spriteBatch.commandCapacity);

        for (std::size_t index = 0; index < drawableCount; ++index) {
            const Drawable& drawable = drawables[index];
            const RenderSpriteCommand& renderSprite = packet.sprites[drawable.spriteIndex];
            const TransformComponent& transform = renderSprite.transform;
            const SpriteComponent& sprite = renderSprite.sprite;
            appendQuad(
                spriteBatch,
                drawable.texture,
                transform,
                sprite.halfWidth,
                sprite.halfHeight,
                sprite.uMin,
                sprite.vMin,
                sprite.uMax,
                sprite.vMax,
                sprite.red,
                sprite.green,
                sprite.blue,
                sprite.alpha);
            ++stats.spriteCount;
        }

        drawBatch(spriteBatch, light.enabled);
    });

    if (debugCollidersEnabled) {
        renderGraph.addPass("Batched debug collider pass", [&]() {
            BatchBuffer colliderBatch;
            colliderBatch.vertexCapacity = packet.colliders.size() * 6;
            colliderBatch.commandCapacity = packet.colliders.size();
            colliderBatch.vertices = frameScratch.allocateArray<BatchVertex>(colliderBatch.vertexCapacity);
            colliderBatch.commands = frameScratch.allocateArray<DrawCommand>(colliderBatch.commandCapacity);

            for (std::size_t index = 0; index < packet.colliders.size(); ++index) {
                const RenderColliderCommand& renderCollider = packet.colliders[index];
                if (!renderCollider.collider.solid) {
                    continue;
                }

                appendQuad(
                    colliderBatch,
                    0,
                    renderCollider.transform,
                    renderCollider.collider.halfWidth,
                    renderCollider.collider.halfHeight,
                    0.0f,
                    0.0f,
                    1.0f,
                    1.0f,
                    1.0f,
                    0.0f,
                    0.0f,
                    0.18f);
                ++stats.colliderCount;
            }

            drawBatch(colliderBatch, false);
        });
    }

    renderGraph.execute();
    stats.renderPasses = renderGraph.getStats().executedPasses;

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisableVertexAttribArray(colorAttribute);
    glDisableVertexAttribArray(textureCoordAttribute);
    glDisableVertexAttribArray(positionAttribute);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

RenderLight& Renderer::getLight() {
    return light;
}

const RenderLight& Renderer::getLight() const {
    return light;
}

const RenderStats& Renderer::getStats() const {
    return stats;
}

bool Renderer::isInstancingAvailable() const {
    return instancingAvailable;
}

bool Renderer::isInstancingEnabled() const {
    return instancingEnabled;
}

void Renderer::setInstancingEnabled(bool enabled) {
    instancingEnabled = enabled;
}

const GpuResourceStats& Renderer::getGpuResourceStats() const {
    return gpuResources.getStats();
}

std::size_t Renderer::frameScratchUsed() const {
    return frameScratch.used();
}

std::size_t Renderer::frameScratchCapacity() const {
    return frameScratch.capacity();
}

void Renderer::cleanup() {
    clearTextureCache();

    if (batchVbo) {
        glDeleteBuffers(1, &batchVbo);
        batchVbo = 0;
    }

    if (instanceVbo) {
        glDeleteBuffers(1, &instanceVbo);
        instanceVbo = 0;
    }

    if (quadVbo) {
        glDeleteBuffers(1, &quadVbo);
        quadVbo = 0;
    }

    if (instanceShaderProgram) {
        glDeleteProgram(instanceShaderProgram);
        instanceShaderProgram = 0;
    }

    if (shaderProgram) {
        glDeleteProgram(shaderProgram);
        shaderProgram = 0;
    }
}
