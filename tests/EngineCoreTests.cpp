#include "App.h"
#include "AssetManifest.h"
#include "GameConfig.h"
#include "Game.h"
#include "Grid.h"
#include "InputMap.h"
#include "PhysicsSystem.h"
#include "Prefab.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "SceneSerializer.h"
#include "ScriptSystem.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
class TempDirectory {
public:
    TempDirectory() {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() / ("SimpleEngineTests_" + std::to_string(suffix));
        std::filesystem::create_directories(path);
    }

    ~TempDirectory() {
        if (preserve) {
            return;
        }
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }

    const std::filesystem::path& get() const {
        return path;
    }

    void preserveOnFailure() {
        preserve = true;
    }

private:
    std::filesystem::path path;
    bool preserve = false;
};

void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void expectNear(float actual, float expected, const std::string& message) {
    if (std::fabs(actual - expected) > 0.0001f) {
        throw std::runtime_error(message + " expected " + std::to_string(expected) + " got " + std::to_string(actual));
    }
}

void testInputMapSupportsKeyboardAndGamepadBindings() {
    InputMap input;
    input.bindAction("MoveLeft", 65);
    input.bindKey("MoveLeft", 37);
    input.bindGamepadButton("MoveLeft", 14);
    input.bindGamepadAxis("MoveLeft", 0, GamepadAxisDirection::Negative, 0.35f);
    input.bindGamepadAxis("MoveRight", 0, GamepadAxisDirection::Positive, 0.35f);

    expect(input.isBoundToKey("MoveLeft", 65), "MoveLeft should keep its primary keyboard binding.");
    expect(input.isBoundToKey("MoveLeft", 37), "MoveLeft should allow multiple keyboard bindings.");
    expect(!input.isDown("MoveLeft"), "MoveLeft should start released.");

    input.handleKeyChanged(65, true);
    expect(input.isDown("MoveLeft"), "Keyboard press should activate an action.");
    expectNear(input.getAxis("MoveLeft", "MoveRight"), -1.0f, "Keyboard left should drive the horizontal axis.");

    input.handleKeyChanged(65, false);
    expect(!input.isDown("MoveLeft"), "Keyboard release should deactivate an action when no other binding is down.");

    input.setGamepadButtonState(14, true);
    expect(input.isDown("MoveLeft"), "Gamepad button should activate an action.");
    input.setGamepadButtonState(14, false);
    expect(!input.isDown("MoveLeft"), "Gamepad button release should deactivate an action.");

    input.setGamepadAxisValue(0, -0.50f);
    expect(input.isDown("MoveLeft"), "Negative gamepad axis should activate its bound action.");
    expectNear(input.getAxis("MoveLeft", "MoveRight"), -1.0f, "Negative gamepad axis should drive the action axis.");

    input.setGamepadAxisValue(0, 0.50f);
    expect(!input.isDown("MoveLeft"), "Positive axis movement should release the negative action.");
    expect(input.isDown("MoveRight"), "Positive gamepad axis should activate its bound action.");
    expectNear(input.getAxis("MoveLeft", "MoveRight"), 1.0f, "Positive gamepad axis should drive the action axis.");

    input.clearGamepadState();
    expect(!input.isDown("MoveRight"), "Clearing gamepad state should release gamepad-only actions.");

    const InputAction& moveLeft = input.getActions().at("MoveLeft");
    expect(moveLeft.keys.size() == 2, "MoveLeft should store both keyboard keys.");
    expect(moveLeft.gamepadButtons.size() == 1, "MoveLeft should store one gamepad button.");
    expect(moveLeft.gamepadAxes.size() == 1, "MoveLeft should store one gamepad axis binding.");
}

