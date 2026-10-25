#include "ProjectManager.h"
#include "AssetWorkspace.h"
#include "ProjectValidation.h"
#include "LuaScriptRuntime.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "SceneSerializer.h"
#include "ScriptSystem.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 3 || argc > 5) {
        std::cerr << "Usage:\n"
                  << "  simpleengine_project create <parent-directory> <project-name>\n"
                  << "  simpleengine_project info <project-directory>\n"
                  << "  simpleengine_project validate <project-directory>\n"
                  << "  simpleengine_project assets-sync <project-directory>\n"
                  << "  simpleengine_project assets-audit <project-directory>\n"
                  << "  simpleengine_project assets-list <project-directory>\n"
                  << "  simpleengine_project assets-deps <project-directory> <asset-id>\n"
                  << "  simpleengine_project assets-move <project-directory> <asset-id> <new-asset-path>\n";
        return 2;
    }

    ProjectInfo project;
    std::string error;
    const std::string operation = argv[1];
    bool success = false;
    if (operation == "create" && argc == 4) {
        success = ProjectManager::create(argv[2], argv[3], project, error);
    } else if ((operation == "info" || operation == "validate" ||
                operation == "assets-sync" || operation == "assets-audit" ||
                operation == "assets-list") && argc == 3) {
        success = ProjectManager::open(argv[2], project, error);
    } else if (operation == "assets-deps" && argc == 4) {
        success = ProjectManager::open(argv[2], project, error);
    } else if (operation == "assets-move" && argc == 5) {
        success = ProjectManager::open(argv[2], project, error);
    } else {
        std::cerr << "Unknown command or incorrect arguments.\n";
        return 2;
    }

    if (!success) {
        std::cerr << "Project error: " << error << '\n';
        return 1;
    }

    if (operation.rfind("assets-", 0) == 0) {
        AssetWorkspace assets(project.config.assetRoot);
        if(!assets.open(error)) {
            std::cerr << "Asset database error: " << error << '\n';
            return 1;
        }
        if(operation=="assets-sync") {
            std::size_t imported=0;
            if(!assets.sync(imported,error)) {
                std::cerr << "Asset sync failed: " << error << '\n';
                return 1;
            }
            std::cout << "Asset catalog synchronized: " << imported
                      << " new, " << assets.index().entryCount() << " total.\n";
            return 0;
        }
        if(operation=="assets-move") {
            if(!assets.move(argv[3],argv[4],error)) {
                std::cerr << "Asset move failed: " << error << '\n';
                return 1;
            }
            std::cout << "Moved asset " << argv[3] << " -> " << argv[4] << '\n';
            return 0;
        }
        if(operation=="assets-deps") {
            const std::string target=argv[3];
            const auto* asset=assets.index().findById(target);
            if(!asset) {
                std::cerr<<"Unknown asset ID: "<<target<<'\n';
                return 1;
            }
            std::cout<<"Asset "<<target<<" ["<<asset->guid<<"] "<<asset->path<<'\n';
            for(const auto& dependency:asset->dependencies)
                std::cout<<"  depends on: "<<dependency<<'\n';
            for(const auto& item:assets.index().getEntries())
                for(const auto& ref:item.dependencies)
                    if(ref==target)
                        std::cout<<"  referenced by: "<<item.id<<" ("<<item.path<<")\n";
            return 0;
        }
        if(operation=="assets-list") {
            for(const auto& item:assets.index().getEntries())
                std::cout << item.id << " | " << item.guid << " | "
                          << item.path << " | " << item.contentHash << '\n';
            return 0;
        }
        const AssetAudit report=assets.audit();
        for(const auto& item:report.errors)
            std::cerr << "Asset audit: " << item << '\n';
        std::cout << "Asset audit: " << report.indexed
                  << " indexed, " << report.changed << " missing fingerprints, "
                  << report.errors.size() << " errors.\n";
        return report.valid() ? 0 : 1;
    }

    if (operation == "validate") {
        const ProjectValidationResult result = validateProject(project);
        for (const auto& issue : result.errors)
            std::cerr << "Validation: " << issue << '\n';
        if (!result.ok()) {
            std::cerr << "Project validation failed with "
                      << result.errors.size() << " issue(s).\n";
            return 1;
        }
        std::cout << "Project validated successfully; " << result.scenes
                  << " scene(s), " << result.entities << " entities and "
                  << result.luaScripts << " Lua script(s) compiled.\n";
        return 0;
    }

    std::cout << "Project: " << project.name << '\n'
              << "Root: " << project.root.string() << '\n'
              << "Assets: " << project.config.assetRoot << '\n'
              << "Startup scene: " << (project.config.startupScene.empty()
                                      ? "(not configured)" : project.config.startupScene) << '\n';
    return 0;
}
