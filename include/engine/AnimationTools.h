#pragma once
#include "ECS.h"

namespace AnimationTools {
struct FrameUv {
    float uMin = 0;
    float vMin = 0;
    float uMax = 1;
    float vMax = 1;
};
void normalize(SpriteAnimationComponent& animation);
void restart(SpriteAnimationComponent& animation);
void step(SpriteAnimationComponent& animation, int direction);
void advance(SpriteAnimationComponent& animation, float dt);
FrameUv uv(const SpriteAnimationComponent& animation);
}