void testInputMapPublishesActionChanges() {
    InputMap input;
    input.bindAction("Jump", 32);
    input.bindGamepadButton("Dash", 0);
    input.bindGamepadButton("Dash", 1);

    input.handleKeyChanged(32, true);
    std::vector<InputActionChange> changes = input.consumeActionChanges();
    expect(changes.size() == 1, "Keyboard press should publish one action change.");
    expect(changes[0].actionName == "Jump" && changes[0].pressed, "Keyboard press should publish Jump pressed.");
    expect(input.consumeActionChanges().empty(), "Consuming action changes should clear them.");

    input.handleKeyChanged(32, true);
    expect(input.consumeActionChanges().empty(), "Repeated key state should not publish duplicate action changes.");

    input.handleKeyChanged(32, false);
    changes = input.consumeActionChanges();
    expect(changes.size() == 1, "Keyboard release should publish one action change.");
    expect(changes[0].actionName == "Jump" && !changes[0].pressed, "Keyboard release should publish Jump released.");

    input.replaceGamepadState({true, false}, {});
    changes = input.consumeActionChanges();
    expect(changes.size() == 1, "Gamepad button press should publish one action change.");
    expect(changes[0].actionName == "Dash" && changes[0].pressed, "Gamepad button press should publish Dash pressed.");

    input.replaceGamepadState({false, true}, {});
    expect(input.isDown("Dash"), "Dash should stay down when another bound button takes over.");
    expect(input.consumeActionChanges().empty(), "Replacing one held binding with another should not publish a false release.");

    input.replaceGamepadState({false, false}, {});
    changes = input.consumeActionChanges();
    expect(changes.size() == 1, "Gamepad release should publish one action change.");
    expect(changes[0].actionName == "Dash" && !changes[0].pressed, "Gamepad release should publish Dash released.");
}

void testResourceManagerReusesNamedTextures() {
    ResourceManager resources;
    const TextureHandle first = resources.createSolidColorTexture("white", 255, 255, 255, 255);
    const TextureHandle second = resources.createSolidColorTexture("white", 1, 2, 3, 4);

    expect(first == second, "ResourceManager should reuse existing texture handles by name.");
    expect(resources.textureCount() == 1, "Duplicate named textures should not create extra texture resources.");

    const TextureResource& texture = resources.getTexture(first);
    expect(texture.width == 1 && texture.height == 1, "Solid color texture should be one pixel.");
    expect(texture.pixels.size() == 4, "Solid color texture should contain RGBA bytes.");
    expect(texture.pixels[0] == 255 && texture.pixels[3] == 255, "Solid color texture should keep original pixel data.");
}

