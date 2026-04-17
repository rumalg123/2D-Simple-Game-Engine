#include "Engine.h"

#include "SceneSerializer.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
constexpr float FixedDeltaTime = 1.0f / 60.0f;
constexpr float MaxFrameTime = 0.25f;

float clampValue(float value, float minValue, float maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

int clampInt(int value, int minValue, int maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

std::string describeActionBindings(const InputAction& action) {
    std::string description;
    auto append = [&description](const std::string& value) {
        if (!description.empty()) {
            description += ", ";
        }
        description += value;
    };

    for (int key : action.keys) {
        append("key " + std::to_string(key));
    }
    for (int button : action.gamepadButtons) {
        append("pad button " + std::to_string(button));
    }
    for (const GamepadAxisBinding& binding : action.gamepadAxes) {
        append(
            "pad axis " + std::to_string(binding.axis) +
            (binding.direction == GamepadAxisDirection::Negative ? " -" : " +"));
    }

    return description.empty() ? "unbound" : description;
}

std::array<unsigned char, 7> glyphRowsFor(char character) {
    switch (static_cast<char>(std::toupper(static_cast<unsigned char>(character)))) {
        case 'A': return {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
        case 'B': return {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
        case 'C': return {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
        case 'D': return {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
        case 'E': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
        case 'F': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
        case 'G': return {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E};
        case 'H': return {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
        case 'I': return {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E};
        case 'J': return {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C};
        case 'K': return {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
        case 'L': return {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
        case 'M': return {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
        case 'N': return {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
        case 'O': return {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
        case 'P': return {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
        case 'Q': return {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
        case 'R': return {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
        case 'S': return {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
        case 'T': return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
        case 'U': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
        case 'V': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
        case 'W': return {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A};
        case 'X': return {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
        case 'Y': return {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
        case 'Z': return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};
        case '0': return {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
        case '1': return {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
        case '2': return {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
        case '3': return {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E};
        case '4': return {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
        case '5': return {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E};
        case '6': return {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E};
        case '7': return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
        case '8': return {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
        case '9': return {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E};
        case ':': return {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00};
        case '.': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};
        case '-': return {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
        case '/': return {0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10};
        case '!': return {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04};
        case '?': return {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04};
        default: return {0x00, 0x00, 0x0E, 0x02, 0x06, 0x00, 0x04};
    }
}

bool isTextureAsset(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" ||
           extension == ".tga" || extension == ".ppm";
}

void editStringField(const char* label, std::string& value) {
    std::array<char, 64> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
    if (ImGui::InputText(label, buffer.data(), buffer.size())) {
        value = buffer.data();
    }
}

std::string sanitizeAssetName(std::string value) {
    for (char& character : value) {
        const bool valid =
            std::isalnum(static_cast<unsigned char>(character)) || character == '_' || character == '-';
        if (!valid) {
            character = '_';
        }
    }

    if (value.empty()) {
        value = "Entity";
    }

    return value;
}

void getCameraHalfSizeForDisplay(const Camera& camera, float displayWidth, float displayHeight, float& halfWidth, float& halfHeight) {
    halfWidth = camera.halfWidth;
    halfHeight = camera.halfHeight;

    const float viewportAspect = displayHeight > 0.0f ? displayWidth / displayHeight : 1.0f;
    const float cameraAspect = halfHeight != 0.0f ? halfWidth / halfHeight : 1.0f;
    if (viewportAspect > cameraAspect) {
        halfWidth = halfHeight * viewportAspect;
    } else {
        halfHeight = halfWidth / viewportAspect;
    }
}

ImVec2 worldToScreen(const TransformComponent& transform, const Camera& camera, const ImVec2& displaySize) {
    float cameraHalfWidth = 1.0f;
    float cameraHalfHeight = 1.0f;
    getCameraHalfSizeForDisplay(camera, displaySize.x, displaySize.y, cameraHalfWidth, cameraHalfHeight);

    const float normalizedX = (transform.x - camera.x) / cameraHalfWidth;
    const float normalizedY = (transform.y - camera.y) / cameraHalfHeight;
    return {
        (normalizedX * 0.5f + 0.5f) * displaySize.x,
        (0.5f - normalizedY * 0.5f) * displaySize.y
    };
}

TransformComponent screenToWorld(const ImVec2& screenPosition, const Camera& camera, const ImVec2& displaySize) {
    float cameraHalfWidth = 1.0f;
    float cameraHalfHeight = 1.0f;
    getCameraHalfSizeForDisplay(camera, displaySize.x, displaySize.y, cameraHalfWidth, cameraHalfHeight);

    const float normalizedX = (screenPosition.x / displaySize.x) * 2.0f - 1.0f;
    const float normalizedY = 1.0f - (screenPosition.y / displaySize.y) * 2.0f;
    return {
        camera.x + normalizedX * cameraHalfWidth,
        camera.y + normalizedY * cameraHalfHeight
    };
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    Engine* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    if (engine) {
        engine->onFramebufferResized(width, height);
    }
}

void keyCallback(GLFWwindow* window, int key, int, int action, int) {
    if (action != GLFW_PRESS && action != GLFW_RELEASE) {
        return;
    }

    Engine* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    if (engine) {
        engine->onKeyChanged(key, action);
    }
}

struct OwnedImGuiDrawData {
    ImDrawData drawData;
    std::vector<ImDrawList*> ownedLists;

    ~OwnedImGuiDrawData() {
        for (ImDrawList* drawList : ownedLists) {
            IM_DELETE(drawList);
        }
    }
};

std::shared_ptr<OwnedImGuiDrawData> cloneImGuiDrawData(ImDrawData* source) {
    auto owned = std::make_shared<OwnedImGuiDrawData>();
    owned->drawData.Clear();
    owned->drawData.DisplayPos = source->DisplayPos;
    owned->drawData.DisplaySize = source->DisplaySize;
    owned->drawData.FramebufferScale = source->FramebufferScale;
    owned->drawData.OwnerViewport = source->OwnerViewport;
    owned->drawData.Textures = source->Textures;

    owned->ownedLists.reserve(source->CmdListsCount);
    for (int index = 0; index < source->CmdListsCount; ++index) {
        ImDrawList* clonedList = source->CmdLists[index]->CloneOutput();
        owned->ownedLists.push_back(clonedList);
        owned->drawData.CmdLists.push_back(clonedList);
        owned->drawData.TotalVtxCount += clonedList->VtxBuffer.Size;
        owned->drawData.TotalIdxCount += clonedList->IdxBuffer.Size;
    }

    owned->drawData.CmdListsCount = owned->drawData.CmdLists.Size;
    owned->drawData.Valid = source->Valid;
    return owned;
}
}

Engine::Engine(std::unique_ptr<IGame> nextGame, GameConfig nextConfig)
    : game(std::move(nextGame)), config(std::move(nextConfig)) {
    isPlaying = config.startPlaying;
    hotReloadEnabled = config.hotReload;
}

bool Engine::init() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW.\n";
        return false;
    }

    window = glfwCreateWindow(config.windowWidth, config.windowHeight, config.windowTitle.c_str(), nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window.\n";
        glfwTerminate();
        return false;
    }

    glContext.attach(window);
    glContext.makeCurrent();
    renderCommands.setContext(&glContext);
    glfwSwapInterval(config.vsync ? 1 : 0);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetKeyCallback(window, keyCallback);

    if (!renderer.init(config.windowWidth, config.windowHeight)) {
        std::cerr << "Failed to initialize renderer.\n";
        cleanup();
        return false;
    }

    if (config.editorEnabled && !initImGui()) {
        std::cerr << "Failed to initialize ImGui.\n";
        cleanup();
        return false;
    }

    jobSystem.start();
    initInputMap();
    initScripts();
    initScene();
    initPlugins();
    glContext.release();
    renderCommands.start();
    return true;
}

void Engine::run() {
    float lastFrame = 0.0f;
    float accumulatedTime = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        const float currentFrame = static_cast<float>(glfwGetTime());
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        if (deltaTime > MaxFrameTime) {
            deltaTime = MaxFrameTime;
        }
        accumulatedTime += deltaTime;

        glfwPollEvents();
        eventSystem();
        updateGamepadInput();

        if (isPlaying) {
            while (accumulatedTime >= FixedDeltaTime) {
                inputSystem();
                scriptSystem.update(scene, FixedDeltaTime, &prefabs, &inputMap);
                if (game) {
                    GameContext gameContext = makeGameContext();
                    game->onFixedUpdate(gameContext, FixedDeltaTime);
                }
                spriteAnimationSystem(FixedDeltaTime);
                PluginContext pluginContext = makePluginContext();
                pluginManager.update(pluginContext, FixedDeltaTime);
                physicsSystem.update(scene, eventQueue, FixedDeltaTime, &jobSystem);
                lifetimeSystem(FixedDeltaTime);
                accumulatedTime -= FixedDeltaTime;
            }
        } else {
            accumulatedTime = 0.0f;
        }

        cameraSystem();
        hotReloadSystem();
        audioSystem.update();
        renderCommands.waitUntilIdle();
        if (imguiInitialized) {
            beginUiFrame();
            handleGizmo();
            renderUi();
            renderSystem();
            endUiFrame();
        } else {
            renderSystem();
            presentRuntimeFrame();
        }
    }

    renderCommands.waitUntilIdle();
}

void Engine::onFramebufferResized(int width, int height) {
    submitRenderCommand("Resize viewport", [this, width, height]() {
        renderer.setViewport(width, height);
    });
}

void Engine::onKeyChanged(int key, int action) {
    eventQueue.publishKeyChanged(key, action == GLFW_PRESS);
}

GameContext Engine::makeGameContext() {
    return {&scene, &resources, &prefabs, &scriptSystem, &inputMap, &audioSystem, &editorStatus};
}

PluginContext Engine::makePluginContext() {
    return {&scene, &resources, &prefabs, &scriptSystem, &inputMap, &editorStatus};
}

void Engine::notifyGameSceneLoaded() {
    if (!game) {
        return;
    }

    GameContext gameContext = makeGameContext();
    game->onSceneLoaded(gameContext);
}

void Engine::initScene() {
    clearGpuTextureCache();
    resources.clear();
    scene.clear();
    prefabs.clear();
    if (game) {
        GameContext gameContext = makeGameContext();
        game->loadInitialScene(gameContext);
    }
    std::size_t loadedPrefabCount = 0;
    std::string prefabError;
    if (!loadPrefabDirectoryFromJson(prefabs, resources, config.prefabDirectory, loadedPrefabCount, prefabError)) {
        editorStatus = prefabError;
    }
    syncGpuTextures(true);
    selectedEntity = scene.getPlayerEntity();
    notifyGameSceneLoaded();
    cameraSystem();
}

void Engine::initInputMap() {
    inputMap.clear();
    resetActionWasDown = false;
    if (game) {
        GameContext gameContext = makeGameContext();
        game->configureInput(gameContext);
    }
}

void Engine::initScripts() {
    scriptSystem.clear();
    if (game) {
        GameContext gameContext = makeGameContext();
        game->registerScripts(gameContext);
    }
}

void Engine::initPlugins() {
    if (game) {
        GameContext gameContext = makeGameContext();
        game->registerPlugins(gameContext, pluginManager);
    }
}

bool Engine::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = config.editorLayoutPath.c_str();

    ImGui::StyleColorsDark();

    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 120")) {
        ImGui_ImplGlfw_Shutdown();
        return false;
    }

    if (!ImGui_ImplOpenGL3_CreateDeviceObjects()) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        return false;
    }

    imguiInitialized = true;
    return true;
}

void Engine::shutdownImGui() {
    if (!imguiInitialized) {
        return;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    imguiInitialized = false;
}

void Engine::beginUiFrame() {
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Engine::renderUi() {
    renderPlayPausePanel();
    renderSceneHierarchyPanel();
    renderInspectorPanel();
    renderAssetBrowserPanel();
    PluginContext pluginContext = makePluginContext();
    pluginManager.renderUi(pluginContext);
}

void Engine::endUiFrame() {
    ImGui::Render();
    std::shared_ptr<OwnedImGuiDrawData> drawData = cloneImGuiDrawData(ImGui::GetDrawData());
    submitRenderCommand("Prepare ImGui OpenGL backend", []() {
        ImGui_ImplOpenGL3_NewFrame();
    });
    submitRenderCommand("Render ImGui draw data", [drawData]() {
        ImGui_ImplOpenGL3_RenderDrawData(&drawData->drawData);
    });
    submitRenderCommand("Swap buffers", [this]() {
        glfwSwapBuffers(window);
    });
    renderCommands.submitFrame();
}

void Engine::presentRuntimeFrame() {
    submitRenderCommand("Swap buffers", [this]() {
        glfwSwapBuffers(window);
    });
    renderCommands.submitFrame();
}

void Engine::hotReloadSystem() {
    if (!hotReloadEnabled) {
        return;
    }

    std::string error;
    if (!resources.reloadChangedTextures(error)) {
        editorStatus = error;
        return;
    }

    syncGpuTextures(false);
}

void Engine::handleGizmo() {
    if (selectedEntity == InvalidEntity || !scene.isValidEntity(selectedEntity)) {
        gizmoDragging = false;
        return;
    }

    NameComponent* name = scene.getName(selectedEntity);
    TagComponent* tag = scene.getTag(selectedEntity);
    TransformComponent* transform = scene.getTransform(selectedEntity);
    if (!transform) {
        gizmoDragging = false;
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f) {
        return;
    }

    const ImVec2 handlePosition = worldToScreen(*transform, scene.getCamera(), io.DisplaySize);
    const ImVec2 mousePosition = io.MousePos;
    const float deltaX = mousePosition.x - handlePosition.x;
    const float deltaY = mousePosition.y - handlePosition.y;
    const float handleRadius = 13.0f;
    const bool mouseOnHandle = (deltaX * deltaX + deltaY * deltaY) <= handleRadius * handleRadius;

    if (!io.WantCaptureMouse && mouseOnHandle && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        gizmoDragging = true;
    }

    if (gizmoDragging && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const TransformComponent newPosition = screenToWorld(mousePosition, scene.getCamera(), io.DisplaySize);
        transform->x = newPosition.x;
        transform->y = newPosition.y;

        PhysicsComponent* physics = scene.getPhysics(selectedEntity);
        if (physics) {
            physics->velocityX = 0.0f;
            physics->velocityY = 0.0f;
            physics->forceX = 0.0f;
            physics->forceY = 0.0f;
        }
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        gizmoDragging = false;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const ImU32 fillColor = gizmoDragging ? IM_COL32(255, 210, 70, 255) : IM_COL32(80, 220, 255, 230);
    drawList->AddCircleFilled(handlePosition, handleRadius, fillColor, 24);
    drawList->AddCircle(handlePosition, handleRadius + 4.0f, IM_COL32(255, 255, 255, 210), 24, 2.0f);
    drawList->AddLine({handlePosition.x - 28.0f, handlePosition.y}, {handlePosition.x + 28.0f, handlePosition.y}, IM_COL32(255, 255, 255, 180), 2.0f);
    drawList->AddLine({handlePosition.x, handlePosition.y - 28.0f}, {handlePosition.x, handlePosition.y + 28.0f}, IM_COL32(255, 255, 255, 180), 2.0f);
}

void Engine::renderPlayPausePanel() {
    ImGui::Begin("Engine");
    ImGui::Text("Simulation: %s", isPlaying ? "Playing" : "Paused");

    if (ImGui::Button(isPlaying ? "Pause" : "Play")) {
        isPlaying = !isPlaying;
    }
    ImGui::SameLine();
    if (ImGui::Button("Step")) {
        inputSystem();
        spriteAnimationSystem(FixedDeltaTime);
        physicsSystem.update(scene, eventQueue, FixedDeltaTime, &jobSystem);
        cameraSystem();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Scene")) {
        initScene();
    }

    if (ImGui::Button("New Empty Scene")) {
        newEmptyScene();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save Scene JSON")) {
        saveCurrentScene();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Scene JSON")) {
        loadSavedScene();
    }

    ImGui::SameLine();
    ImGui::Checkbox("Hot Reload", &hotReloadEnabled);

    ImGui::Separator();
    ImGui::Text("Game: %s", game ? game->name() : "None");
    if (game) {
        for (const GameDebugStat& stat : game->debugStats()) {
            ImGui::Text("%s: %d", stat.name.c_str(), stat.value);
        }
    }
    ImGui::Text("Entities: %zu", scene.entityCount());
    ImGui::Text("Alive entities: %zu", scene.livingEntityCount());
    ImGui::Text("Prefabs: %zu", prefabs.prefabCount());
    ImGui::Text("Scripts: %zu", scriptSystem.scriptCount());
    ImGui::Text("Plugins: %zu", pluginManager.pluginCount());
    ImGui::Text("Job workers: %zu", jobSystem.workerCount());
    ImGui::Text("Audio clips: %zu", audioSystem.clipCount());
    ImGui::Text("Audio voices: %zu", audioSystem.activeVoiceCount());
    if (game && game->controlsHelp()[0] != '\0') {
        ImGui::Text("Controls: %s", game->controlsHelp());
    }

    if (ImGui::CollapsingHeader("Renderer")) {
        const RenderStats& renderStats = renderer.getStats();
        RenderLight& light = renderer.getLight();
        ImGui::Text("Sprites: %zu", renderStats.spriteCount);
        ImGui::Text("Shadows: %zu", renderStats.shadowCount);
        ImGui::Text("Collider overlays: %zu", renderStats.colliderCount);
        ImGui::Text("Batches: %zu", renderStats.batches);
        ImGui::Text("Draw calls: %zu", renderStats.drawCalls);
        ImGui::Text("Rendered frame: %zu", renderStats.frameIndex);
        ImGui::Text("Texture metadata: %zu", renderStats.textureCount);
        ImGui::Text("GPU textures: %zu", renderStats.gpuTextureCount);
        ImGui::Text("Render passes: %zu", renderStats.renderPasses);
        ImGui::Text(
            "Instancing: %s%s",
            renderStats.instancingUsed ? "active" : "inactive",
            renderer.isInstancingAvailable() ? "" : " (unsupported)");
        const GpuResourceStats& gpuStats = renderer.getGpuResourceStats();
        ImGui::Text("Texture uploads: %zu", gpuStats.textureUploads);
        ImGui::Text("Texture deletes: %zu", gpuStats.textureDeletes);
        ImGui::Text("Render frames submitted: %zu", renderCommands.submittedFrameCount());
        ImGui::Text("Render commands last frame: %zu", renderCommands.lastExecutedCommandCount());
        ImGui::Text("Render commands queued: %zu", renderCommands.queuedCommandCount());
        ImGui::Text("Render thread: %s", renderCommands.isRunning() ? "running" : "stopped");
        ImGui::Text("Render thread id hash: %zu", renderCommands.renderThreadHash());
        ImGui::Text("GL owner id hash: %zu", glContext.ownerThreadHash());
        ImGui::Text(
            "Main thread owns GL: %s",
            glContext.isCurrentThreadOwner() ? "yes" : "no");
        ImGui::Text("Frame allocator: %zu / %zu bytes", renderer.frameScratchUsed(), renderer.frameScratchCapacity());
        ImGui::Text("Event pool: %zu / %zu", eventQueue.eventPoolUsed(), eventQueue.eventPoolCapacity());
        bool instancingEnabled = renderer.isInstancingEnabled();
        if (ImGui::Checkbox("Use Instancing", &instancingEnabled)) {
            renderer.setInstancingEnabled(instancingEnabled);
        }
        ImGui::Checkbox("Lighting", &light.enabled);
        ImGui::Checkbox("2D Shadows", &light.shadowsEnabled);
        ImGui::DragFloat2("Light Position", &light.x, 0.01f);
        ImGui::ColorEdit3("Light Color", &light.red);
        ImGui::DragFloat("Ambient", &light.ambient, 0.01f, 0.0f, 2.0f);
        ImGui::DragFloat("Radius", &light.radius, 0.01f, 0.05f, 10.0f);
        ImGui::DragFloat2("Shadow Offset", &light.shadowOffsetX, 0.01f);
        ImGui::DragFloat("Shadow Alpha", &light.shadowAlpha, 0.01f, 0.0f, 1.0f);
    }

    if (ImGui::CollapsingHeader("ECS Storage")) {
        ImGui::Text("Transforms: %zu", scene.getTransformPool().size());
        ImGui::Text("Physics bodies: %zu", scene.getPhysicsPool().size());
        ImGui::Text("Sprites: %zu", scene.getSpritePool().size());
        ImGui::Text("Sprite animations: %zu", scene.getSpriteAnimationPool().size());
        ImGui::Text("Texts: %zu", scene.getTextPool().size());
        ImGui::Text("Tilemaps: %zu", scene.getTilemapPool().size());
        ImGui::Text("Colliders: %zu", scene.getColliderPool().size());
        ImGui::Text("Inputs: %zu", scene.getInputPool().size());
        ImGui::Text("Scripts: %zu", scene.getScriptPool().size());
        ImGui::Text("Tags: %zu", scene.getTagPool().size());
        ImGui::Text("Lifetimes: %zu", scene.getLifetimePool().size());
    }

    if (ImGui::CollapsingHeader("Input Mapping")) {
        const std::string gamepadStatus = activeGamepadName.empty() ? "" : " (" + activeGamepadName + ")";
        ImGui::Text("Gamepads: %d%s", connectedGamepadCount, gamepadStatus.c_str());
        std::vector<std::string> actionNames;
        for (const auto& action : inputMap.getActions()) {
            actionNames.push_back(action.first);
        }
        std::sort(actionNames.begin(), actionNames.end());

        for (const std::string& actionName : actionNames) {
            const InputAction& action = inputMap.getActions().at(actionName);
            const std::string bindings = describeActionBindings(action);
            ImGui::Text("%s -> %s [%s]", action.name.c_str(), bindings.c_str(), action.down ? "down" : "up");
        }
    }
    if (!editorStatus.empty()) {
        ImGui::TextWrapped("%s", editorStatus.c_str());
    }
    ImGui::End();
}

void Engine::renderSceneHierarchyPanel() {
    ImGui::Begin("Scene Hierarchy");

    if (ImGui::Button("Create Empty")) {
        createEmptyEntity();
    }
    ImGui::SameLine();
    if (ImGui::Button("Duplicate") && selectedEntity != InvalidEntity) {
        duplicateEntity(selectedEntity);
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete") && selectedEntity != InvalidEntity) {
        deleteEntity(selectedEntity);
    }

    ImGui::Separator();

    const Entity player = scene.getPlayerEntity();
    for (Entity entity = 0; entity < scene.entityCount(); ++entity) {
        if (!scene.isValidEntity(entity)) {
            continue;
        }

        char label[64];
        const NameComponent* name = scene.getName(entity);
        const char* displayName = name ? name->name.c_str() : "Entity";
        if (entity == player) {
            std::snprintf(label, sizeof(label), "%s %zu (Player)", displayName, entity);
        } else {
            std::snprintf(label, sizeof(label), "%s %zu", displayName, entity);
        }

        const bool isSelected = selectedEntity == entity;
        if (ImGui::Selectable(label, isSelected)) {
            selectedEntity = entity;
        }
    }

    ImGui::End();
}

void Engine::renderInspectorPanel() {
    ImGui::Begin("Object Inspector");

    if (selectedEntity == InvalidEntity || !scene.isValidEntity(selectedEntity)) {
        ImGui::Text("No entity selected.");
        ImGui::End();
        return;
    }

    ImGui::Text("Entity %zu", selectedEntity);

    NameComponent* name = scene.getName(selectedEntity);
    TagComponent* tag = scene.getTag(selectedEntity);
    TransformComponent* transform = scene.getTransform(selectedEntity);
    PhysicsComponent* physics = scene.getPhysics(selectedEntity);
    SpriteComponent* sprite = scene.getSprite(selectedEntity);
    SpriteAnimationComponent* spriteAnimation = scene.getSpriteAnimation(selectedEntity);
    TextComponent* text = scene.getText(selectedEntity);
    TilemapComponent* tilemap = scene.getTilemap(selectedEntity);
    ColliderComponent* collider = scene.getCollider(selectedEntity);
    BoundsComponent* entityBounds = scene.getBounds(selectedEntity);
    InputComponent* input = scene.getInput(selectedEntity);
    ScriptComponent* script = scene.getScript(selectedEntity);
    LifetimeComponent* lifetime = scene.getLifetime(selectedEntity);

    if (ImGui::Button("Set Player")) {
        scene.setPlayerEntity(selectedEntity);
        scene.setTag(selectedEntity, {"Player"});
        tag = scene.getTag(selectedEntity);
        editorStatus = "Entity " + std::to_string(selectedEntity) + " is now the player.";
    }
    ImGui::SameLine();
    if (ImGui::Button("Set Camera Target")) {
        Camera camera = scene.getCamera();
        camera.target = selectedEntity;
        scene.setCamera(camera);
        editorStatus = "Camera now follows entity " + std::to_string(selectedEntity);
    }
    ImGui::SameLine();
    if (ImGui::Button("Duplicate")) {
        duplicateEntity(selectedEntity);
        ImGui::End();
        return;
    }

    if (ImGui::Button("Save As Prefab")) {
        saveSelectedPrefab();
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete Entity")) {
        deleteEntity(selectedEntity);
        ImGui::End();
        return;
    }

    ImGui::SeparatorText("Add Components");
    if (!name && ImGui::Button("Add Name")) {
        scene.setName(selectedEntity, {"Entity " + std::to_string(selectedEntity)});
        name = scene.getName(selectedEntity);
    }
    if (!tag && ImGui::Button("Add Tag")) {
        scene.setTag(selectedEntity, {"Untagged"});
        tag = scene.getTag(selectedEntity);
    }
    if (!transform && ImGui::Button("Add Transform")) {
        scene.setTransform(selectedEntity, {0.0f, 0.0f});
        transform = scene.getTransform(selectedEntity);
    }
    if (!sprite && ImGui::Button("Add Sprite")) {
        scene.setSprite(selectedEntity, {0.10f, 0.10f, 1.0f, 1.0f, 1.0f, 1.0f, 10, getEditorDefaultTexture()});
        sprite = scene.getSprite(selectedEntity);
    }
    if (!spriteAnimation && ImGui::Button("Add Sprite Animation")) {
        scene.setSpriteAnimation(selectedEntity, {4, 1, 0, 4, 0, 0.10f, 0.0f, true, true});
        spriteAnimation = scene.getSpriteAnimation(selectedEntity);
    }
    if (!text && ImGui::Button("Add Text")) {
        scene.setText(selectedEntity, {"TEXT", 0.08f, 0.12f, 0.02f, 0.04f, 1.0f, 1.0f, 1.0f, 1.0f, 100, true});
        text = scene.getText(selectedEntity);
    }
    if (!tilemap && ImGui::Button("Add Tilemap")) {
        scene.setTilemap(
            selectedEntity,
            {8, 8, 0.16f, 0.16f, 1, 1, -100, getEditorDefaultTexture(), 1.0f, 1.0f, 1.0f, 1.0f,
             std::vector<int>(64, 0)});
        tilemap = scene.getTilemap(selectedEntity);
    }
    if (!physics && ImGui::Button("Add Physics")) {
        scene.setPhysics(selectedEntity, {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, false, false, false});
        physics = scene.getPhysics(selectedEntity);
    }
    if (!collider && ImGui::Button("Add Collider")) {
        scene.setCollider(selectedEntity, {0.10f, 0.10f, true, false});
        collider = scene.getCollider(selectedEntity);
    }
    if (!entityBounds && ImGui::Button("Add Bounds")) {
        scene.setBounds(selectedEntity, {-1.0f, 1.0f, -1.0f, 1.0f});
        entityBounds = scene.getBounds(selectedEntity);
    }
    if (!input && ImGui::Button("Add Input")) {
        scene.setInput(selectedEntity, {true, "MoveLeft", "MoveRight", "MoveUp", "MoveDown", 6.0f});
        input = scene.getInput(selectedEntity);
    }
    if (!script && ImGui::Button("Add Script")) {
        scene.setScript(selectedEntity, {"", true, 0.0f});
        script = scene.getScript(selectedEntity);
    }
    if (!lifetime && ImGui::Button("Add Lifetime")) {
        scene.setLifetime(selectedEntity, {5.0f});
        lifetime = scene.getLifetime(selectedEntity);
    }

    if (name && ImGui::CollapsingHeader("Name", ImGuiTreeNodeFlags_DefaultOpen)) {
        editStringField("Display Name", name->name);
        if (ImGui::Button("Remove Name")) {
            scene.getNamePool().remove(selectedEntity);
            name = nullptr;
        }
    }

    if (tag && ImGui::CollapsingHeader("Tag")) {
        editStringField("Tag", tag->tag);
        if (ImGui::Button("Remove Tag")) {
            scene.getTagPool().remove(selectedEntity);
            tag = nullptr;
        }
    }

    if (transform && ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("X", &transform->x, 0.01f);
        ImGui::DragFloat("Y", &transform->y, 0.01f);
        if (ImGui::Button("Remove Transform")) {
            scene.getTransformPool().remove(selectedEntity);
            transform = nullptr;
        }
    }

    if (sprite && ImGui::CollapsingHeader("Sprite", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Half Width", &sprite->halfWidth, 0.01f, 0.01f, 5.0f);
        ImGui::DragFloat("Half Height", &sprite->halfHeight, 0.01f, 0.01f, 5.0f);
        ImGui::ColorEdit4("Tint", &sprite->red);
        ImGui::InputInt("Layer", &sprite->layer);
        ImGui::DragFloat("U Min", &sprite->uMin, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("V Min", &sprite->vMin, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("U Max", &sprite->uMax, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("V Max", &sprite->vMax, 0.01f, 0.0f, 1.0f);
        ImGui::Text("Texture handle: %zu", sprite->texture);
        if (ImGui::Button("Reset UVs")) {
            sprite->uMin = 0.0f;
            sprite->vMin = 0.0f;
            sprite->uMax = 1.0f;
            sprite->vMax = 1.0f;
        }
        if (ImGui::Button("Use Default White Texture")) {
            sprite->texture = getEditorDefaultTexture();
        }
        if (ImGui::Button("Remove Sprite")) {
            scene.getSpritePool().remove(selectedEntity);
            sprite = nullptr;
        }
    }

    if (spriteAnimation && ImGui::CollapsingHeader("Sprite Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Playing", &spriteAnimation->playing);
        ImGui::Checkbox("Loop", &spriteAnimation->loop);
        ImGui::InputInt("Columns", &spriteAnimation->columns);
        ImGui::InputInt("Rows", &spriteAnimation->rows);
        ImGui::InputInt("First Frame", &spriteAnimation->firstFrame);
        ImGui::InputInt("Frame Count", &spriteAnimation->frameCount);
        ImGui::InputInt("Current Frame", &spriteAnimation->currentFrame);
        ImGui::DragFloat("Seconds Per Frame", &spriteAnimation->secondsPerFrame, 0.01f, 0.01f, 10.0f);
        ImGui::DragFloat("Elapsed Seconds", &spriteAnimation->elapsedSeconds, 0.01f, 0.0f, 10.0f);
        if (ImGui::Button("Restart Animation")) {
            spriteAnimation->currentFrame = 0;
            spriteAnimation->elapsedSeconds = 0.0f;
            spriteAnimation->playing = true;
        }
        if (ImGui::Button("Remove Sprite Animation")) {
            scene.getSpriteAnimationPool().remove(selectedEntity);
            spriteAnimation = nullptr;
        }
    }

    if (text && ImGui::CollapsingHeader("Text", ImGuiTreeNodeFlags_DefaultOpen)) {
        editStringField("Text", text->text);
        ImGui::DragFloat("Character Width", &text->characterWidth, 0.01f, 0.01f, 2.0f);
        ImGui::DragFloat("Character Height", &text->characterHeight, 0.01f, 0.01f, 2.0f);
        ImGui::DragFloat("Character Spacing", &text->characterSpacing, 0.005f, 0.0f, 1.0f);
        ImGui::DragFloat("Line Spacing", &text->lineSpacing, 0.005f, 0.0f, 1.0f);
        ImGui::ColorEdit4("Text Color", &text->red);
        ImGui::InputInt("Text Layer", &text->layer);
        ImGui::Checkbox("Screen Space", &text->screenSpace);
        if (ImGui::Button("Remove Text")) {
            scene.getTextPool().remove(selectedEntity);
            text = nullptr;
        }
    }

    if (tilemap && ImGui::CollapsingHeader("Tilemap", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputInt("Columns", &tilemap->columns);
        ImGui::InputInt("Rows", &tilemap->rows);
        ImGui::DragFloat("Tile Width", &tilemap->tileWidth, 0.01f, 0.01f, 5.0f);
        ImGui::DragFloat("Tile Height", &tilemap->tileHeight, 0.01f, 0.01f, 5.0f);
        ImGui::InputInt("Atlas Columns", &tilemap->atlasColumns);
        ImGui::InputInt("Atlas Rows", &tilemap->atlasRows);
        ImGui::InputInt("Tile Layer", &tilemap->layer);
        ImGui::ColorEdit4("Tile Tint", &tilemap->red);
        ImGui::Text("Texture handle: %zu", tilemap->texture);
        if (ImGui::Button("Use Default Tile Texture")) {
            tilemap->texture = getEditorDefaultTexture();
        }
        ImGui::SameLine();
        if (ImGui::Button("Resize Tiles")) {
            tilemap->columns = std::max(1, tilemap->columns);
            tilemap->rows = std::max(1, tilemap->rows);
            tilemap->tiles.resize(static_cast<std::size_t>(tilemap->columns * tilemap->rows), 0);
        }
        if (ImGui::Button("Fill Tile 0")) {
            tilemap->columns = std::max(1, tilemap->columns);
            tilemap->rows = std::max(1, tilemap->rows);
            tilemap->tiles.assign(static_cast<std::size_t>(tilemap->columns * tilemap->rows), 0);
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Tiles")) {
            tilemap->columns = std::max(1, tilemap->columns);
            tilemap->rows = std::max(1, tilemap->rows);
            tilemap->tiles.assign(static_cast<std::size_t>(tilemap->columns * tilemap->rows), -1);
        }
        ImGui::SameLine();
        if (ImGui::Button("Checker")) {
            tilemap->columns = std::max(1, tilemap->columns);
            tilemap->rows = std::max(1, tilemap->rows);
            tilemap->tiles.resize(static_cast<std::size_t>(tilemap->columns * tilemap->rows), 0);
            const int tileChoices = std::max(1, tilemap->atlasColumns * tilemap->atlasRows);
            for (int row = 0; row < tilemap->rows; ++row) {
                for (int column = 0; column < tilemap->columns; ++column) {
                    tilemap->tiles[static_cast<std::size_t>(row * tilemap->columns + column)] =
                        (row + column) % std::min(2, tileChoices);
                }
            }
        }

        tilemap->columns = std::max(1, tilemap->columns);
        tilemap->rows = std::max(1, tilemap->rows);
        const int editableRows = std::min(tilemap->rows, 8);
        const int editableColumns = std::min(tilemap->columns, 16);
        if (static_cast<int>(tilemap->tiles.size()) < tilemap->columns * tilemap->rows) {
            tilemap->tiles.resize(static_cast<std::size_t>(tilemap->columns * tilemap->rows), 0);
        }
        ImGui::Text("Tile indices (-1 hides tile). Showing %d x %d.", editableColumns, editableRows);
        ImGui::PushItemWidth(42.0f);
        for (int row = 0; row < editableRows; ++row) {
            for (int column = 0; column < editableColumns; ++column) {
                const std::size_t tileIndex = static_cast<std::size_t>(row * tilemap->columns + column);
                ImGui::PushID(static_cast<int>(tileIndex));
                ImGui::InputInt("##tile", &tilemap->tiles[tileIndex], 0, 0);
                ImGui::PopID();
                if (column + 1 < editableColumns) {
                    ImGui::SameLine();
                }
            }
        }
        ImGui::PopItemWidth();

        if (ImGui::Button("Remove Tilemap")) {
            scene.getTilemapPool().remove(selectedEntity);
            tilemap = nullptr;
        }
    }

    if (physics && ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Velocity X", &physics->velocityX, 0.01f);
        ImGui::DragFloat("Velocity Y", &physics->velocityY, 0.01f);
        ImGui::DragFloat("Force X", &physics->forceX, 0.01f);
        ImGui::DragFloat("Force Y", &physics->forceY, 0.01f);
        ImGui::DragFloat("Inverse Mass", &physics->inverseMass, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("Drag", &physics->drag, 0.01f, 0.0f, 20.0f);
        ImGui::DragFloat("Restitution", &physics->restitution, 0.01f, 0.0f, 1.0f);
        ImGui::Checkbox("Static", &physics->isStatic);
        ImGui::Checkbox("Use World Bounds", &physics->usesWorldBounds);
        ImGui::Checkbox("Bounce At Bounds", &physics->bounceAtBounds);
        if (ImGui::Button("Remove Physics")) {
            scene.getPhysicsPool().remove(selectedEntity);
            physics = nullptr;
        }
    }

    if (collider && ImGui::CollapsingHeader("Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Collider Half Width", &collider->halfWidth, 0.01f, 0.01f, 5.0f);
        ImGui::DragFloat("Collider Half Height", &collider->halfHeight, 0.01f, 0.01f, 5.0f);
        ImGui::Checkbox("Solid", &collider->solid);
        ImGui::Checkbox("Trigger", &collider->trigger);
        if (ImGui::Button("Remove Collider")) {
            scene.getColliderPool().remove(selectedEntity);
            collider = nullptr;
        }
    }

    if (entityBounds && ImGui::CollapsingHeader("Bounds")) {
        ImGui::DragFloat("Min X", &entityBounds->minX, 0.01f);
        ImGui::DragFloat("Max X", &entityBounds->maxX, 0.01f);
        ImGui::DragFloat("Min Y", &entityBounds->minY, 0.01f);
        ImGui::DragFloat("Max Y", &entityBounds->maxY, 0.01f);
        if (ImGui::Button("Remove Bounds")) {
            scene.getBoundsPool().remove(selectedEntity);
            entityBounds = nullptr;
        }
    }

    if (input && ImGui::CollapsingHeader("Input")) {
        ImGui::Checkbox("Enabled", &input->enabled);
        ImGui::DragFloat("Move Force", &input->moveForce, 0.1f, 0.0f, 50.0f);
        editStringField("Left Action", input->moveLeftAction);
        editStringField("Right Action", input->moveRightAction);
        editStringField("Up Action", input->moveUpAction);
        editStringField("Down Action", input->moveDownAction);
        if (ImGui::Button("Remove Input")) {
            scene.getInputPool().remove(selectedEntity);
            input = nullptr;
        }
    }

    if (script && ImGui::CollapsingHeader("Script")) {
        ImGui::Checkbox("Active", &script->active);
        editStringField("Script Name", script->scriptName);
        scriptSystem.applyDefaultParameters(*script);
        const char* currentScriptName = script->scriptName.empty() ? "<none>" : script->scriptName.c_str();
        if (ImGui::BeginCombo("Registered Script", currentScriptName)) {
            for (const std::string& scriptName : scriptSystem.getScriptNames()) {
                const bool selected = script->scriptName == scriptName;
                if (ImGui::Selectable(scriptName.c_str(), selected)) {
                    script->scriptName = scriptName;
                    scriptSystem.applyDefaultParameters(*script);
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::Text("Elapsed: %.2f", script->elapsedTime);
        if (ImGui::Button("Reset Script Time")) {
            script->elapsedTime = 0.0f;
        }

        const std::vector<ScriptParameterDefinition>* parameterDefinitions =
            scriptSystem.getParameterDefinitions(script->scriptName);
        if (parameterDefinitions && !parameterDefinitions->empty()) {
            ImGui::SeparatorText("Parameters");
            if (ImGui::Button("Apply Missing Defaults")) {
                scriptSystem.applyDefaultParameters(*script);
            }

            for (const ScriptParameterDefinition& parameter : *parameterDefinitions) {
                float& value = script->parameters[parameter.name];
                ImGui::DragFloat(parameter.name.c_str(), &value, 0.01f, parameter.minValue, parameter.maxValue);
            }

            std::vector<std::string> customParameters;
            for (const auto& parameter : script->parameters) {
                const bool defined = std::any_of(
                    parameterDefinitions->begin(),
                    parameterDefinitions->end(),
                    [&parameter](const ScriptParameterDefinition& definition) {
                        return definition.name == parameter.first;
                    });
                if (!defined) {
                    customParameters.push_back(parameter.first);
                }
            }
            std::sort(customParameters.begin(), customParameters.end());

            if (!customParameters.empty()) {
                ImGui::SeparatorText("Custom Parameters");
                std::string parameterToRemove;
                for (const std::string& parameterName : customParameters) {
                    ImGui::PushID(parameterName.c_str());
                    ImGui::DragFloat(parameterName.c_str(), &script->parameters[parameterName], 0.01f);
                    ImGui::SameLine();
                    if (ImGui::Button("Remove")) {
                        parameterToRemove = parameterName;
                    }
                    ImGui::PopID();
                }
                if (!parameterToRemove.empty()) {
                    script->parameters.erase(parameterToRemove);
                }
            }
        }

        if (ImGui::Button("Remove Script")) {
            scene.getScriptPool().remove(selectedEntity);
            script = nullptr;
        }
    }

    if (lifetime && ImGui::CollapsingHeader("Lifetime")) {
        ImGui::DragFloat("Remaining Seconds", &lifetime->remainingSeconds, 0.05f, 0.0f, 60.0f);
        if (ImGui::Button("Remove Lifetime")) {
            scene.getLifetimePool().remove(selectedEntity);
            lifetime = nullptr;
        }
    }

    ImGui::End();
}

void Engine::renderAssetBrowserPanel() {
    ImGui::Begin("Asset Browser");

    if (ImGui::Button("Save Scene")) {
        saveCurrentScene();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Scene")) {
        loadSavedScene();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save Selected Prefab")) {
        saveSelectedPrefab();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Prefab Assets")) {
        reloadPrefabAssets();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload File Textures")) {
        std::string error;
        bool success = true;
        for (TextureHandle handle = 0; handle < resources.textureCount(); ++handle) {
            success = resources.reloadTexture(handle, error) && success;
        }
        editorStatus = success ? "Reloaded file-backed textures." : error;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto Reload", &hotReloadEnabled);

    ImGui::SeparatorText("Loaded Textures");
    for (TextureHandle handle = 0; handle < resources.textureCount(); ++handle) {
        const TextureResource& texture = resources.getTexture(handle);
        ImGui::Text(
            "%zu: %s (%dx%d, v%u)",
            handle,
            texture.name.c_str(),
            texture.width,
            texture.height,
            texture.version);
        if (!texture.sourcePath.empty()) {
            ImGui::TextDisabled("%s", texture.sourcePath.c_str());
        }
    }

    ImGui::SeparatorText("Assets");
    const std::filesystem::path assetRoot = config.assetRoot;
    if (!std::filesystem::exists(assetRoot)) {
        ImGui::Text("No assets folder.");
        ImGui::End();
        return;
    }

    std::vector<std::filesystem::directory_entry> entries;
    for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(assetRoot)) {
        if (entry.is_regular_file()) {
            entries.push_back(entry);
        }
    }

    std::sort(entries.begin(), entries.end(), [](const auto& first, const auto& second) {
        return first.path().generic_string() < second.path().generic_string();
    });

    for (const std::filesystem::directory_entry& entry : entries) {
        const std::filesystem::path path = entry.path();
        const std::string pathText = path.generic_string();
        ImGui::Text("%s", pathText.c_str());

        if (isTextureAsset(path)) {
            ImGui::SameLine();
            ImGui::PushID(pathText.c_str());
            SpriteComponent* selectedSprite =
                selectedEntity != InvalidEntity ? scene.getSprite(selectedEntity) : nullptr;
            if (ImGui::Button("Use On Selected") && selectedSprite) {
                try {
                    selectedSprite->texture = resources.loadTextureFromFile(pathText, pathText);
                    syncGpuTextures(false);
                    editorStatus = "Assigned texture: " + pathText;
                } catch (const std::exception& exception) {
                    editorStatus = exception.what();
                }
            }
            ImGui::PopID();
        }
    }

    ImGui::End();
}

void Engine::saveCurrentScene() {
    std::filesystem::create_directories(config.assetRoot);
    const std::string scenePath = (std::filesystem::path(config.assetRoot) / "scene_saved.json").generic_string();

    std::string error;
    if (saveSceneToJson(scene, resources, scenePath, error)) {
        editorStatus = "Saved scene to " + scenePath;
    } else {
        editorStatus = error;
    }
}

void Engine::loadSavedScene() {
    const std::string scenePath = (std::filesystem::path(config.assetRoot) / "scene_saved.json").generic_string();

    std::string error;
    if (!loadSceneFromJson(scene, resources, scenePath, error)) {
        editorStatus = error;
        return;
    }

    syncGpuTextures(false);
    selectedEntity = scene.getPlayerEntity();
    notifyGameSceneLoaded();
    cameraSystem();
    editorStatus = "Loaded scene from " + scenePath;
}

void Engine::saveSelectedPrefab() {
    if (selectedEntity == InvalidEntity || !scene.isValidEntity(selectedEntity)) {
        editorStatus = "Select an entity before saving a prefab.";
        return;
    }

    std::string prefabName = "Entity_" + std::to_string(selectedEntity);
    if (const NameComponent* name = scene.getName(selectedEntity)) {
        prefabName = name->name;
    }

    std::filesystem::create_directories(config.prefabDirectory);
    const std::string path =
        (std::filesystem::path(config.prefabDirectory) / (sanitizeAssetName(prefabName) + ".prefab.json")).generic_string();

    std::string error;
    if (saveEntityPrefabToJson(scene, resources, selectedEntity, path, error)) {
        editorStatus = "Saved prefab to " + path;
    } else {
        editorStatus = error;
    }
}

void Engine::reloadPrefabAssets() {
    std::filesystem::create_directories(config.prefabDirectory);

    std::size_t loadedCount = 0;
    std::string error;
    if (!loadPrefabDirectoryFromJson(prefabs, resources, config.prefabDirectory, loadedCount, error)) {
        editorStatus = error;
        return;
    }

    syncGpuTextures(false);
    editorStatus = "Loaded " + std::to_string(loadedCount) + " prefab asset(s).";
}

void Engine::newEmptyScene() {
    scene.clear();
    scene.setCamera({0.0f, 0.0f, 1.0f, 1.0f, InvalidEntity});
    selectedEntity = InvalidEntity;
    notifyGameSceneLoaded();
    editorStatus = "Created a new empty scene.";
}

Entity Engine::createEmptyEntity() {
    const Entity entity = scene.createEntity();
    scene.setName(entity, {"Entity " + std::to_string(entity)});
    scene.setTransform(entity, {0.0f, 0.0f});
    selectedEntity = entity;
    editorStatus = "Created empty entity " + std::to_string(entity);
    return entity;
}

Entity Engine::duplicateEntity(Entity source) {
    if (!scene.isValidEntity(source)) {
        editorStatus = "Select a valid entity to duplicate.";
        return InvalidEntity;
    }

    const Entity entity = scene.createEntity();

    if (const NameComponent* name = scene.getName(source)) {
        scene.setName(entity, {name->name + " Copy"});
    } else {
        scene.setName(entity, {"Entity " + std::to_string(entity)});
    }

    if (const TagComponent* tag = scene.getTag(source)) {
        scene.setTag(entity, *tag);
    }
    if (const TransformComponent* transform = scene.getTransform(source)) {
        scene.setTransform(entity, {transform->x + 0.12f, transform->y + 0.12f});
    }
    if (const PhysicsComponent* physics = scene.getPhysics(source)) {
        scene.setPhysics(entity, *physics);
    }
    if (const SpriteComponent* sprite = scene.getSprite(source)) {
        scene.setSprite(entity, *sprite);
    }
    if (const SpriteAnimationComponent* spriteAnimation = scene.getSpriteAnimation(source)) {
        scene.setSpriteAnimation(entity, *spriteAnimation);
    }
    if (const TextComponent* text = scene.getText(source)) {
        scene.setText(entity, *text);
    }
    if (const TilemapComponent* tilemap = scene.getTilemap(source)) {
        scene.setTilemap(entity, *tilemap);
    }
    if (const ColliderComponent* collider = scene.getCollider(source)) {
        scene.setCollider(entity, *collider);
    }
    if (const BoundsComponent* entityBounds = scene.getBounds(source)) {
        scene.setBounds(entity, *entityBounds);
    }
    if (const InputComponent* input = scene.getInput(source)) {
        scene.setInput(entity, *input);
    }
    if (const ScriptComponent* script = scene.getScript(source)) {
        ScriptComponent copy = *script;
        copy.elapsedTime = 0.0f;
        scene.setScript(entity, copy);
    }
    if (const LifetimeComponent* lifetime = scene.getLifetime(source)) {
        scene.setLifetime(entity, *lifetime);
    }

    selectedEntity = entity;
    editorStatus = "Duplicated entity " + std::to_string(source) + " as " + std::to_string(entity);
    return entity;
}

void Engine::deleteEntity(Entity entity) {
    if (!scene.isValidEntity(entity)) {
        editorStatus = "Select a valid entity to delete.";
        return;
    }

    scene.destroyEntity(entity);
    if (selectedEntity == entity) {
        selectedEntity = InvalidEntity;
    }
    editorStatus = "Deleted entity " + std::to_string(entity);
}

TextureHandle Engine::getEditorDefaultTexture() {
    TextureHandle texture = resources.getTextureHandle("editor_default_white");
    if (texture != InvalidTexture) {
        return texture;
    }

    texture = resources.createSolidColorTexture("editor_default_white", 255, 255, 255, 255);
    syncGpuTextures(false);
    return texture;
}

void Engine::eventSystem() {
    for (const Event& event : eventQueue.drain()) {
        if (event.type == EventType::KeyChanged) {
            const bool keyboardCaptured = imguiInitialized && ImGui::GetIO().WantCaptureKeyboard;
            if (event.key.key == GLFW_KEY_ESCAPE && event.key.pressed) {
                glfwSetWindowShouldClose(window, true);
            }

            if (!keyboardCaptured || !event.key.pressed) {
                inputMap.handleKeyChanged(event.key.key, event.key.pressed);
            }
        }

        if (game) {
            GameContext gameContext = makeGameContext();
            game->onEvent(gameContext, event);
        }
    }
}

void Engine::updateGamepadInput() {
    inputMap.clearGamepadState();
    connectedGamepadCount = 0;
    activeGamepadName.clear();

    std::array<bool, GLFW_GAMEPAD_BUTTON_LAST + 1> buttonStates{};
    std::array<float, GLFW_GAMEPAD_AXIS_LAST + 1> axisValues{};

    for (int joystick = GLFW_JOYSTICK_1; joystick <= GLFW_JOYSTICK_LAST; ++joystick) {
        if (!glfwJoystickIsGamepad(joystick)) {
            continue;
        }

        GLFWgamepadstate state{};
        if (!glfwGetGamepadState(joystick, &state)) {
            continue;
        }

        ++connectedGamepadCount;
        if (activeGamepadName.empty()) {
            const char* gamepadName = glfwGetGamepadName(joystick);
            activeGamepadName = gamepadName ? gamepadName : "connected";
        }

        for (int button = 0; button <= GLFW_GAMEPAD_BUTTON_LAST; ++button) {
            buttonStates[static_cast<std::size_t>(button)] =
                buttonStates[static_cast<std::size_t>(button)] || state.buttons[button] == GLFW_PRESS;
        }

        for (int axis = 0; axis <= GLFW_GAMEPAD_AXIS_LAST; ++axis) {
            float& value = axisValues[static_cast<std::size_t>(axis)];
            if (std::fabs(state.axes[axis]) > std::fabs(value)) {
                value = state.axes[axis];
            }
        }
    }

    for (int button = 0; button <= GLFW_GAMEPAD_BUTTON_LAST; ++button) {
        inputMap.setGamepadButtonState(button, buttonStates[static_cast<std::size_t>(button)]);
    }

    for (int axis = 0; axis <= GLFW_GAMEPAD_AXIS_LAST; ++axis) {
        inputMap.setGamepadAxisValue(axis, axisValues[static_cast<std::size_t>(axis)]);
    }

    const bool resetDown = inputMap.isDown("ResetScene");
    const bool keyboardCaptured = imguiInitialized && ImGui::GetIO().WantCaptureKeyboard;
    if (resetDown && !resetActionWasDown && !keyboardCaptured) {
        initScene();
    }
    resetActionWasDown = resetDown;
}

void Engine::inputSystem() {
    ComponentPool<InputComponent>& inputs = scene.getInputPool();

    for (std::size_t index = 0; index < inputs.size(); ++index) {
        const Entity entity = inputs.entityAt(index);
        const InputComponent& input = inputs.componentAt(index);
        PhysicsComponent* physics = scene.getPhysics(entity);
        if (!input.enabled || !physics) {
            continue;
        }

        float inputX = inputMap.getAxis(input.moveLeftAction, input.moveRightAction);
        float inputY = inputMap.getAxis(input.moveDownAction, input.moveUpAction);

        if (inputX != 0.0f && inputY != 0.0f) {
            constexpr float diagonalScale = 0.70710678f;
            inputX *= diagonalScale;
            inputY *= diagonalScale;
        }

        physics->forceX += inputX * input.moveForce;
        physics->forceY += inputY * input.moveForce;
    }
}

void Engine::spriteAnimationSystem(float deltaTime) {
    ComponentPool<SpriteAnimationComponent>& animations = scene.getSpriteAnimationPool();

    for (std::size_t index = 0; index < animations.size(); ++index) {
        const Entity entity = animations.entityAt(index);
        SpriteAnimationComponent& animation = animations.componentAt(index);

        animation.columns = std::max(1, animation.columns);
        animation.rows = std::max(1, animation.rows);
        const int totalFrames = std::max(1, animation.columns * animation.rows);
        animation.firstFrame = clampInt(animation.firstFrame, 0, totalFrames - 1);
        animation.frameCount = clampInt(animation.frameCount, 1, totalFrames - animation.firstFrame);
        animation.currentFrame = clampInt(animation.currentFrame, 0, animation.frameCount - 1);
        animation.secondsPerFrame = std::max(0.001f, animation.secondsPerFrame);

        if (animation.playing) {
            animation.elapsedSeconds += deltaTime;
            while (animation.elapsedSeconds >= animation.secondsPerFrame) {
                animation.elapsedSeconds -= animation.secondsPerFrame;

                if (animation.currentFrame + 1 < animation.frameCount) {
                    ++animation.currentFrame;
                    continue;
                }

                if (animation.loop) {
                    animation.currentFrame = 0;
                } else {
                    animation.currentFrame = animation.frameCount - 1;
                    animation.elapsedSeconds = 0.0f;
                    animation.playing = false;
                    break;
                }
            }
        }

        SpriteComponent* sprite = scene.getSprite(entity);
        if (!sprite) {
            continue;
        }

        const int frame = animation.firstFrame + animation.currentFrame;
        const int column = frame % animation.columns;
        const int row = frame / animation.columns;
        const float frameWidth = 1.0f / static_cast<float>(animation.columns);
        const float frameHeight = 1.0f / static_cast<float>(animation.rows);
        sprite->uMin = frameWidth * static_cast<float>(column);
        sprite->uMax = sprite->uMin + frameWidth;
        sprite->vMax = 1.0f - frameHeight * static_cast<float>(row);
        sprite->vMin = sprite->vMax - frameHeight;
    }
}

void Engine::lifetimeSystem(float deltaTime) {
    ComponentPool<LifetimeComponent>& lifetimes = scene.getLifetimePool();
    std::vector<Entity> expiredEntities;

    for (std::size_t index = 0; index < lifetimes.size(); ++index) {
        const Entity entity = lifetimes.entityAt(index);
        LifetimeComponent& lifetime = lifetimes.componentAt(index);
        lifetime.remainingSeconds -= deltaTime;
        if (lifetime.remainingSeconds <= 0.0f) {
            expiredEntities.push_back(entity);
        }
    }

    for (Entity entity : expiredEntities) {
        scene.destroyEntity(entity);
        if (selectedEntity == entity) {
            selectedEntity = InvalidEntity;
        }
    }
}

void Engine::cameraSystem() {
    Camera& camera = scene.getCamera();
    const TransformComponent* target = scene.getTransform(camera.target);

    if (!target) {
        camera.x = 0.0f;
        camera.y = 0.0f;
        return;
    }

    camera.x = target->x;
    camera.y = target->y;

    const BoundsComponent* cameraBounds = scene.getBounds(camera.target);
    if (cameraBounds) {
        camera.x = clampValue(target->x, cameraBounds->minX + camera.halfWidth, cameraBounds->maxX - camera.halfWidth);
        camera.y = clampValue(target->y, cameraBounds->minY + camera.halfHeight, cameraBounds->maxY - camera.halfHeight);
    }
}

void Engine::renderSystem() {
    auto packet = std::make_shared<RenderPacket>(buildRenderPacket());
    submitRenderCommand("Render scene", [this, packet]() {
        renderer.render(*packet);
    });
}

RenderPacket Engine::buildRenderPacket() {
    RenderPacket packet;
    packet.frameIndex = nextRenderFrame++;
    packet.camera = scene.getCamera();
    const ComponentPool<TextComponent>& texts = scene.getTextPool();
    const ComponentPool<TilemapComponent>& tilemaps = scene.getTilemapPool();
    const TextureHandle textTexture = texts.empty() ? InvalidTexture : getEditorDefaultTexture();

    std::size_t tileSpriteBudget = 0;
    for (std::size_t index = 0; index < tilemaps.size(); ++index) {
        const TilemapComponent& tilemap = tilemaps.componentAt(index);
        const int columns = std::max(1, tilemap.columns);
        const int rows = std::max(1, tilemap.rows);
        tileSpriteBudget += std::min(tilemap.tiles.size(), static_cast<std::size_t>(columns * rows));
    }

    packet.textures.reserve(resources.textureCount());
    for (TextureHandle handle = 0; handle < resources.textureCount(); ++handle) {
        const TextureResource& texture = resources.getTexture(handle);
        packet.textures.push_back({handle, texture.version, texture.width, texture.height});
    }

    const ComponentPool<SpriteComponent>& sprites = scene.getSpritePool();
    packet.sprites.reserve(sprites.size() + tileSpriteBudget + texts.size() * 80);

    for (std::size_t index = 0; index < tilemaps.size(); ++index) {
        const Entity entity = tilemaps.entityAt(index);
        const TransformComponent* transform = scene.getTransform(entity);
        if (!transform) {
            continue;
        }

        const TilemapComponent& tilemap = tilemaps.componentAt(index);
        if (tilemap.texture == InvalidTexture) {
            continue;
        }

        const int columns = std::max(1, tilemap.columns);
        const int rows = std::max(1, tilemap.rows);
        const int atlasColumns = std::max(1, tilemap.atlasColumns);
        const int atlasRows = std::max(1, tilemap.atlasRows);
        const int atlasTileCount = std::max(1, atlasColumns * atlasRows);
        const float tileHalfWidth = std::max(0.001f, tilemap.tileWidth) * 0.5f;
        const float tileHalfHeight = std::max(0.001f, tilemap.tileHeight) * 0.5f;
        const float atlasTileWidth = 1.0f / static_cast<float>(atlasColumns);
        const float atlasTileHeight = 1.0f / static_cast<float>(atlasRows);

        for (int row = 0; row < rows; ++row) {
            for (int column = 0; column < columns; ++column) {
                const std::size_t tileOffset = static_cast<std::size_t>(row * columns + column);
                if (tileOffset >= tilemap.tiles.size()) {
                    continue;
                }

                const int rawTile = tilemap.tiles[tileOffset];
                if (rawTile < 0) {
                    continue;
                }

                const int tile = clampInt(rawTile, 0, atlasTileCount - 1);
                const int atlasColumn = tile % atlasColumns;
                const int atlasRow = tile / atlasColumns;
                const float uMin = atlasTileWidth * static_cast<float>(atlasColumn);
                const float uMax = uMin + atlasTileWidth;
                const float vMax = 1.0f - atlasTileHeight * static_cast<float>(atlasRow);
                const float vMin = vMax - atlasTileHeight;

                const TransformComponent tileTransform{
                    transform->x + tileHalfWidth + static_cast<float>(column) * tilemap.tileWidth,
                    transform->y - tileHalfHeight - static_cast<float>(row) * tilemap.tileHeight
                };
                const SpriteComponent tileSprite{
                    tileHalfWidth,
                    tileHalfHeight,
                    tilemap.red,
                    tilemap.green,
                    tilemap.blue,
                    tilemap.alpha,
                    tilemap.layer,
                    tilemap.texture,
                    uMin,
                    vMin,
                    uMax,
                    vMax
                };
                packet.sprites.push_back({entity, tileTransform, tileSprite});
            }
        }
    }

    for (std::size_t index = 0; index < sprites.size(); ++index) {
        const Entity entity = sprites.entityAt(index);
        const TransformComponent* transform = scene.getTransform(entity);
        if (!transform) {
            continue;
        }

        packet.sprites.push_back({entity, *transform, sprites.componentAt(index)});
    }

    for (std::size_t index = 0; index < texts.size(); ++index) {
        const Entity entity = texts.entityAt(index);
        const TransformComponent* transform = scene.getTransform(entity);
        if (!transform || textTexture == InvalidTexture) {
            continue;
        }

        const TextComponent& text = texts.componentAt(index);
        const float characterWidth = std::max(0.001f, text.characterWidth);
        const float characterHeight = std::max(0.001f, text.characterHeight);
        const float cellWidth = characterWidth / 5.0f;
        const float cellHeight = characterHeight / 7.0f;

        float originX = transform->x;
        float originY = transform->y;
        if (text.screenSpace) {
            const Camera& camera = scene.getCamera();
            originX = camera.x + transform->x * camera.halfWidth;
            originY = camera.y + transform->y * camera.halfHeight;
        }

        float cursorX = 0.0f;
        float cursorY = 0.0f;
        for (char character : text.text) {
            if (character == '\n') {
                cursorX = 0.0f;
                cursorY -= characterHeight + text.lineSpacing;
                continue;
            }

            const std::array<unsigned char, 7> rows = glyphRowsFor(character);
            for (int row = 0; row < 7; ++row) {
                for (int column = 0; column < 5; ++column) {
                    const unsigned char mask = static_cast<unsigned char>(1u << (4 - column));
                    if ((rows[row] & mask) == 0) {
                        continue;
                    }

                    const TransformComponent glyphTransform{
                        originX + cursorX + cellWidth * (static_cast<float>(column) + 0.5f),
                        originY + cursorY - cellHeight * (static_cast<float>(row) + 0.5f)
                    };
                    const SpriteComponent glyphSprite{
                        cellWidth * 0.5f,
                        cellHeight * 0.5f,
                        text.red,
                        text.green,
                        text.blue,
                        text.alpha,
                        text.layer,
                        textTexture
                    };
                    packet.sprites.push_back({entity, glyphTransform, glyphSprite});
                }
            }

            cursorX += characterWidth + text.characterSpacing;
        }
    }

    const ComponentPool<ColliderComponent>& colliders = scene.getColliderPool();
    packet.colliders.reserve(colliders.size());
    for (std::size_t index = 0; index < colliders.size(); ++index) {
        const Entity entity = colliders.entityAt(index);
        const TransformComponent* transform = scene.getTransform(entity);
        if (!transform) {
            continue;
        }

        packet.colliders.push_back({entity, *transform, colliders.componentAt(index)});
    }

    return packet;
}

void Engine::clearGpuTextureCache() {
    submittedTextureVersions.clear();
    submitRenderCommand("Clear GPU texture cache", [this]() {
        renderer.clearTextureCache();
    });
}

void Engine::syncGpuTextures(bool forceAll) {
    if (forceAll) {
        submittedTextureVersions.clear();
    }

    if (submittedTextureVersions.size() < resources.textureCount()) {
        submittedTextureVersions.resize(resources.textureCount(), 0);
    }

    for (TextureHandle handle = 0; handle < resources.textureCount(); ++handle) {
        const TextureResource& texture = resources.getTexture(handle);
        if (!forceAll && submittedTextureVersions[handle] == texture.version) {
            continue;
        }

        TextureResource uploadTexture = texture;
        submitRenderCommand(
            "Upload texture",
            [this, handle, uploadTexture = std::move(uploadTexture)]() {
                renderer.uploadTexture(handle, uploadTexture);
            });
        submittedTextureVersions[handle] = texture.version;
    }

    if (submittedTextureVersions.size() > resources.textureCount()) {
        submittedTextureVersions.resize(resources.textureCount());
    }
}

void Engine::submitRenderCommand(std::string label, RenderCommandQueue::Command command) {
    renderCommands.enqueue(std::move(label), std::move(command));
}

void Engine::cleanup() {
    PluginContext pluginContext = makePluginContext();
    pluginManager.unloadAll(pluginContext);
    jobSystem.stop();
    renderCommands.stop();
    renderCommands.clear();
    if (glContext.isAttached()) {
        glContext.makeCurrent();
    }
    shutdownImGui();
    renderer.cleanup();
    audioSystem.clear();
    resources.clear();
    scene.clear();
    glContext.release();

    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }

    glfwTerminate();
}
