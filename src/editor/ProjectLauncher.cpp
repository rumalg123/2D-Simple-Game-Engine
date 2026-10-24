#include "ProjectLauncher.h"

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <array>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

bool showProjectLauncher(ProjectInfo& selected) {
    if (!glfwInit()) {
        std::cerr << "Project launcher: cannot initialize GLFW.\n";
        return false;
    }

    GLFWwindow* window = glfwCreateWindow(840, 510, "SimpleEngine - Projects",
                                          nullptr, nullptr);
    if (!window) {
        std::cerr << "Project launcher: cannot create a window.\n";
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Project launcher: failed to load OpenGL.\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();

    const bool glfwReady = ImGui_ImplGlfw_InitForOpenGL(window, true);
    const bool glReady = glfwReady && ImGui_ImplOpenGL3_Init("#version 120");
    if (!glReady) {
        std::cerr << "Project launcher: failed to initialize ImGui.\n";
        if (glfwReady) ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return false;
    }

    std::array<char, 1024> parent{};
    std::array<char, 1024> existing{};
    std::array<char, 128> name{};
    const std::string cwd = std::filesystem::current_path().string();
    std::snprintf(parent.data(), parent.size(), "%s", cwd.c_str());
    std::snprintf(existing.data(), existing.size(), "%s", cwd.c_str());
    std::snprintf(name.data(), name.size(), "MyFirstGame");

    std::string feedback;
    bool accepted = false;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        ImGui::SetNextWindowPos(ImVec2(16, 16), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(static_cast<float>(width - 32),
                                         static_cast<float>(height - 32)),
                                 ImGuiCond_Always);
        ImGui::Begin("SimpleEngine Projects", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
        ImGui::Text("Welcome to SimpleEngine");
        ImGui::TextDisabled("Create a 2D game project or open an existing one.");
        ImGui::SeparatorText("Create Project");
        ImGui::InputText("Parent folder", parent.data(), parent.size());
        ImGui::InputText("Project name", name.data(), name.size());
        if (ImGui::Button("Create and Open")) {
            std::string error;
            if (ProjectManager::create(parent.data(), name.data(), selected, error)) {
                accepted = true;
            } else {
                feedback = error;
            }
        }
        ImGui::Spacing();
        ImGui::SeparatorText("Open Project");
        ImGui::InputText("Project folder", existing.data(), existing.size());
        if (ImGui::Button("Open Existing Project")) {
            std::string error;
            if (ProjectManager::open(existing.data(), selected, error)) {
                accepted = true;
            } else {
                feedback = error;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) glfwSetWindowShouldClose(window, GLFW_TRUE);

        ImGui::Spacing();
        if (!feedback.empty()) {
            ImGui::TextWrapped("Project error: %s", feedback.c_str());
        }
        ImGui::TextDisabled("Projects keep their own scenes, assets and settings.");
        ImGui::End();

        ImGui::Render();
        glViewport(0, 0, width, height);
        glClearColor(0.10f, 0.11f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
        if (accepted) break;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return accepted;
}