void testAssetManifestScansImportsAndPersists() {
    TempDirectory temp;
    const std::filesystem::path assetRoot = temp.get() / "assets";
    std::filesystem::create_directories(assetRoot / "sprites");
    std::filesystem::create_directories(assetRoot / "audio");
    std::filesystem::create_directories(assetRoot / "levels");

    {
        std::ofstream output(assetRoot / "sprites" / "player.ppm");
        output << "P3\n1 1\n255\n255 0 0\n";
    }
    {
        std::ofstream output(assetRoot / "sprites" / "hero_atlas.png");
        output << "not a real png";
    }
    {
        std::ofstream output(assetRoot / "audio" / "jump.wav");
        output << "not a real wav";
    }
    {
        std::ofstream output(assetRoot / "levels" / "intro.scene.json");
        output << "{}";
    }

    AssetManifest manifest;
    const std::size_t added = manifest.scanDirectory(assetRoot);
    expect(added == 4, "Asset manifest scan should import supported asset files.");
    expect(manifest.scanDirectory(assetRoot) == 0, "Repeated asset manifest scan should not duplicate entries.");

    const AssetManifestEntry* player = manifest.findByPath("sprites/player.ppm");
    expect(player != nullptr, "Scanned texture should be findable by manifest path.");
    expect(player->id == "texture:sprites/player", "Texture asset id should be stable and path-derived.");
    expect(player->type == AssetType::Texture, "PPM files should import as texture assets.");

    AssetManifestEntry& atlas =
        manifest.importAsset(assetRoot, assetRoot / "sprites" / "hero_atlas.png", AssetType::SpriteSheet);
    atlas.spriteSheet.columns = 4;
    atlas.spriteSheet.rows = 2;
    expect(atlas.id == "texture:sprites/hero_atlas", "Explicit type updates should preserve the stable asset id.");
    expect(atlas.type == AssetType::SpriteSheet, "Explicit import should allow image assets to become sprite sheets.");

    const AssetManifestEntry* audio = manifest.findByPath("audio/jump.wav");
    expect(audio && audio->type == AssetType::Audio, "WAV files should import as audio assets.");

    const std::filesystem::path manifestPath = AssetManifest::defaultManifestPath(assetRoot);
    std::string error;
    expect(manifest.saveToFile(manifestPath.string(), error), "Saving asset manifest failed: " + error);

    AssetManifest loaded;
    const bool loadedManifest = loaded.loadFromFile(manifestPath.string(), error);
    if (!loadedManifest) {
        temp.preserveOnFailure();
    }
    expect(loadedManifest, "Loading asset manifest failed: " + error);
    expect(loaded.entryCount() == manifest.entryCount(), "Loaded manifest should preserve asset count.");

    const AssetManifestEntry* loadedAtlas = loaded.findById(atlas.id);
    expect(loadedAtlas != nullptr, "Loaded manifest should preserve sprite sheet asset id.");
    expect(loadedAtlas->type == AssetType::SpriteSheet, "Loaded manifest should preserve sprite sheet type.");
    expect(
        loadedAtlas->spriteSheet.columns == 4 && loadedAtlas->spriteSheet.rows == 2,
        "Loaded manifest should preserve sprite sheet metadata.");
}

void testGameConfigLoadsFlatJsonAndKeepsDefaults() {
    TempDirectory temp;
    const std::filesystem::path configPath = temp.get() / "game.config.json";

    {
        std::ofstream output(configPath.string());
        output << "{\n";
        output << "  \"windowTitle\": \"Snake\",\n";
        output << "  \"windowWidth\": 960,\n";
        output << "  \"windowHeight\": 540,\n";
        output << "  \"editorEnabled\": false,\n";
        output << "  \"hotReload\": false,\n";
        output << "  \"assetRoot\": \"samples/snake/assets\"\n";
        output << "}\n";
    }

    GameConfig config;
    config.prefabDirectory = "custom_prefabs";
    std::string error;
    const bool loaded = loadGameConfigFromFile(configPath.string(), config, error);
    if (!loaded) {
        temp.preserveOnFailure();
        throw std::runtime_error("Loading game config failed: " + error);
    }

    expect(config.windowTitle == "Snake", "Config should load the window title.");
    expect(config.windowWidth == 960, "Config should load the window width.");
    expect(config.windowHeight == 540, "Config should load the window height.");
    expect(!config.editorEnabled, "Config should load editor mode.");
    expect(!config.hotReload, "Config should load hot reload mode.");
    expect(config.startPlaying, "Missing config keys should keep default bool values.");
    expect(config.vsync, "Missing config keys should keep default vsync value.");
    expect(config.assetRoot == "samples/snake/assets", "Config should load asset root.");
    expect(config.prefabDirectory == "custom_prefabs", "Missing string keys should keep existing values.");
}

void testGameConfigRejectsInvalidWindowSize() {
    TempDirectory temp;
    const std::filesystem::path configPath = temp.get() / "bad.config.json";

    {
        std::ofstream output(configPath.string());
        output << "{ \"windowWidth\": 0, \"windowHeight\": 600 }\n";
    }

    GameConfig config;
    std::string error;
    expect(!loadGameConfigFromFile(configPath.string(), config, error), "Invalid window size should fail.");
    expect(error.find("windowWidth") != std::string::npos, "Invalid window size should report the window keys.");
}

