#include "Engine.h"
#include "AnimationTools.h"
#include "imgui.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

void Engine::renderAnimationTimelinePanel() {
    ImGui::Begin("Sprite Animation Timeline");
    if (!scene.isValidEntity(selectedEntity) ||
        !scene.getSpriteAnimation(selectedEntity)) {
        ImGui::TextDisabled("Select an entity with a Sprite Animation component.");
        ImGui::End();
        return;
    }
    auto& animation = *scene.getSpriteAnimation(selectedEntity);
    const bool editable = !playSession.active();
    if(!editable) ImGui::TextDisabled("Preview is read-only during Play Mode.");
    if(editable) {
        AnimationTools::normalize(animation);
        if (animation.playing) AnimationTools::advance(
            animation, ImGui::GetIO().DeltaTime);
    }
    ImGui::SeparatorText("Named Animation Clips");
    static char newClipName[64] = "Run";
    ImGui::InputText("New Clip Name",newClipName,sizeof(newClipName));
    if(ImGui::Button("Add Clip") && editable) {
        AnimationClip clip{newClipName,animation.firstFrame,animation.frameCount,
                           animation.secondsPerFrame,animation.loop};
        if(clip.name[0] && !AnimationTools::findClip(animation,clip.name)) {
            recordSceneEdit();
        }
        if(AnimationTools::addClip(animation,clip)) {
            AnimationTools::selectClip(animation,clip.name);
            editorStatus="Added named animation clip: "+clip.name;
        } else editorStatus="Clip name already used or invalid frame range.";
    }
    if(animation.clips.empty())
        ImGui::TextDisabled("Create a named clip for reusable Idle/Run/Jump animations.");
    else {
        const char* current=animation.activeClip.empty() ?
            "(Legacy frame range)" : animation.activeClip.c_str();
        if(ImGui::BeginCombo("Active Clip",current)) {
            for(const auto& clip:animation.clips) {
                bool selected=animation.activeClip==clip.name;
                if(ImGui::Selectable(clip.name.c_str(),selected) && editable) {
                    recordSceneEdit();
                    AnimationTools::selectClip(animation,clip.name);
                }
                if(selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if(!animation.activeClip.empty()) {
            const AnimationClip* original=AnimationTools::findClip(animation,animation.activeClip);
            if(original) {
                AnimationClip modified=*original;
                bool changed=false;
                ImGui::BeginDisabled(!editable);
                changed |= ImGui::InputInt("Clip First Frame",&modified.firstFrame);
                changed |= ImGui::InputInt("Clip Frame Count",&modified.frameCount);
                changed |= ImGui::DragFloat("Clip Frame Duration",
                                              &modified.secondsPerFrame,0.005f,0.001f,2.0f);
                changed |= ImGui::Checkbox("Clip Loop",&modified.loop);
                ImGui::EndDisabled();
                if(changed && editable) {
                    // Caller can make an undo checkpoint before continuous edits.
                    if(!AnimationTools::updateClip(animation,modified))
                        editorStatus="Clip values exceed sprite-sheet boundaries.";
                }
                if(ImGui::Button("Delete Active Clip") && editable) {
                    recordSceneEdit();
                    AnimationTools::removeClip(animation,modified.name);
                }
            }
        }
    }

    const int total=std::max(1,animation.columns)*std::max(1,animation.rows);
    ImGui::Text("Sprite sheet: %d columns x %d rows (%d frames)",
                animation.columns,animation.rows,total);
    ImGui::Text("Frame range: [%d..%d]",animation.firstFrame,
                animation.firstFrame+animation.frameCount-1);
    if(ImGui::Button(animation.playing ? "Pause Preview" : "Play Preview") && editable)
        animation.playing=!animation.playing;
    ImGui::SameLine();
    if(ImGui::Button("Restart") && editable) AnimationTools::restart(animation);
    ImGui::SameLine();
    if(ImGui::Button("< Step") && editable) {
        animation.playing=false;
        AnimationTools::step(animation,-1);
    }
    ImGui::SameLine();
    if(ImGui::Button("Step >") && editable) {
        animation.playing=false;
        AnimationTools::step(animation,1);
    }
    ImGui::BeginDisabled(!editable);
    if(ImGui::SliderInt("Timeline Frame",&animation.currentFrame,
                        0,std::max(0,animation.frameCount-1))) {
        animation.playing=false;
        animation.elapsedSeconds=0;
    }
    ImGui::DragFloat("Seconds Per Frame",&animation.secondsPerFrame,
                     0.005f,0.001f,2.0f,"%.3f sec");
    ImGui::Checkbox("Loop",&animation.loop);
    ImGui::EndDisabled();

    ImGui::SeparatorText("Frame Strip");
    if(ImGui::BeginChild("AnimationFrames",ImVec2(0,110),true,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        for(int i=0;i<std::min(animation.frameCount,256);++i) {
            if(i>0) ImGui::SameLine();
            ImGui::PushID(i);
            const bool selected=i==animation.currentFrame;
            if(selected) ImGui::PushStyleColor(ImGuiCol_Button,IM_COL32(105,134,56,255));
            std::string label=std::to_string(animation.firstFrame+i);
            if(ImGui::Button(label.c_str(),ImVec2(38,56)) && editable) {
                animation.currentFrame=i;
                animation.playing=false;
                animation.elapsedSeconds=0.0f;
            }
            if(selected) ImGui::PopStyleColor();
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    const auto uv=AnimationTools::uv(animation);
    ImGui::Text("UV preview: (%.3f, %.3f) .. (%.3f, %.3f)",
                uv.uMin,uv.vMin,uv.uMax,uv.vMax);
    // Scrubbing in Edit Mode must immediately update the sprite UVs; otherwise
    // previews would only change after the next simulation tick.
    if(editable) {
        if(SpriteComponent* sprite=scene.getSprite(selectedEntity)) {
            sprite->uMin=uv.uMin;sprite->vMin=uv.vMin;
            sprite->uMax=uv.uMax;sprite->vMax=uv.vMax;
        }
    }
    ImGui::TextDisabled("Animated sprite frame state is serialized with the scene.");
    ImGui::End();
}
