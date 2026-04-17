#pragma once

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "Event.h"
#include "Game.h"
#include "GameConfig.h"
#include "AudioSystem.h"
#include "InputMap.h"
#include "JobSystem.h"
#include "OpenGLContext.h"
#include "PhysicsSystem.h"
#include "PluginSystem.h"
#include "Prefab.h"
#include "RenderCommandQueue.h"
#include "RenderPacket.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "ScriptSystem.h"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class Engine {
public:
    explicit Engine(std::unique_ptr<IGame> game = nullptr, GameConfig config = {});
    bool init();
    void run();
    void cleanup();
    void onFramebufferResized(int width, int height);
    void onKeyChanged(int key, int action);

private:
    GLFWwindow* window = nullptr;
    OpenGLContext glContext;
    RenderCommandQueue renderCommands;
    Renderer renderer;
    ResourceManager resources;
    Scene scene;
    EventQueue eventQueue;
    PrefabRegistry prefabs;
    ScriptSystem scriptSystem;
    InputMap inputMap;
    AudioSystem audioSystem;
    PluginManager pluginManager;
    PhysicsSystem physicsSystem;
    JobSystem jobSystem;
    std::unique_ptr<IGame> game;
    GameConfig config;
    Entity selectedEntity = InvalidEntity;
    bool isPlaying = true;
    bool hotReloadEnabled = true;
    bool gizmoDragging = false;
    bool imguiInitialized = false;
    bool resetActionWasDown = false;
    int connectedGamepadCount = 0;
    std::string activeGamepadName;
    std::size_t nextRenderFrame = 1;
    std::vector<unsigned int> submittedTextureVersions;
    std::string editorStatus;

    GameContext makeGameContext();
    PluginContext makePluginContext();
    void notifyGameSceneLoaded();
    void initScene();
    void initInputMap();
    void initScripts();
    void initPlugins();
    bool initImGui();
    void shutdownImGui();
    void beginUiFrame();
    void renderUi();
    void endUiFrame();
    void presentRuntimeFrame();
    void hotReloadSystem();
    void handleGizmo();
    void renderPlayPausePanel();
    void renderSceneHierarchyPanel();
    void renderInspectorPanel();
    void renderAssetBrowserPanel();
    void saveCurrentScene();
    void loadSavedScene();
    void saveSelectedPrefab();
    void reloadPrefabAssets();
    void newEmptyScene();
    Entity createEmptyEntity();
    Entity duplicateEntity(Entity source);
    void deleteEntity(Entity entity);
    TextureHandle getEditorDefaultTexture();
    void eventSystem();
    void updateGamepadInput();
    void inputSystem();
    void spriteAnimationSystem(float deltaTime);
    void lifetimeSystem(float deltaTime);
    void cameraSystem();
    void renderSystem();
    RenderPacket buildRenderPacket();
    void clearGpuTextureCache();
    void syncGpuTextures(bool forceAll);
    void submitRenderCommand(std::string label, RenderCommandQueue::Command command);
};