void testAppModesNormalizeConfig() {
    GameConfig runtimeInput;
    runtimeInput.editorEnabled = true;
    runtimeInput.startPlaying = false;
    GameConfig runtimeConfig = RuntimeApp::makeRuntimeConfig(runtimeInput);
    expect(!runtimeConfig.editorEnabled, "RuntimeApp config should disable editor UI.");
    expect(runtimeConfig.startPlaying, "RuntimeApp config should start simulation playback.");

    GameConfig editorInput;
    editorInput.editorEnabled = false;
    editorInput.startPlaying = false;
    GameConfig editorConfig = EditorApp::makeEditorConfig(editorInput);
    expect(editorConfig.editorEnabled, "EditorApp config should enable editor UI.");
    expect(!editorConfig.startPlaying, "EditorApp config should preserve the requested play state.");
}

void testSceneGameplayHelpersFindEntities() {
    Scene scene;

    const Entity player = scene.createSprite(
        "Player",
        {1.0f, 2.0f},
        {0.25f, 0.5f, 1.0f, 0.8f, 0.6f, 1.0f, 10, InvalidTexture},
        "Actor");
    const Entity hud = scene.createText(
        "Score",
        {-0.9f, 0.8f},
        {"SCORE 0"},
        "UI");
    const Entity wall = scene.createCollider(
        "Wall",
        {0.0f, 0.0f},
        {0.5f, 0.5f, true},
        "Solid");

    expect(scene.findEntityByName("Player") == player, "Scene should find entities by name.");
    expect(scene.findEntityByTag("UI") == hud, "Scene should find the first entity with a tag.");
    expect(scene.findEntityByName("Wall") == wall, "Collider helper should name the entity.");
    expect(scene.getSprite(player) != nullptr, "Sprite helper should add a sprite component.");
    expect(scene.getText(hud) != nullptr, "Text helper should add a text component.");
    expect(scene.getCollider(wall) != nullptr, "Collider helper should add a collider component.");

    std::vector<Entity> actors = scene.findEntitiesByTag("Actor");
    expect(actors.size() == 1 && actors[0] == player, "Scene should collect all entities with a tag.");

    scene.destroyEntity(player);
    expect(scene.findEntityByName("Player") == InvalidEntity, "Destroyed entities should not be returned by name lookup.");
    expect(scene.findEntityByTag("Actor") == InvalidEntity, "Destroyed entities should not be returned by tag lookup.");
}

void testGameContextInstantiatesPrefabs() {
    Scene scene;
    PrefabRegistry prefabs;
    Prefab prefab;
    prefab.name = "Crate";
    prefab.transform = TransformComponent{0.25f, -0.25f};
    prefab.sprite = SpriteComponent{0.1f, 0.1f, 0.7f, 0.5f, 0.3f, 1.0f, 4, InvalidTexture};
    prefabs.registerPrefab(prefab);

    GameContext context;
    context.scene = &scene;
    context.prefabs = &prefabs;

    const Entity entity = context.instantiatePrefab("Crate");
    expect(entity != InvalidEntity, "GameContext should instantiate registered prefabs.");
    expect(scene.getName(entity) && scene.getName(entity)->name == "Crate", "Instantiated prefab should keep its name.");
    expect(scene.getSprite(entity) != nullptr, "Instantiated prefab should copy sprite components.");

    const Entity moved = context.instantiatePrefab("Crate", {1.0f, 2.0f});
    const TransformComponent* transform = scene.getTransform(moved);
    expect(transform != nullptr, "Prefab transform override should create a transform.");
    expectNear(transform->x, 1.0f, "Prefab transform override should set x.");
    expectNear(transform->y, 2.0f, "Prefab transform override should set y.");
    expect(context.instantiatePrefab("Missing") == InvalidEntity, "Missing prefabs should return InvalidEntity.");
}

