#include "Engine.h"
#include "AnimationTools.h"
#include "imgui.h"

#include <algorithm>
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
    if(editable) AnimationTools::normalize(animation);
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
    const int oldFrame=animation.currentFrame;
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
    if(oldFrame != animation.currentFrame && editable) {
        if(SpriteComponent* sprite=scene.getSprite(selectedEntity)) {
            sprite->uMin=uv.uMin;sprite->vMin=uv.vMin;
            sprite->uMax=uv.uMax;sprite->vMax=uv.vMax;
        }
    }
    ImGui::TextDisabled("Animated sprite frame state is serialized with the scene.");
    ImGui::End();
}
