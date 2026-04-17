#include "DemoGame.h"

#include "DemoScene.h"
#include "AudioSystem.h"
#include "Event.h"
#include "InputMap.h"
#include "PluginSystem.h"
#include "Prefab.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "ScriptSystem.h"

#include "imgui.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <memory>
#include <string>

namespace {
bool hasTag(const Scene& scene, Entity entity, const std::string& expectedTag) {
    const TagComponent* tag = scene.getTag(entity);
    return tag && tag->tag == expectedTag;
}

void updateScoreText(Scene& scene, int score) {
    ComponentPool<TextComponent>& texts = scene.getTextPool();
    for (std::size_t index = 0; index < texts.size(); ++index) {
        const Entity entity = texts.entityAt(index);
        if (hasTag(scene, entity, "ScoreHud")) {
            texts.componentAt(index).text = "SCORE " + std::to_string(score);
        }
    }
}

void registerDemoScripts(ScriptSystem& scripts) {
    scripts.registerScript("pulse_alpha", [](ScriptContext& context, Entity entity, ScriptComponent& script, float) {
        Scene& scene = *context.scene;
        SpriteComponent* sprite = scene.getSprite(entity);
        if (!sprite) {
            return;
        }

        const float speed = ScriptSystem::getFloatParameter(script, "speed", 5.0f);
        const float minAlpha = ScriptSystem::getFloatParameter(script, "minAlpha", 0.65f);
        const float maxAlpha = ScriptSystem::getFloatParameter(script, "maxAlpha", 1.0f);
        sprite->alpha = minAlpha + (maxAlpha - minAlpha) * (std::sin(script.elapsedTime * speed) * 0.5f + 0.5f);
    }, {
        {"speed", 5.0f, 0.1f, 20.0f},
        {"minAlpha", 0.65f, 0.0f, 1.0f},
        {"maxAlpha", 1.0f, 0.0f, 1.0f}
    });

    scripts.registerScript("wander_force", [](ScriptContext& context, Entity entity, ScriptComponent& script, float) {
        Scene& scene = *context.scene;
        PhysicsComponent* physics = scene.getPhysics(entity);
        if (!physics) {
            return;
        }

        const float speedX = ScriptSystem::getFloatParameter(script, "speedX", 2.0f);
        const float speedY = ScriptSystem::getFloatParameter(script, "speedY", 2.5f);
        const float forceX = ScriptSystem::getFloatParameter(script, "forceX", 0.45f);
        const float forceY = ScriptSystem::getFloatParameter(script, "forceY", 0.35f);
        physics->forceX += std::sin(script.elapsedTime * speedX) * forceX;
        physics->forceY += std::cos(script.elapsedTime * speedY) * forceY;
    }, {
        {"speedX", 2.0f, 0.1f, 20.0f},
        {"speedY", 2.5f, 0.1f, 20.0f},
        {"forceX", 0.45f, -20.0f, 20.0f},
        {"forceY", 0.35f, -20.0f, 20.0f}
    });

    scripts.registerScript("flappy_bird", [](ScriptContext& context, Entity entity, ScriptComponent& script, float) {
        if (!context.scene || !context.inputMap) {
            return;
        }

        PhysicsComponent* physics = context.scene->getPhysics(entity);
        if (!physics) {
            return;
        }

        physics->forceY += ScriptSystem::getFloatParameter(script, "gravity", -0.9f);
        if (context.inputMap->isDown("Flap")) {
            physics->velocityY = ScriptSystem::getFloatParameter(script, "flapVelocity", 1.25f);
        }
    }, {
        {"gravity", -0.9f, -20.0f, 20.0f},
        {"flapVelocity", 1.25f, -10.0f, 10.0f}
    });

    scripts.registerScript("flappy_pipe_mover", [](ScriptContext& context, Entity entity, ScriptComponent& script, float) {
        if (!context.scene) {
            return;
        }

        PhysicsComponent* physics = context.scene->getPhysics(entity);
        if (physics) {
            physics->velocityX = ScriptSystem::getFloatParameter(script, "speedX", -0.82f);
        }
    }, {
        {"speedX", -0.82f, -10.0f, 10.0f}
    });

    scripts.registerScript("flappy_pipe_spawner", [](ScriptContext& context, Entity, ScriptComponent& script, float) {
        const float spawnInterval = ScriptSystem::getFloatParameter(script, "spawnInterval", 1.35f);
        if (!context.scene || !context.prefabs || script.elapsedTime < spawnInterval) {
            return;
        }

        script.elapsedTime = 0.0f;
        const float gapRange = ScriptSystem::getFloatParameter(script, "gapYRange", 0.42f);
        const float gapOffset = ScriptSystem::getFloatParameter(script, "pipeGapOffset", 0.82f);
        const float spawnX = ScriptSystem::getFloatParameter(script, "spawnX", 1.55f);
        const float gapY = std::sin(static_cast<float>(context.scene->entityCount()) * 1.37f) * gapRange;
        context.prefabs->instantiate(*context.scene, "FlappyPipeTop", {spawnX, gapY + gapOffset});
        context.prefabs->instantiate(*context.scene, "FlappyPipeBottom", {spawnX, gapY - gapOffset});
        context.prefabs->instantiate(*context.scene, "FlappyScoreZone", {spawnX, gapY});
    }, {
        {"spawnInterval", 1.35f, 0.1f, 10.0f},
        {"spawnX", 1.55f, -10.0f, 10.0f},
        {"gapYRange", 0.42f, 0.0f, 2.0f},
        {"pipeGapOffset", 0.82f, 0.1f, 3.0f}
    });
}

class DemoGameplayPlugin final : public IPlugin {
public:
    const char* name() const override {
        return "Demo Gameplay Tools";
    }

