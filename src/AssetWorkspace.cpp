#include "AssetWorkspace.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

namespace {
bool insideRoot(const std::filesystem::path& root,
                const std::filesystem::path& file) {
    const auto relative=file.lexically_relative(root);
    return !relative.empty() && !relative.is_absolute() &&
           *relative.begin()!="..";
}
bool isKnownRef(const AssetManifest& index,const std::string& id) {
    return !id.empty() && index.findById(id);
}
bool hasJsonSuffix(const std::string& name) {
    return name.size()>=5 && name.compare(name.size()-5,5,".json")==0;
}
}
AssetWorkspace::AssetWorkspace(std::filesystem::path root)
    : root_(std::move(root)) {}

bool AssetWorkspace::open(std::string& error) {
    if(!std::filesystem::is_directory(root_)) {
        error="Missing asset root: "+root_.string();return false;
    }
    const auto path=AssetManifest::defaultManifestPath(root_);
    if(std::filesystem::is_regular_file(path) &&
       !catalog_.loadFromFile(path.string(),error)) return false;
    return true;
}
bool AssetWorkspace::sync(std::size_t& imported,std::string& error) {
    imported=0;
    try { imported=catalog_.scanDirectory(root_); }
    catch(const std::exception& e) {error=e.what();return false;}
    return catalog_.saveToFile(AssetManifest::defaultManifestPath(root_).string(),error);
}
AssetAudit AssetWorkspace::audit() const {
    AssetAudit result;
    result.indexed=catalog_.entryCount();
    std::set<std::string> guids, paths;
    for(const auto& entry:catalog_.getEntries()) {
        if(entry.guid.empty() || !guids.insert(entry.guid).second)
            result.errors.push_back("Missing or duplicated GUID: "+entry.id);
        if(entry.path.empty() || !paths.insert(entry.path).second)
            result.errors.push_back("Missing or duplicated path: "+entry.id);
        const auto file=root_/entry.path;
        if(!insideRoot(root_,file) ||
           !std::filesystem::is_regular_file(file) ||
           std::filesystem::is_symlink(file)) {
            result.errors.push_back("Missing/unsafe asset file: "+entry.id+" -> "+entry.path);
            continue;
        }
        if(entry.type==AssetType::Unknown)
            result.errors.push_back("Unsupported asset type: "+entry.id);
        for(const auto& dependency:entry.dependencies)
            if(!isKnownRef(catalog_,dependency))
                result.errors.push_back(entry.id+" missing dependency "+dependency);
        if(entry.type==AssetType::AnimationClip &&
           !entry.animationClip.spriteSheetId.empty() &&
           !isKnownRef(catalog_,entry.animationClip.spriteSheetId))
            result.errors.push_back(entry.id+" missing spriteSheet "+entry.animationClip.spriteSheetId);
        // Source fingerprints are advisory: developers can deliberately edit
        // assets between scans. A sync recomputes hashes without changing GUIDs.
        if(entry.contentHash.empty()) ++result.changed;
    }
    return result;
}
bool AssetWorkspace::move(const std::string& id,const std::string& destination,
                          std::string& error) {
    const auto* entry=catalog_.findById(id);
    if(!entry) {error="No asset with ID "+id;return false;}
    // Scene entrypoints and Lua script registration are still path-based;
    // moving either would require modifying project manifests/scriptName.
    // Restrict managed moves to path-independent file-backed resources.
    if(entry->type==AssetType::Scene || entry->type==AssetType::Script ||
       entry->type==AssetType::Prefab) {
        error="Scene, Lua script and prefab moves are not supported yet; keep their project references intact.";
        return false;
    }
    const std::string previous=entry->path;
    if(!catalog_.moveAsset(root_,id,destination,error)) return false;
    if(!catalog_.saveToFile(
           AssetManifest::defaultManifestPath(root_).string(),error)) {
        std::string rollback;
        catalog_.moveAsset(root_,id,previous,rollback);
        return false;
    }
    return true;
}
