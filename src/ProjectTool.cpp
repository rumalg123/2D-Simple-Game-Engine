#include "ProjectManager.h"
#include "AssetWorkspace.h"
#include "AssetImportPipeline.h"
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
    if (argc < 3 || argc > 7) {
        std::cerr << "Usage:\n"
                  << "  simpleengine_project create <parent-directory> <project-name>\n"
                  << "  simpleengine_project info <project-directory>\n"
                  << "  simpleengine_project validate <project-directory>\n"
                  << "  simpleengine_project assets-sync <project-directory>\n"
                  << "  simpleengine_project assets-audit <project-directory>\n"
                  << "  simpleengine_project assets-list <project-directory>\n"
                  << "  simpleengine_project assets-deps <project-directory> <asset-id>\n"
                  << "  simpleengine_project assets-move <project-directory> <asset-id> <new-asset-path>\n"
                  << "  simpleengine_project assets-build <project-directory> [target-platform]\n"
                  << "  simpleengine_project assets-clean <project-directory> [target-platform]\n"
                  << "  simpleengine_project assets-import-settings <project-directory> <asset-id> <max-size> <flipY:0|1> <premultiply:0|1>\n";
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
    } else if ((operation=="assets-build" || operation=="assets-clean") &&
               (argc==3 || argc==4)) {
        success=ProjectManager::open(argv[2],project,error);
    } else if (operation=="assets-import-settings" && argc==7) {
        success=ProjectManager::open(argv[2],project,error);
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

    if(operation=="assets-build") {
        const std::string platform=argc==4 ? argv[3] : "windows-x64";
        const auto report=AssetImportPipeline::build(project.root,platform);
        for(const auto& issue:report.errors)
            std::cerr<<"Import: "<<issue<<'\n';
        if(!report.ok()) return 1;
        std::cout<<"Asset import: "<<report.built<<" rebuilt, "
                 <<report.cached<<" cached, "<<report.artifacts.size()
                 <<" total; "<<report.outputBytes<<" artifact bytes.\n";
        return 0;
    }
    if(operation=="assets-clean") {
        const std::string platform=argc==4 ? argv[3] : "windows-x64";
        if(!AssetImportPipeline::clean(project.root,platform,error)) {
            std::cerr<<"Cache clean: "<<error<<'\n';
            return 1;
        }
        std::cout<<"Cleaned import cache for "<<platform<<"\n";
        return 0;
    }
    if (operation.rfind("assets-", 0) == 0) {
        AssetWorkspace assets(project.config.assetRoot);
        if(!assets.open(error)) {
            std::cerr << "Asset database error: " << error << '\n';
            return 1;
        }
        if(operation=="assets-import-settings") {
            TextureImportSettings settings;
            try {
                settings.maxDimension=std::stoi(argv[4]);
            } catch(const std::exception&) {
                std::cerr<<"Invalid max texture dimension.\n";
                return 2;
            }
            const std::string flip=argv[5],alpha=argv[6];
            if((flip!="0" && flip!="1") ||
               (alpha!="0" && alpha!="1")) {
                std::cerr<<"flipY and premultiply must be 0 or 1.\n";
                return 2;
            }
            settings.flipY=flip=="1";
            settings.premultiplyAlpha=alpha=="1";
            if(!assets.setTextureSettings(argv[3],settings,error)) {
                std::cerr<<"Texture settings: "<<error<<'\n';
                return 1;
            }
            std::cout<<"Saved import settings for "<<argv[3]<<".\n";
            return 0;
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
