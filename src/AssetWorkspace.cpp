#include "AssetWorkspace.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <set>
#include <string>

namespace {
std::string fingerprint(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);
    if(!file) return {};
    std::uint64_t result=14695981039346656037ULL;
    char data[8192];
    while(file) {
        file.read(data,sizeof(data));
        for(std::streamsize i=0;i<file.gcount();++i) {
            result^=static_cast<unsigned char>(data[i]);
            result*=1099511628211ULL;
        }
    }
    std::ostringstream out;
    out<<std::hex<<std::setfill('0')<<std::setw(16)<<result;
    return out.str();
}
std::vector<std::string> discoverDependencies(
    const AssetManifest& manifest, const AssetManifestEntry& entry,
    const std::filesystem::path& file) {
    std::set<std::string> referenced;
    if(entry.type==AssetType::AnimationClip &&
       !entry.animationClip.spriteSheetId.empty())
        referenced.insert(entry.animationClip.spriteSheetId);
    if(entry.type!=AssetType::Scene && entry.type!=AssetType::Prefab)
        return {referenced.begin(),referenced.end()};
    std::error_code ec;
    if(std::filesystem::file_size(file,ec)>8*1024*1024 || ec)
        return {referenced.begin(),referenced.end()};
    std::ifstream input(file);
    if(!input) return {referenced.begin(),referenced.end()};
    const std::string data((std::istreambuf_iterator<char>(input)),
                           std::istreambuf_iterator<char>());
    const std::regex textures(R"ASSET("textureName"\s*:\s*"([^"]+)")ASSET");
    const std::regex scripts(R"ASSET("scriptName"\s*:\s*"([^"]+)")ASSET");
    for(std::sregex_iterator i(data.begin(),data.end(),textures), end;i!=end;++i) {
        const std::string name=(*i)[1].str();
        if(name.rfind("texture:",0)==0 || name.rfind("spritesheet:",0)==0)
            referenced.insert(name);
    }
    for(std::sregex_iterator i(data.begin(),data.end(),scripts),end;i!=end;++i) {
        const auto name=(*i)[1].str();
        if(name.empty()) continue;
        const auto* script=manifest.findByPath("Scripts/"+name+".lua");
        referenced.insert(script ? script->id : "script:scripts/"+name);
    }
    return {referenced.begin(),referenced.end()};
}
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
    // Build a deterministic dependency index from the project's scene/prefab
    // source and animation clip metadata, not from a runtime-loaded scene.
    const auto snapshot=catalog_.getEntries();
    for(const auto& asset:snapshot) {
        if(auto* mutableEntry=catalog_.findById(asset.id)) {
            mutableEntry->dependencies=discoverDependencies(
                catalog_,asset,root_/asset.path);
        }
    }
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
        if(entry.contentHash.empty() ||
           fingerprint(file)!=entry.contentHash) ++result.changed;
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