void testGameContextSceneRequests() {
    GameContext context;
    bool reloaded = false;
    bool cleared = false;
    std::string loadedPath;

    context.reloadSceneRequest = [&reloaded]() {
        reloaded = true;
    };
    context.clearSceneRequest = [&cleared]() {
        cleared = true;
    };
    context.loadSceneRequest = [&loadedPath](const std::string& path) {
        loadedPath = path;
        return path == "levels/one.scene.json";
    };

    context.reloadScene();
    context.clearScene();
    const bool loaded = context.loadScene("levels/one.scene.json");

    expect(reloaded, "GameContext should forward scene reload requests.");
    expect(cleared, "GameContext should forward clear scene requests.");
    expect(loaded, "GameContext should forward load scene requests.");
    expect(loadedPath == "levels/one.scene.json", "GameContext should pass the requested scene path.");
}

void testScriptSystemLifecycleCallbacks() {
    Scene scene;
    ScriptSystem scripts;
    std::vector<std::string> calls;

    ScriptSystem::ScriptLifecycle lifecycle;
    lifecycle.onCreate = [&calls](ScriptContext&, Entity, ScriptComponent& script) {
        calls.push_back("create");
        script.parameters["created"] = 1.0f;
    };
    lifecycle.onStart = [&calls](ScriptContext&, Entity, ScriptComponent&) {
        calls.push_back("start");
    };
    lifecycle.onUpdate = [&calls](ScriptContext&, Entity, ScriptComponent&, float deltaTime) {
        calls.push_back(deltaTime > 0.0f ? "update" : "bad_update");
    };
    lifecycle.onFixedUpdate = [&calls](ScriptContext&, Entity, ScriptComponent& script, float deltaTime) {
        calls.push_back(deltaTime > 0.0f ? "fixed" : "bad_fixed");
        script.parameters["fixed"] = 1.0f;
    };
    lifecycle.onEvent = [&calls](ScriptContext&, Entity, ScriptComponent&, const Event& event) {
        if (event.type == EventType::ActionChanged && event.action.pressed) {
            calls.push_back("event:" + event.action.actionName);
        }
    };
    lifecycle.onDestroy = [&calls](ScriptContext&, Entity, ScriptComponent& script) {
        calls.push_back(script.parameters.count("fixed") ? "destroy" : "bad_destroy");
    };

    scripts.registerScript("probe", lifecycle, {{"speed", 2.0f, 0.0f, 10.0f}});

    const Entity entity = scene.createEntity();
    scene.setScript(entity, {"probe", true, 0.0f});

    scripts.update(scene, 0.25f);
    ScriptComponent* script = scene.getScript(entity);
    expect(script != nullptr, "Lifecycle script should still exist after update.");
    expectNear(script->parameters["created"], 1.0f, "onCreate should be able to mutate script parameters.");
    expectNear(script->parameters["speed"], 2.0f, "Script default parameters should be applied.");

    scripts.fixedUpdate(scene, 0.5f);

    Event event;
    event.type = EventType::ActionChanged;
    event.action = {"Confirm", true};
    scripts.handleEvent(scene, event);

    scripts.destroyEntity(scene, entity);
    scene.destroyEntity(entity);

    const std::vector<std::string> expected{
        "create",
        "start",
        "update",
        "fixed",
        "event:Confirm",
        "destroy"
    };
    expect(calls == expected, "Script lifecycle callbacks should run in the expected order.");
}

void testLegacyScriptRegistrationRunsDuringFixedUpdate() {
    Scene scene;
    ScriptSystem scripts;
    int updateCount = 0;

    scripts.registerScript("legacy", [&updateCount](ScriptContext&, Entity, ScriptComponent&, float) {
        ++updateCount;
    });

    const Entity entity = scene.createEntity();
    scene.setScript(entity, {"legacy", true, 0.0f});

    scripts.update(scene, 0.25f);
    expect(updateCount == 0, "Legacy script update should not run during variable update.");

    scripts.fixedUpdate(scene, 0.25f);
    expect(updateCount == 1, "Legacy script update should run during fixed update.");
}

