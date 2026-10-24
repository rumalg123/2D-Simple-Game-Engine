#include "AnimationTools.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace AnimationTools {
const AnimationClip* findClip(const SpriteAnimationComponent& animation,
                              const std::string& name) {
    for (const auto& clip: animation.clips)
        if (clip.name == name) return &clip;
    return nullptr;
}
namespace {
bool validClip(const SpriteAnimationComponent& animation,const AnimationClip& c) {
    if(c.name.empty() || c.name.size()>64 || c.firstFrame<0 ||
       c.frameCount<1 || c.secondsPerFrame<=0 ||
       !std::isfinite(c.secondsPerFrame)) return false;
    const int cols=std::clamp(animation.columns,1,256);
    const int rows=std::clamp(animation.rows,1,256);
    return c.firstFrame < cols*rows &&
           c.frameCount <= cols*rows-c.firstFrame;
}
}
bool addClip(SpriteAnimationComponent& animation,AnimationClip clip) {
    if(!validClip(animation,clip) || findClip(animation,clip.name) ||
       animation.clips.size()>=64) return false;
    animation.clips.push_back(std::move(clip));
    return true;
}
bool updateClip(SpriteAnimationComponent& animation,AnimationClip clip) {
    if(!validClip(animation,clip)) return false;
    for(auto& existing:animation.clips)
        if(existing.name==clip.name) {
            existing=clip;
            if(animation.activeClip==clip.name) selectClip(animation,clip.name,false);
            return true;
        }
    return false;
}
bool removeClip(SpriteAnimationComponent& animation,const std::string& name) {
    for(auto it=animation.clips.begin();it!=animation.clips.end();++it)
        if(it->name==name) {
            animation.clips.erase(it);
            if(animation.activeClip==name) animation.activeClip.clear();
            return true;
        }
    return false;
}
bool selectClip(SpriteAnimationComponent& animation,const std::string& name,bool reset) {
    const AnimationClip* clip=findClip(animation,name);
    if(!clip) return false;
    const AnimationClip selected=*clip;
    const bool switched=animation.activeClip!=name;
    animation.activeClip=name;
    animation.firstFrame=selected.firstFrame;
    animation.frameCount=selected.frameCount;
    animation.secondsPerFrame=selected.secondsPerFrame;
    animation.loop=selected.loop;
    if(reset || switched) {
        animation.currentFrame=0;
        animation.elapsedSeconds=0;
        animation.playing=true;
    }
    normalize(animation);
    return true;
}

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
