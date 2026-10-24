#include "ProjectManager.h"
#include "SceneSerializer.h"

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
    if (created.config.startupScene.empty() ||
        !std::filesystem::is_regular_file(created.config.startupScene)) {
        std::cerr << "Generated project has no playable startup scene.\n";
        return 1;
    }
    Scene starter;
    ResourceManager resources;
    if (!loadSceneFromJson(starter, resources, created.config.startupScene, error) ||
        starter.findEntityByName("Welcome") == InvalidEntity) {
        std::cerr << "Starter scene cannot be loaded: " << error << '\n';
        return 1;
    }

    // Edit -> save -> reload: the same scene format is used by the editor
    // and the standalone player, so this must round-trip without ImGui.
    const Entity welcome = starter.findEntityByName("Welcome");
    starter.setTransform(welcome, {0.25f, -0.30f});
    if (!saveSceneToJson(starter, resources, created.config.startupScene, error)) {
        std::cerr << "Cannot save edited project scene: " << error << '\\n';
        return 1;
    }
    Scene reloadedScene;
    if (!loadSceneFromJson(reloadedScene, resources, created.config.startupScene, error)) {
        std::cerr << "Cannot reload edited project scene: " << error << '\\n';
        return 1;
    }
    const Entity saved = reloadedScene.findEntityByName("Welcome");
    const TransformComponent* savedTransform =
        saved == InvalidEntity ? nullptr : reloadedScene.getTransform(saved);
    if (!savedTransform || savedTransform->x != 0.25f ||
        savedTransform->y != -0.30f) {
        std::cerr << "Saved project scene did not preserve edits.\\n";
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
    // A missing startup scene is a hard error, not an empty game.
    std::filesystem::rename(created.config.startupScene,
                            created.config.startupScene + ".bak");
    if (ProjectManager::open(created.root, reopened, error)) {
        std::cerr << "Missing startup scene was accepted.\n";
        return 1;
    }
    std::filesystem::rename(created.config.startupScene + ".bak",
                            created.config.startupScene);


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