void testScriptCallbacksCanDestroyEntities() {
    Scene scene;
    ScriptSystem scripts;
    std::vector<std::string> calls;

    ScriptSystem::ScriptLifecycle lifecycle;
    lifecycle.onUpdate = [&calls](ScriptContext& context, Entity entity, ScriptComponent& script, float) {
        calls.push_back("update");
        script.parameters["touched"] = 1.0f;
        context.scene->destroyEntity(entity);
    };
    lifecycle.onDestroy = [&calls](ScriptContext&, Entity, ScriptComponent&) {
        calls.push_back("destroy");
    };

    scripts.registerScript("self_destruct", lifecycle);

    const Entity entity = scene.createEntity();
    scene.setScript(entity, {"self_destruct", true, 0.0f});

    scripts.update(scene, 0.25f);
    expect(!scene.isValidEntity(entity), "Script callback should be able to destroy its entity.");

    scripts.update(scene, 0.25f);

    const std::vector<std::string> expected{"update", "destroy"};
    expect(calls == expected, "ScriptSystem should clean up instances destroyed by callbacks.");
}

void testPhysicsCollisionLayersAndContactPhases() {
    Scene scene;
    PhysicsSystem physics;
    EventQueue events;

    const Entity sensor = scene.createEntity();
    scene.setTransform(sensor, {0.0f, 0.0f});
    scene.setCollider(sensor, {0.5f, 0.5f, false, true, 1u, 2u});

    const Entity target = scene.createEntity();
    scene.setTransform(target, {0.25f, 0.0f});
    scene.setCollider(target, {0.5f, 0.5f, false, true, 2u, 1u});

    physics.update(scene, events, 0.0f);
    std::vector<Event> drained = events.drain();
    expect(drained.size() == 1, "First filtered trigger overlap should publish one event.");
    expect(drained[0].type == EventType::Trigger, "Trigger overlap should publish a trigger event.");
    expect(drained[0].collision.phase == CollisionPhase::Enter, "First trigger overlap should be Enter.");

    physics.update(scene, events, 0.0f);
    drained = events.drain();
    expect(drained.size() == 1, "Persistent trigger overlap should publish one stay event.");
    expect(drained[0].collision.phase == CollisionPhase::Stay, "Persistent trigger overlap should be Stay.");

    scene.getTransform(target)->x = 5.0f;
    physics.update(scene, events, 0.0f);
    drained = events.drain();
    expect(drained.size() == 1, "Separated trigger overlap should publish one exit event.");
    expect(drained[0].collision.phase == CollisionPhase::Exit, "Separated trigger overlap should be Exit.");

    scene.getTransform(target)->x = 0.25f;
    scene.getCollider(target)->mask = 4u;
    physics.update(scene, events, 0.0f);
    expect(events.drain().empty(), "Collision masks should suppress non-matching trigger pairs.");
}

void testPhysicsAabbQueryUsesLayersAndMasks() {
    Scene scene;
    PhysicsSystem physics;

    const Entity pickup = scene.createEntity();
    scene.setTransform(pickup, {0.25f, 0.0f});
    scene.setCollider(pickup, {0.25f, 0.25f, false, true, 2u, ColliderMaskAll});

    const Entity wall = scene.createEntity();
    scene.setTransform(wall, {0.0f, 0.25f});
    scene.setCollider(wall, {0.25f, 0.25f, true, false, 4u, ColliderMaskAll});

    const ColliderComponent pickupOnlyQuery{0.5f, 0.5f, false, true, 1u, 2u};
    std::vector<Entity> hits = physics.queryAabb(scene, {0.0f, 0.0f}, pickupOnlyQuery);
    expect(hits.size() == 1 && hits[0] == pickup, "AABB query should honor the query collision mask.");

    const ColliderComponent allQuery{0.5f, 0.5f, false, true, 1u, 2u | 4u};
    hits = physics.queryAabb(scene, {0.0f, 0.0f}, allQuery);
    expect(hits.size() == 2, "AABB query should return every overlapping collider allowed by masks.");
}

