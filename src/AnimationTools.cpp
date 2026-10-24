#include "AnimationTools.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace AnimationTools {
void normalize(SpriteAnimationComponent& a) {
    a.columns = std::clamp(a.columns, 1, 256);
    a.rows = std::clamp(a.rows, 1, 256);
    const int total = a.columns * a.rows;
    a.firstFrame = std::clamp(a.firstFrame, 0, total - 1);
    a.frameCount = std::clamp(a.frameCount, 1, total - a.firstFrame);
    a.currentFrame = std::clamp(a.currentFrame, 0, a.frameCount - 1);
    if (!std::isfinite(a.secondsPerFrame))
        a.secondsPerFrame = 0.1f;
    a.secondsPerFrame = std::clamp(a.secondsPerFrame, 0.001f, 10.0f);
    if (!std::isfinite(a.elapsedSeconds) || a.elapsedSeconds < 0)
        a.elapsedSeconds = 0;
}
void restart(SpriteAnimationComponent& a) {
    normalize(a);
    a.currentFrame = 0;
    a.elapsedSeconds = 0.0f;
    a.playing = true;
}
void step(SpriteAnimationComponent& a, int direction) {
    normalize(a);
    a.currentFrame = std::clamp(a.currentFrame + (direction > 0 ? 1 : -1),
                                0, a.frameCount - 1);
    a.elapsedSeconds = 0;
}
void advance(SpriteAnimationComponent& a, float dt) {
    normalize(a);
    if (!a.playing || !std::isfinite(dt) || dt <= 0) return;
    a.elapsedSeconds += dt;
    if (!std::isfinite(a.elapsedSeconds)) a.elapsedSeconds = 0;
    const int framesToAdvance = std::min(100000,
        static_cast<int>(a.elapsedSeconds / a.secondsPerFrame));
    if (framesToAdvance <= 0) return;
    a.elapsedSeconds -= framesToAdvance * a.secondsPerFrame;
    const int next = a.currentFrame + framesToAdvance;
    if (a.loop) a.currentFrame = next % a.frameCount;
    else if (next >= a.frameCount) {
        a.currentFrame = a.frameCount - 1;
        a.elapsedSeconds = 0;
        a.playing = false;
    } else a.currentFrame = next;
}
FrameUv uv(const SpriteAnimationComponent& original) {
    SpriteAnimationComponent a = original;
    normalize(a);
    const int frame = a.firstFrame + a.currentFrame;
    const int column = frame % a.columns;
    const int row = frame / a.columns;
    const float fw = 1.0f / a.columns, fh = 1.0f / a.rows;
    const float u = column * fw, v = 1.0f - row * fh;
    return {u, v - fh, u + fw, v};
}
}
