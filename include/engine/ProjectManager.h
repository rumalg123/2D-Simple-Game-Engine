#pragma once

#include "GameConfig.h"

#include <filesystem>
#include <string>

// A first-stage game project: one directory containing a project configuration
// and separate authoring folders. Editor integration and scene GUIDs follow later.
struct ProjectInfo {
    std::filesystem::path root;
    std::string name;
    GameConfig config;
};

class ProjectManager {
public:
    static constexpr const char* ManifestName = "project.simple.json";

    // Never overwrites an existing directory.
    static bool create(const std::filesystem::path& parent,
                       const std::string& name,
                       ProjectInfo& project,
                       std::string& error);

    // Validates and resolves project-owned paths without changing the process CWD.
    static bool open(const std::filesystem::path& root,
                     ProjectInfo& project,
                     std::string& error);
};