void testGridHelpersConvertCells() {
    const GridLayout layout{10, 5, 2.0f, 4.0f, {0.0f, 0.0f}, true};

    expect(gridCellCount(layout) == 50, "Grid cell count should use columns times rows.");
    expect(gridContains(layout, {0, 0}), "Grid should contain its top-left cell.");
    expect(!gridContains(layout, {-1, 0}), "Grid should reject negative columns.");
    expect(!gridContains(layout, {10, 0}), "Grid should reject columns past the width.");
    expect(gridToIndex({3, 2}, layout.columns) == 23, "Grid index should be row-major.");
    expect(gridFromIndex(23, layout.columns) == GridCell{3, 2}, "Grid index conversion should round-trip.");

    const TransformComponent world = gridToWorld(layout, {0, 0});
    expectNear(world.x, -9.0f, "Grid world x should center cells around the layout center.");
    expectNear(world.y, 8.0f, "Y-down grid world y should put row zero at the top.");
}

void testSceneJsonRoundTripsTextAndTilemaps() {
    TempDirectory temp;
    const std::filesystem::path scenePath = temp.get() / "scene.json";

    Scene scene;
    ResourceManager resources;
    const TextureHandle tileTexture = resources.createSolidColorTexture("test_tiles", 56, 152, 75, 255);

    const Entity discarded = scene.createEntity();
    scene.setName(discarded, {"Discarded"});

    const Entity entity = scene.createEntity();
    scene.destroyEntity(discarded);

    scene.setName(entity, {"LevelRoot"});
    scene.setTag(entity, {"Tilemap"});
    scene.setTransform(entity, {-1.25f, 1.50f});
    scene.setText(entity, {"HELLO\n2D", 0.04f, 0.07f, 0.01f, 0.02f, 0.9f, 0.8f, 0.7f, 1.0f, 80, false});
    scene.setCollider(entity, {0.30f, 0.40f, true, false, 4u, 8u});
    scene.setTilemap(entity, {
        3,
        2,
        0.25f,
        0.50f,
        4,
        1,
        -30,
        tileTexture,
        0.8f,
        0.9f,
        1.0f,
        0.75f,
        {0, 1, -1, 2, 3, 0}
    });
    scene.setPlayerEntity(entity);
    scene.setCamera({5.0f, 6.0f, 1.5f, 2.0f, entity});

    std::string error;
    expect(saveSceneToJson(scene, resources, scenePath.string(), error), "Saving scene JSON failed: " + error);

    Scene loadedScene;
    ResourceManager loadedResources;
    if (!loadSceneFromJson(loadedScene, loadedResources, scenePath.string(), error)) {
        temp.preserveOnFailure();
        throw std::runtime_error("Loading scene JSON failed from " + scenePath.string() + ": " + error);
    }

    expect(loadedScene.livingEntityCount() == 1, "Loaded scene should contain one living entity.");
    const Entity loadedEntity = loadedScene.getPlayerEntity();
    expect(loadedEntity != InvalidEntity, "Loaded scene should remap player entity.");
    expect(loadedScene.getCamera().target == loadedEntity, "Loaded scene should remap camera target.");
    expectNear(loadedScene.getCamera().x, 5.0f, "Camera x should round-trip.");
    expectNear(loadedScene.getCamera().halfHeight, 2.0f, "Camera half-height should round-trip.");

    const NameComponent* name = loadedScene.getName(loadedEntity);
    expect(name && name->name == "LevelRoot", "Name component should round-trip.");

    const TransformComponent* transform = loadedScene.getTransform(loadedEntity);
    expect(transform != nullptr, "Transform component should round-trip.");
    expectNear(transform->x, -1.25f, "Transform x should round-trip.");
    expectNear(transform->y, 1.50f, "Transform y should round-trip.");

    const TextComponent* text = loadedScene.getText(loadedEntity);
    expect(text != nullptr, "Text component should round-trip.");
    expect(text->text == "HELLO\n2D", "Text content should round-trip escaped newlines.");
    expectNear(text->characterWidth, 0.04f, "Text character width should round-trip.");
    expect(text->screenSpace == false, "Text screen-space flag should round-trip.");

    const ColliderComponent* collider = loadedScene.getCollider(loadedEntity);
    expect(collider != nullptr, "Collider component should round-trip.");
    expectNear(collider->halfWidth, 0.30f, "Collider half width should round-trip.");
    expect(collider->solid && !collider->trigger, "Collider solid/trigger flags should round-trip.");
    expect(collider->layer == 4u && collider->mask == 8u, "Collider layer and mask should round-trip.");

    const TilemapComponent* tilemap = loadedScene.getTilemap(loadedEntity);
    expect(tilemap != nullptr, "Tilemap component should round-trip.");
    expect(tilemap->columns == 3 && tilemap->rows == 2, "Tilemap dimensions should round-trip.");
    expectNear(tilemap->tileWidth, 0.25f, "Tilemap tile width should round-trip.");
    expectNear(tilemap->tileHeight, 0.50f, "Tilemap tile height should round-trip.");
    expect(tilemap->atlasColumns == 4 && tilemap->atlasRows == 1, "Tilemap atlas layout should round-trip.");
    expect(tilemap->layer == -30, "Tilemap layer should round-trip.");
    expectNear(tilemap->alpha, 0.75f, "Tilemap alpha should round-trip.");
    expect(tilemap->tiles == std::vector<int>({0, 1, -1, 2, 3, 0}), "Tilemap tile data should round-trip.");
    expect(tilemap->texture != InvalidTexture, "Loaded tilemap should resolve a usable texture.");
}

