#include "Engine.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <memory>
#include <vector>

namespace {
struct OwnedImGuiDrawData {
    ImDrawData drawData;
    std::vector<ImDrawList*> ownedLists;

    ~OwnedImGuiDrawData() {
        for (ImDrawList* drawList : ownedLists) {
            IM_DELETE(drawList);
        }
    }
};

std::shared_ptr<OwnedImGuiDrawData> cloneImGuiDrawData(ImDrawData* source) {
    auto owned = std::make_shared<OwnedImGuiDrawData>();
    owned->drawData.Clear();
    owned->drawData.DisplayPos = source->DisplayPos;
    owned->drawData.DisplaySize = source->DisplaySize;
    owned->drawData.FramebufferScale = source->FramebufferScale;
    owned->drawData.OwnerViewport = source->OwnerViewport;
    owned->drawData.Textures = source->Textures;

    owned->ownedLists.reserve(source->CmdListsCount);
    for (int index = 0; index < source->CmdListsCount; ++index) {
        ImDrawList* clonedList = source->CmdLists[index]->CloneOutput();
        owned->ownedLists.push_back(clonedList);
        owned->drawData.CmdLists.push_back(clonedList);
        owned->drawData.TotalVtxCount += clonedList->VtxBuffer.Size;
        owned->drawData.TotalIdxCount += clonedList->IdxBuffer.Size;
    }

    owned->drawData.CmdListsCount = owned->drawData.CmdLists.Size;
    owned->drawData.Valid = source->Valid;
    return owned;
}

}

bool Engine::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = config.editorLayoutPath.c_str();

    ImGui::StyleColorsDark();

    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 120")) {
        ImGui_ImplGlfw_Shutdown();
        return false;
    }

    if (!ImGui_ImplOpenGL3_CreateDeviceObjects()) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        return false;
    }

    imguiInitialized = true;
    return true;
}

void Engine::shutdownImGui() {
    if (!imguiInitialized) {
        return;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    imguiInitialized = false;
}

void Engine::beginUiFrame() {
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Engine::renderUi() {
    renderPlayPausePanel();
    renderSceneHierarchyPanel();
    renderInspectorPanel();
    renderAssetBrowserPanel();
    PluginContext pluginContext = makePluginContext();
    pluginManager.renderUi(pluginContext);
}

void Engine::endUiFrame() {
    ImGui::Render();
    std::shared_ptr<OwnedImGuiDrawData> drawData = cloneImGuiDrawData(ImGui::GetDrawData());
    submitRenderCommand("Prepare ImGui OpenGL backend", []() {
        ImGui_ImplOpenGL3_NewFrame();
    });
    submitRenderCommand("Render ImGui draw data", [drawData]() {
        ImGui_ImplOpenGL3_RenderDrawData(&drawData->drawData);
    });
    submitRenderCommand("Swap buffers", [this]() {
        glfwSwapBuffers(window);
    });
    renderCommands.submitFrame();
}

bool Engine::editorWantsKeyboardCapture() const {
    return ImGui::GetIO().WantCaptureKeyboard;
}
