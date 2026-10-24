#include "ProjectManager.h"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 3 || argc > 4) {
        std::cerr << "Usage:\n"
                  << "  simpleengine_project create <parent-directory> <project-name>\n"
                  << "  simpleengine_project info <project-directory>\n";
        return 2;
    }

    ProjectInfo project;
    std::string error;
    const std::string operation = argv[1];
    bool success = false;
    if (operation == "create" && argc == 4) {
        success = ProjectManager::create(argv[2], argv[3], project, error);
    } else if (operation == "info" && argc == 3) {
        success = ProjectManager::open(argv[2], project, error);
    } else {
        std::cerr << "Unknown command or incorrect arguments.\n";
        return 2;
    }

    if (!success) {
        std::cerr << "Project error: " << error << '\n';
        return 1;
    }
    std::cout << "Project: " << project.name << '\n'
              << "Root: " << project.root.string() << '\n'
              << "Assets: " << project.config.assetRoot << '\n';
    return 0;
}
