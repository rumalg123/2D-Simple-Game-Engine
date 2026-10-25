#pragma once

#include "ProjectManager.h"

#include <cstddef>
#include <string>
#include <vector>

// Headless, side-effect-free project audit used by the CLI, player and export
// pipeline. No Dear ImGui, GLFW window, or Lua script execution is required.
struct ProjectValidationResult {
    std::size_t scenes = 0;
    std::size_t entities = 0;
    std::size_t luaScripts = 0;
    std::vector<std::string> errors;

    bool ok() const { return errors.empty(); }
};

ProjectValidationResult validateProject(const ProjectInfo& project);
