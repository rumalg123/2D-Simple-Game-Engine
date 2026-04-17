#pragma once

#include "Game.h"
#include "AudioSystem.h"

#include <memory>
#include <vector>

class DemoGame final : public IGame {
public:
    const char* name() const override;
    const char* controlsHelp() const override;
    void configureInput(GameContext& context) override;
    void registerScripts(GameContext& context) override;
    void loadInitialScene(GameContext& context) override;
    void registerPlugins(GameContext& context, PluginManager& plugins) override;
    void onSceneLoaded(GameContext& context) override;
    void onFixedUpdate(GameContext& context, float deltaTime) override;
    void onEvent(GameContext& context, const Event& event) override;
    std::vector<GameDebugStat> debugStats() const override;

private:
    int score = 0;
    float collisionSoundCooldown = 0.0f;
    AudioClipHandle scoreSound = InvalidAudioClip;
    AudioClipHandle collisionSound = InvalidAudioClip;
};

std::unique_ptr<IGame> createDemoGame();
