#include "Engine.h"
#include "imgui.h"

namespace {
void getCameraHalfSizeForDisplay(const Camera& camera, float displayWidth, float displayHeight, float& halfWidth, float& halfHeight) {
    halfWidth = camera.halfWidth;
    halfHeight = camera.halfHeight;

    const float viewportAspect = displayHeight > 0.0f ? displayWidth / displayHeight : 1.0f;
    const float cameraAspect = halfHeight != 0.0f ? halfWidth / halfHeight : 1.0f;
    if (viewportAspect > cameraAspect) {
        halfWidth = halfHeight * viewportAspect;
    } else {
        halfHeight = halfWidth / viewportAspect;
    }
}

ImVec2 worldToScreen(const TransformComponent& transform, const Camera& camera, const ImVec2& displaySize) {
    float cameraHalfWidth = 1.0f;
    float cameraHalfHeight = 1.0f;
    getCameraHalfSizeForDisplay(camera, displaySize.x, displaySize.y, cameraHalfWidth, cameraHalfHeight);

    const float normalizedX = (transform.x - camera.x) / cameraHalfWidth;
    const float normalizedY = (transform.y - camera.y) / cameraHalfHeight;
    return {
        (normalizedX * 0.5f + 0.5f) * displaySize.x,
        (0.5f - normalizedY * 0.5f) * displaySize.y
    };
}

TransformComponent screenToWorld(const ImVec2& screenPosition, const Camera& camera, const ImVec2& displaySize) {
    float cameraHalfWidth = 1.0f;
    float cameraHalfHeight = 1.0f;
    getCameraHalfSizeForDisplay(camera, displaySize.x, displaySize.y, cameraHalfWidth, cameraHalfHeight);

    const float normalizedX = (screenPosition.x / displaySize.x) * 2.0f - 1.0f;
    const float normalizedY = 1.0f - (screenPosition.y / displaySize.y) * 2.0f;
    return {
        camera.x + normalizedX * cameraHalfWidth,
        camera.y + normalizedY * cameraHalfHeight
    };
}
}

void Engine::handleGizmo() {
    if (selectedEntity == InvalidEntity || !scene.isValidEntity(selectedEntity)) {
        gizmoDragging = false;
        return;
    }

    NameComponent* name = scene.getName(selectedEntity);
    TagComponent* tag = scene.getTag(selectedEntity);
    TransformComponent* transform = scene.getTransform(selectedEntity);
    if (!transform) {
        gizmoDragging = false;
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f) {
        return;
    }

    const ImVec2 handlePosition = worldToScreen(*transform, scene.getCamera(), io.DisplaySize);
    const ImVec2 mousePosition = io.MousePos;
    const float deltaX = mousePosition.x - handlePosition.x;
    const float deltaY = mousePosition.y - handlePosition.y;
    const float handleRadius = 13.0f;
    const bool mouseOnHandle = (deltaX * deltaX + deltaY * deltaY) <= handleRadius * handleRadius;

    if (!io.WantCaptureMouse && mouseOnHandle && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        gizmoDragging = true;
    }

    if (gizmoDragging && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const TransformComponent newPosition = screenToWorld(mousePosition, scene.getCamera(), io.DisplaySize);
        transform->x = newPosition.x;
        transform->y = newPosition.y;

        PhysicsComponent* physics = scene.getPhysics(selectedEntity);
        if (physics) {
            physics->velocityX = 0.0f;
            physics->velocityY = 0.0f;
            physics->forceX = 0.0f;
            physics->forceY = 0.0f;
        }
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        gizmoDragging = false;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const ImU32 fillColor = gizmoDragging ? IM_COL32(255, 210, 70, 255) : IM_COL32(80, 220, 255, 230);
    drawList->AddCircleFilled(handlePosition, handleRadius, fillColor, 24);
    drawList->AddCircle(handlePosition, handleRadius + 4.0f, IM_COL32(255, 255, 255, 210), 24, 2.0f);
    drawList->AddLine({handlePosition.x - 28.0f, handlePosition.y}, {handlePosition.x + 28.0f, handlePosition.y}, IM_COL32(255, 255, 255, 180), 2.0f);
    drawList->AddLine({handlePosition.x, handlePosition.y - 28.0f}, {handlePosition.x, handlePosition.y + 28.0f}, IM_COL32(255, 255, 255, 180), 2.0f);
}
