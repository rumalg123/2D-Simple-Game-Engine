#include "ProjectManager.h"

#include <cctype>
#include <fstream>
#include <system_error>

namespace {
bool validName(const std::string& name) {
    if (name.empty() || name.size() > 64 || name.front() == ' ' ||
        name.back() == ' ' || name.back() == '.') {
        return false;
    }
    for (unsigned char ch : name) {
        if (std::isalnum(ch) == 0 && ch != ' ' && ch != '_' && ch != '-') {
            return false;
        }
    }
    std::string upper;
    for (unsigned char ch : name) {
        upper += static_cast<char>(std::toupper(ch));
    }
    if (upper == "CON" || upper == "PRN" || upper == "AUX" || upper == "NUL" ||
        upper == "COM1" || upper == "COM2" || upper == "COM3" ||
        upper == "COM4" || upper == "COM5" || upper == "COM6" ||
        upper == "COM7" || upper == "COM8" || upper == "COM9" ||
        upper == "LPT1" || upper == "LPT2" || upper == "LPT3" ||
        upper == "LPT4" || upper == "LPT5" || upper == "LPT6" ||
        upper == "LPT7" || upper == "LPT8" || upper == "LPT9") {
        return false;
    }
    return true;
}

std::string escapeJson(const std::string& value) {
    std::string out;
    for (char ch : value) {
        if (ch == '\\' || ch == '"') out.push_back('\\');
        out.push_back(ch);
    }
    return out;
}

bool isInside(const std::filesystem::path& parent, const std::filesystem::path& candidate) {
    const auto p = parent.lexically_normal();
    const auto c = candidate.lexically_normal();
    auto left = p.begin();
    auto right = c.begin();
    for (; left != p.end(); ++left, ++right) {
        if (right == c.end() || *left != *right) return false;
    }
    return true;
}
}

bool ProjectManager::create(const std::filesystem::path& parent,
                            const std::string& name,
                            ProjectInfo& project,
                            std::string& error) {
    if (!validName(name)) {
        error = "Project name must be 1-64 letters, digits, spaces, '_' or '-', "
                "and must not be a reserved Windows device name.";
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::is_directory(parent, ec)) {
        error = "The parent directory does not exist.";
        return false;
    }
    const auto root = std::filesystem::absolute(parent, ec) / name;
    if (ec) {
        error = "Cannot resolve the parent directory: " + ec.message();
        return false;
    }
    if (!std::filesystem::create_directory(root, ec)) {
        error = ec ? "Cannot create project: " + ec.message()
                   : "Project directory already exists; refusing to overwrite it.";
        return false;
    }

    bool success = false;
    do {
        for (const auto* folder : {"Assets/Scenes", "Assets/Sprites", "Assets/Scripts",
                                   "Assets/Audio", "Assets/Prefabs", "ProjectSettings"}) {
            std::filesystem::create_directories(root / folder, ec);
            if (ec) { error = "Cannot initialize project folders: " + ec.message(); break; }
        }
        if (ec) break;

        std::ofstream output(root / ManifestName, std::ios::binary | std::ios::trunc);
        if (!output) { error = "Cannot write project manifest."; break; }
        output << "{\n"
               << "  \"formatVersion\": 1,\n"
               << "  \"windowTitle\": \"" << escapeJson(name) << "\",\n"
               << "  \"windowWidth\": 1280,\n"
               << "  \"windowHeight\": 720,\n"
               << "  \"editorEnabled\": true,\n"
               << "  \"startPlaying\": false,\n"
               << "  \"assetRoot\": \"Assets\",\n"
               << "  \"prefabDirectory\": \"Assets/Prefabs\",\n"
               << "  \"editorLayoutPath\": \"ProjectSettings/editor.ini\"\n"
               << "}\n";
        output.close();
        if (!output) { error = "Could not finish writing the project manifest."; break; }

        success = open(root, project, error);
    } while (false);

    if (!success) {
        // Only remove the newly created folder, never an existing user project.
        std::filesystem::remove_all(root, ec);
    }
    return success;
}

bool ProjectManager::open(const std::filesystem::path& root,
                          ProjectInfo& project,
                          std::string& error) {
    std::error_code ec;
    const auto canonicalRoot = std::filesystem::canonical(root, ec);
    if (ec || !std::filesystem::is_directory(canonicalRoot)) {
        error = "Cannot open project directory.";
        return false;
    }
    const auto manifest = canonicalRoot / ManifestName;
    if (!std::filesystem::is_regular_file(manifest)) {
        error = "Not a SimpleEngine project: missing project.simple.json.";
        return false;
    }

    GameConfig config;
    if (!loadGameConfigFromFile(manifest.string(), config, error)) {
        return false;
    }

    // Config paths in a project are relative; reject external traversal.
    for (const std::string* path : {&config.assetRoot, &config.prefabDirectory,
                                    &config.editorLayoutPath}) {
        const std::filesystem::path relative(*path);
        if (relative.is_absolute() || !isInside(canonicalRoot, canonicalRoot / relative)) {
            error = "Project configuration contains an invalid external path.";
            return false;
        }
    }

    const auto assetRoot = std::filesystem::weakly_canonical(canonicalRoot / config.assetRoot, ec);
    if (ec || !std::filesystem::is_directory(assetRoot) ||
        !isInside(canonicalRoot, assetRoot)) {
        error = "Project asset folder is missing or resolves outside the project.";
        return false;
    }

    const auto prefabRoot = std::filesystem::weakly_canonical(
        canonicalRoot / config.prefabDirectory, ec);
    if (ec || !isInside(canonicalRoot, prefabRoot)) {
        error = "Project prefab folder resolves outside the project.";
        return false;
    }
    const auto layoutPath = std::filesystem::weakly_canonical(
        canonicalRoot / config.editorLayoutPath, ec);
    if (ec || !isInside(canonicalRoot, layoutPath)) {
        error = "Project editor layout resolves outside the project.";
        return false;
    }

    config.assetRoot = assetRoot.string();
    config.prefabDirectory = prefabRoot.string();
    config.editorLayoutPath = layoutPath.string();
    project = {canonicalRoot, canonicalRoot.filename().string(), config};
    return true;
}