    void onRenderUi(PluginContext& context) override {
        ImGui::Begin("Demo Game");
        ImGui::Text("Loaded plugin: %s", name());

        if (ImGui::Button("Spawn Flappy Starter")) {
            try {
                const Entity bird = context.prefabs->instantiate(*context.scene, "FlappyBird", {-0.45f, 0.0f});
                context.prefabs->instantiate(*context.scene, "FlappyPipeSpawner", {0.0f, 0.0f});

                Camera camera = context.scene->getCamera();
                camera.x = 0.0f;
                camera.y = 0.0f;
                camera.halfWidth = 1.0f;
                camera.halfHeight = 1.0f;
                camera.target = bird;
                context.scene->setPlayerEntity(bird);
                context.scene->setCamera(camera);

                if (context.editorStatus) {
                    *context.editorStatus = "Spawned Flappy starter. Press Space to flap.";
                }
            } catch (const std::exception& exception) {
                if (context.editorStatus) {
                    *context.editorStatus = exception.what();
                }
            }
        }

        ImGui::End();
    }
};
}

const char* DemoGame::name() const {
    return "Engine Demo";
}

const char* DemoGame::controlsHelp() const {
    return "WASD/left stick/D-pad moves, Space/A flaps, R/Start resets, Esc exits.";
}

void DemoGame::configureInput(GameContext& context) {
    if (!context.inputMap) {
        return;
    }

    context.inputMap->bindAction("MoveLeft", GLFW_KEY_A);
    context.inputMap->bindAction("MoveRight", GLFW_KEY_D);
    context.inputMap->bindAction("MoveUp", GLFW_KEY_W);
    context.inputMap->bindAction("MoveDown", GLFW_KEY_S);
    context.inputMap->bindAction("Flap", GLFW_KEY_SPACE);
    context.inputMap->bindAction("ResetScene", GLFW_KEY_R);

    context.inputMap->bindKey("MoveLeft", GLFW_KEY_LEFT);
    context.inputMap->bindKey("MoveRight", GLFW_KEY_RIGHT);
    context.inputMap->bindKey("MoveUp", GLFW_KEY_UP);
    context.inputMap->bindKey("MoveDown", GLFW_KEY_DOWN);

    context.inputMap->bindGamepadButton("MoveLeft", GLFW_GAMEPAD_BUTTON_DPAD_LEFT);
    context.inputMap->bindGamepadButton("MoveRight", GLFW_GAMEPAD_BUTTON_DPAD_RIGHT);
    context.inputMap->bindGamepadButton("MoveUp", GLFW_GAMEPAD_BUTTON_DPAD_UP);
    context.inputMap->bindGamepadButton("MoveDown", GLFW_GAMEPAD_BUTTON_DPAD_DOWN);
    context.inputMap->bindGamepadButton("Flap", GLFW_GAMEPAD_BUTTON_A);
    context.inputMap->bindGamepadButton("ResetScene", GLFW_GAMEPAD_BUTTON_START);

    context.inputMap->bindGamepadAxis("MoveLeft", GLFW_GAMEPAD_AXIS_LEFT_X, GamepadAxisDirection::Negative, 0.35f);
    context.inputMap->bindGamepadAxis("MoveRight", GLFW_GAMEPAD_AXIS_LEFT_X, GamepadAxisDirection::Positive, 0.35f);
    context.inputMap->bindGamepadAxis("MoveUp", GLFW_GAMEPAD_AXIS_LEFT_Y, GamepadAxisDirection::Negative, 0.35f);
    context.inputMap->bindGamepadAxis("MoveDown", GLFW_GAMEPAD_AXIS_LEFT_Y, GamepadAxisDirection::Positive, 0.35f);
}

