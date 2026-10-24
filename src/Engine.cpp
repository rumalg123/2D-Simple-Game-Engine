#include "Engine.h"

#include "SceneSerializer.h"

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

    if (config.editorEnabled) {
#if defined(SIMPLEENGINE_PLAYER_ONLY)
        std::cerr << "This player build cannot initialize the editor.\n";
        cleanup();
        return false;
#else
        if (!initImGui()) {
            std::cerr << "Failed to initialize ImGui.\n";
            cleanup();
            return false;
        }
#endif
    }

    jobSystem.start();
    initInputMap();
    initScripts();
    initAssetManifest();
    if (!initScene()) {
        cleanup();
        return false;
    }
    initPlugins();
    if (config.editorEnabled && isPlaying) playSession.begin(scene);
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
            scriptSystem.update(scene, deltaTime, &prefabs, &inputMap);
            while (accumulatedTime >= FixedDeltaTime) {
                inputSystem();
                scriptSystem.fixedUpdate(scene, FixedDeltaTime, &prefabs, &inputMap);
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
#if !defined(SIMPLEENGINE_PLAYER_ONLY)
        if (imguiInitialized) {
            beginUiFrame();
            handleGizmo();
            renderUi();
            renderSystem();
            endUiFrame();
        } else
#endif
        {
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
    GameContext context;
    context.scene = &scene;
    context.resources = &resources;
    context.assets = &assetManifest;
    context.prefabs = &prefabs;
    context.scripts = &scriptSystem;
    context.inputMap = &inputMap;
    context.audio = &audioSystem;
    context.editorStatus = &editorStatus;
    context.reloadSceneRequest = [this]() {
        initScene();
    };
    context.clearSceneRequest = [this]() {
        newEmptyScene();
    };
    context.loadSceneRequest = [this](const std::string& scenePath) {
        return loadSceneFromPath(scenePath);
    };
    return context;
}

PluginContext Engine::makePluginContext() {
    return {&scene, &resources, &assetManifest, &prefabs, &scriptSystem, &inputMap, &editorStatus};
}

void Engine::notifyGameSceneLoaded() {
    if (!game) {
        return;
    }

    GameContext gameContext = makeGameContext();
    game->onSceneLoaded(gameContext);
}

void Engine::dispatchGameEvent(const Event& event) {
    if (!game) {
        return;
    }

    GameContext gameContext = makeGameContext();
    game->onEvent(gameContext, event);
}

void Engine::dispatchInputActionEvents() {
    for (const InputActionChange& change : inputMap.consumeActionChanges()) {
        Event event;
        event.type = EventType::ActionChanged;
        event.action = {change.actionName, change.pressed};

        if (isPlaying) {
            scriptSystem.handleEvent(scene, event, &prefabs, &inputMap);
        }

        if (change.actionName == "ResetScene" && change.pressed) {
            initScene();
        }

        dispatchGameEvent(event);
    }
}

bool Engine::initScene() {
    scriptSystem.destroyAll(scene, &prefabs, &inputMap);
    physicsSystem.clearContacts();
    clearGpuTextureCache();
    resources.clear();
    scene.clear();
    if (!playSession.active()) sceneHistory.clear();
    prefabs.clear();
    if (game) {
        GameContext gameContext = makeGameContext();
        game->loadInitialScene(gameContext);
    }
    if (!config.startupScene.empty()) {
        std::string error;
        if (!loadSceneFromJson(scene, resources, config.startupScene, error)) {
            std::cerr << "Failed to open startup scene: " << error << '\n';
            editorStatus = error;
            return false;
        }
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
    return true;
}

void Engine::initInputMap() {
    inputMap.clear();
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

void Engine::initAssetManifest() {
    assetManifest.clear();

    const std::filesystem::path assetRoot = config.assetRoot;
    if (!std::filesystem::exists(assetRoot)) {
        return;
    }

    const std::filesystem::path manifestPath = AssetManifest::defaultManifestPath(assetRoot);
    std::string error;
    const bool manifestExists = std::filesystem::exists(manifestPath);
    if (manifestExists && !assetManifest.loadFromFile(manifestPath.generic_string(), error)) {
        editorStatus = error;
        return;
    }

    const std::size_t added = assetManifest.scanDirectory(assetRoot);
    if (config.editorEnabled && ((!manifestExists && assetManifest.entryCount() > 0) || added > 0)) {
        if (!assetManifest.saveToFile(manifestPath.generic_string(), error)) {
            editorStatus = error;
        }
    }
}

void Engine::initPlugins() {
    if (game) {
        GameContext gameContext = makeGameContext();
        game->registerPlugins(gameContext, pluginManager);
    }
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

void Engine::beginPlayMode() {
    if (!config.editorEnabled || playSession.active()) return;
    if (!playSession.begin(scene)) return;
    physicsSystem.clearContacts();
    isPlaying = true;
    editorStatus = "Playing a temporary scene copy. Stop to discard changes.";
}

void Engine::stopPlayMode() {
    if (!playSession.active()) return;
    scriptSystem.destroyAll(scene, &prefabs, &inputMap);
    physicsSystem.clearContacts();
    const std::string selectedGuid = scene.guid(selectedEntity);
    isPlaying = false;
    playSession.stop(scene);
    selectedEntity = selectedGuid.empty() ? InvalidEntity
                                         : scene.findEntityByGuid(selectedGuid);
    cameraSystem();
    editorStatus = "Stopped simulation; authored scene restored.";
}

void Engine::stepPlayMode() {
    if (!playSession.active() || isPlaying) return;
    inputSystem();
    scriptSystem.fixedUpdate(scene, FixedDeltaTime, &prefabs, &inputMap);
    if (game) {
        GameContext ctx = makeGameContext();
        game->onFixedUpdate(ctx, FixedDeltaTime);
    }
    spriteAnimationSystem(FixedDeltaTime);
    PluginContext pluginCtx = makePluginContext();
    pluginManager.update(pluginCtx, FixedDeltaTime);
    physicsSystem.update(scene, eventQueue, FixedDeltaTime, &jobSystem);
    lifetimeSystem(FixedDeltaTime);
    cameraSystem();
}

void Engine::recordSceneEdit() {
    if (config.editorEnabled && !playSession.active()) sceneHistory.record(scene);
}

void Engine::undoSceneEdit() {
    if (!config.editorEnabled || playSession.active()) return;
    if (!sceneHistory.canUndo()) return;
    const std::string selectedGuid = scene.guid(selectedEntity);
    scriptSystem.destroyAll(scene, &prefabs, &inputMap);
    if (sceneHistory.undo(scene)) {
        physicsSystem.clearContacts();
        selectedEntity = selectedGuid.empty() ? InvalidEntity
                                             : scene.findEntityByGuid(selectedGuid);
        editorStatus = "Undid scene operation.";
    }
}

void Engine::redoSceneEdit() {
    if (!config.editorEnabled || playSession.active()) return;
    if (!sceneHistory.canRedo()) return;
    const std::string selectedGuid = scene.guid(selectedEntity);
    scriptSystem.destroyAll(scene, &prefabs, &inputMap);
    if (sceneHistory.redo(scene)) {
        physicsSystem.clearContacts();
        selectedEntity = selectedGuid.empty() ? InvalidEntity
                                             : scene.findEntityByGuid(selectedGuid);
        editorStatus = "Redid scene operation.";
    }
}

void Engine::saveCurrentScene() {
    if (playSession.active()) {
        editorStatus = "Stop Play Mode before saving. Runtime changes are temporary.";
        return;
    }
    std::filesystem::create_directories(config.assetRoot);
    const std::string scenePath = config.startupScene.empty()
        ? (std::filesystem::path(config.assetRoot) / "scene_saved.json").generic_string()
        : config.startupScene;

    std::string error;
    if (saveSceneToJson(scene, resources, scenePath, error)) {
        editorStatus = "Saved scene to " + scenePath;
    } else {
        editorStatus = error;
    }
}

bool Engine::loadSceneFromPath(const std::string& scenePath) {
    if (playSession.active()) stopPlayMode();
    sceneHistory.clear();
    std::string error;
    scriptSystem.destroyAll(scene, &prefabs, &inputMap);
    physicsSystem.clearContacts();
    if (!loadSceneFromJson(scene, resources, scenePath, error)) {
        editorStatus = error;
        return false;
    }

    syncGpuTextures(false);
    selectedEntity = scene.getPlayerEntity();
    notifyGameSceneLoaded();
    cameraSystem();
    editorStatus = "Loaded scene from " + scenePath;
    return true;
}

void Engine::loadSavedScene() {
    const std::string scenePath = config.startupScene.empty()
        ? (std::filesystem::path(config.assetRoot) / "scene_saved.json").generic_string()
        : config.startupScene;
    loadSceneFromPath(scenePath);
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

bool Engine::loadPrefabAssetFromPath(const std::string& path, std::string* loadedPrefabName) {
    std::string error;
    std::string prefabName;
    if (!loadPrefabFromJson(prefabs, resources, path, error, &prefabName)) {
        editorStatus = error;
        return false;
    }

    syncGpuTextures(false);
    if (loadedPrefabName) {
        *loadedPrefabName = prefabName;
    }
    editorStatus = "Loaded prefab asset: " + prefabName;
    return true;
}

void Engine::spawnPrefabAssetFromPath(const std::string& path) {
    std::string prefabName;
    if (!loadPrefabAssetFromPath(path, &prefabName)) {
        return;
    }

    try {
        selectedEntity = prefabs.instantiate(scene, prefabName, {0.0f, 0.0f});
        editorStatus = "Spawned prefab asset: " + prefabName;
    } catch (const std::exception& exception) {
        editorStatus = exception.what();
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
    if (playSession.active()) stopPlayMode();
    recordSceneEdit();
    scriptSystem.destroyAll(scene, &prefabs, &inputMap);
    physicsSystem.clearContacts();
    scene.clear();
    scene.setCamera({0.0f, 0.0f, 1.0f, 1.0f, InvalidEntity});
    selectedEntity = InvalidEntity;
    notifyGameSceneLoaded();
    editorStatus = "Created a new empty scene.";
}

Entity Engine::createEmptyEntity() {
    recordSceneEdit();
    const Entity entity = scene.createEntity();
    scene.setName(entity, {"Entity " + std::to_string(entity)});
    scene.setTransform(entity, {0.0f, 0.0f});
    selectedEntity = entity;
    editorStatus = "Created empty entity " + std::to_string(entity);
    return entity;
}

Entity Engine::duplicateEntity(Entity source) {
    if (scene.isValidEntity(source)) recordSceneEdit();
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
    if (scene.isValidEntity(entity)) recordSceneEdit();
    if (!scene.isValidEntity(entity)) {
        editorStatus = "Select a valid entity to delete.";
        return;
    }

    scriptSystem.destroyEntity(scene, entity, &prefabs, &inputMap);
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
            bool keyboardCaptured = false;
#if !defined(SIMPLEENGINE_PLAYER_ONLY)
            keyboardCaptured = imguiInitialized && editorWantsKeyboardCapture();
#endif
            if (event.key.key == GLFW_KEY_ESCAPE && event.key.pressed) {
                glfwSetWindowShouldClose(window, true);
            }

            if (!keyboardCaptured || !event.key.pressed) {
                inputMap.handleKeyChanged(event.key.key, event.key.pressed);
                dispatchInputActionEvents();
            }
        }

        if (isPlaying) {
            scriptSystem.handleEvent(scene, event, &prefabs, &inputMap);
        }
        dispatchGameEvent(event);
    }
}

void Engine::updateGamepadInput() {
    connectedGamepadCount = 0;
    activeGamepadName.clear();

    std::vector<bool> buttonStates(GLFW_GAMEPAD_BUTTON_LAST + 1, false);
    std::vector<float> axisValues(GLFW_GAMEPAD_AXIS_LAST + 1, 0.0f);

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

    inputMap.replaceGamepadState(buttonStates, axisValues);
    dispatchInputActionEvents();
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
        scriptSystem.destroyEntity(scene, entity, &prefabs, &inputMap);
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
    scriptSystem.destroyAll(scene, &prefabs, &inputMap);
    physicsSystem.clearContacts();
    PluginContext pluginContext = makePluginContext();
    pluginManager.unloadAll(pluginContext);
    jobSystem.stop();
    renderCommands.stop();
    renderCommands.clear();
    if (glContext.isAttached()) {
        glContext.makeCurrent();
    }
#if !defined(SIMPLEENGINE_PLAYER_ONLY)
    shutdownImGui();
#endif
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
