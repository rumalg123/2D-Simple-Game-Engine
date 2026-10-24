#include "ProjectManager.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main() {
    const auto seed = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto parent = std::filesystem::temp_directory_path() /
                        ("simpleengine_project_test_" + std::to_string(seed));
    std::filesystem::create_directories(parent);
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code ec; std::filesystem::remove_all(path, ec); }
    } cleanup{parent};

    ProjectInfo created;
    std::string error;
    if (!ProjectManager::create(parent, "My Platformer", created, error)) {
        std::cerr << error << '\n';
        return 1;
    }
    if (!std::filesystem::exists(created.root / ProjectManager::ManifestName) ||
        !std::filesystem::is_directory(created.root / "Assets/Scenes") ||
        !std::filesystem::is_directory(created.root / "Assets/Scripts")) {
        std::cerr << "Project structure missing.\n";
        return 1;
    }
    if (created.config.windowTitle != "My Platformer" ||
        created.config.editorEnabled != true || created.config.startPlaying != false) {
        std::cerr << "Project configuration not loaded.\n";
        return 1;
    }
    ProjectInfo reopened;
    if (!ProjectManager::open(created.root, reopened, error) ||
        reopened.config.assetRoot != created.config.assetRoot) {
        std::cerr << "Reopening project failed: " << error << '\n';
        return 1;
    }
    if (ProjectManager::create(parent, "My Platformer", reopened, error)) {
        std::cerr << "An existing project was overwritten.\n";
        return 1;
    }
    if (ProjectManager::create(parent, "../escape", reopened, error) ||
        ProjectManager::create(parent, "CON", reopened, error)) {
        std::cerr << "Invalid project names were accepted.\n";
        return 1;
    }

    // Test that a project cannot resolve assets outside its directory.
    const auto manifest = created.root / ProjectManager::ManifestName;
    {
        std::ofstream out(manifest, std::ios::trunc);
        out << "{\"assetRoot\": \"../outside\", \"prefabDirectory\": \"Assets/Prefabs\"}";
    }
    if (ProjectManager::open(created.root, reopened, error)) {
        std::cerr << "Escaping asset directory was accepted.\n";
        return 1;
    }
    std::cout << "Project creation, reopening and validation passed.\n";
    return 0;
}