void DemoGame::registerScripts(GameContext& context) {
    if (context.scripts) {
        registerDemoScripts(*context.scripts);
    }
}

void DemoGame::loadInitialScene(GameContext& context) {
    if (!context.scene || !context.resources || !context.prefabs) {
        return;
    }

    loadDemoScene(*context.scene, *context.resources, *context.prefabs);
    if (context.audio) {
        scoreSound = context.audio->createTone("demo_score", 880.0f, 0.10f, 0.28f);
        collisionSound = context.audio->createTone("demo_collision", 180.0f, 0.08f, 0.22f);
    }
    score = 0;
    collisionSoundCooldown = 0.0f;
    updateScoreText(*context.scene, score);
}

void DemoGame::registerPlugins(GameContext& context, PluginManager& plugins) {
    PluginContext pluginContext{
        context.scene,
        context.resources,
        context.prefabs,
        context.scripts,
        context.inputMap,
        context.editorStatus
    };
    plugins.loadDefaultPlugins(pluginContext);
    plugins.loadPlugin(std::make_unique<DemoGameplayPlugin>(), pluginContext);
}

void DemoGame::onSceneLoaded(GameContext& context) {
    score = 0;
    collisionSoundCooldown = 0.0f;
    if (context.scene) {
        updateScoreText(*context.scene, score);
    }
}

void DemoGame::onFixedUpdate(GameContext&, float deltaTime) {
    collisionSoundCooldown = std::max(0.0f, collisionSoundCooldown - deltaTime);
}

void DemoGame::onEvent(GameContext& context, const Event& event) {
    if (!context.scene) {
        return;
    }

    Scene& scene = *context.scene;
    const Entity player = scene.getPlayerEntity();
    if (player == InvalidEntity) {
        return;
    }

    const bool playerInvolved =
        event.collision.first == player || event.collision.second == player;
    if (!playerInvolved) {
        return;
    }

    if (event.type == EventType::Trigger) {
        const Entity other = event.collision.first == player ? event.collision.second : event.collision.first;
        if (hasTag(scene, other, "ScoreZone")) {
            ++score;
            updateScoreText(scene, score);
            if (context.audio && scoreSound != InvalidAudioClip) {
                context.audio->play(scoreSound, 1.0f);
            }
            scene.destroyEntity(other);
        }
        return;
    }

    if (event.type == EventType::Collision) {
        if (context.audio && collisionSound != InvalidAudioClip && collisionSoundCooldown <= 0.0f) {
            context.audio->play(collisionSound, 1.0f);
            collisionSoundCooldown = 0.14f;
        }

        SpriteComponent* sprite = scene.getSprite(player);
        if (sprite) {
            sprite->red = 1.0f;
            sprite->green = 0.82f;
            sprite->blue = 0.45f;
        }
    }
}

std::vector<GameDebugStat> DemoGame::debugStats() const {
    return {{"Score", score}};
}

std::unique_ptr<IGame> createDemoGame() {
    return std::make_unique<DemoGame>();
}