void runTest(const char* name, void (*test)()) {
    test();
    std::cout << "[PASS] " << name << '\n';
}
}

int main() {
    try {
        runTest("InputMap keyboard and gamepad bindings", testInputMapSupportsKeyboardAndGamepadBindings);
        runTest("InputMap action changes", testInputMapPublishesActionChanges);
        runTest("ResourceManager named texture reuse", testResourceManagerReusesNamedTextures);
        runTest("AssetManifest scan and persistence", testAssetManifestScansImportsAndPersists);
        runTest("GameConfig flat JSON loading", testGameConfigLoadsFlatJsonAndKeepsDefaults);
        runTest("GameConfig invalid window size rejection", testGameConfigRejectsInvalidWindowSize);
        runTest("App mode config normalization", testAppModesNormalizeConfig);
        runTest("Scene gameplay helpers", testSceneGameplayHelpersFindEntities);
        runTest("GameContext prefab helpers", testGameContextInstantiatesPrefabs);
        runTest("GameContext scene requests", testGameContextSceneRequests);
        runTest("ScriptSystem lifecycle callbacks", testScriptSystemLifecycleCallbacks);
        runTest("ScriptSystem legacy fixed update", testLegacyScriptRegistrationRunsDuringFixedUpdate);
        runTest("ScriptSystem callback entity destruction", testScriptCallbacksCanDestroyEntities);
        runTest("Physics collision layers and phases", testPhysicsCollisionLayersAndContactPhases);
        runTest("Physics AABB query filters", testPhysicsAabbQueryUsesLayersAndMasks);
        runTest("Grid helpers", testGridHelpersConvertCells);
        runTest("Scene JSON text and tilemap round-trip", testSceneJsonRoundTripsTextAndTilemaps);
    } catch (const std::exception& exception) {
        std::cerr << "[FAIL] " << exception.what() << '\n';
        return 1;
    }

    return 0;
}
