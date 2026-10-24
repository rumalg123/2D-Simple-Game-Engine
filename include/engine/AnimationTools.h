#pragma once
#include "ECS.h"

namespace AnimationTools {
struct FrameUv {
    float uMin = 0;
    float vMin = 0;
    float uMax = 1;
    float vMax = 1;
};
// Named clips share a sprite atlas and can be selected by editor or Lua.
const AnimationClip* findClip(const SpriteAnimationComponent& animation,
                              const std::string& name);
bool addClip(SpriteAnimationComponent& animation, AnimationClip clip);
bool updateClip(SpriteAnimationComponent& animation, AnimationClip clip);
bool removeClip(SpriteAnimationComponent& animation, const std::string& name);
bool selectClip(SpriteAnimationComponent& animation, const std::string& name,
                bool restart = true);
void normalize(SpriteAnimationComponent& animation);
void restart(SpriteAnimationComponent& animation);
void step(SpriteAnimationComponent& animation, int direction);
void advance(SpriteAnimationComponent& animation, float dt);
FrameUv uv(const SpriteAnimationComponent& animation);
}
