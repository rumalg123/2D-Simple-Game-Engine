#pragma once

#include "ProjectManager.h"

// Returns true when a project has been selected. False means the window was
// closed or cancelled. The launcher owns its GLFW/ImGui context and shuts
// it down before the normal editor initializes its own window.
bool showProjectLauncher(ProjectInfo& selected);
