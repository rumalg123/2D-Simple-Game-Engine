#include "AssetWorkspace.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "SceneSerializer.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void check(bool ok,const std::string& error) {if(!ok)throw std::runtime_error(error);}
struct TempDirectory {
    std::filesystem::path root;
    TempDirectory() {
        root=std::filesystem::temp_directory_path()/(
            "se_asset_database_"+std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root/"Assets/Sprites");
        std::filesystem::create_directories(root/"Assets/Scenes");
        std::filesystem::create_directories(root/"Assets/Scripts");
    }
    ~TempDirectory() {
        std::error_code ec;std::filesystem::remove_all(root,ec);
    }
};
void write(const std::filesystem::path& file,const std::string& data) {
    std::ofstream out(file,std::ios::binary);
    check(bool(out),"Cannot create asset fixture: "+file.string());
    out<<data;check(bool(out),"Cannot write asset fixture");
}
}

int main() {
try {
    TempDirectory temp;
    const auto assets=temp.root/"Assets";
    const auto oldTexture=assets/"Sprites/hero.ppm";
    write(oldTexture,"P3\n1 1\n255\n255 0 0\n");
    write(assets/"Scripts/Player.lua","function on_update(dt) end\n");
    write(assets/"Scenes/Main.scene.json",R"JSON({
        "camera":{"x":0,"y":0,"halfWidth":1,"halfHeight":1},
        "entities":[{
            "id":1,"name":{"name":"Hero"},"transform":{"x":0,"y":0},
            "sprite":{"halfWidth":0.2,"halfHeight":0.2,
              "textureName":"texture:sprites/hero","texturePath":"old/wrong/hero.ppm"}
        }]
    })JSON");
    std::string error;
    AssetWorkspace initial(assets);
    check(initial.open(error),"Could not open initial asset workspace: "+error);
    std::size_t added=0;
    check(initial.sync(added,error),"Cannot import asset database: "+error);
    check(added==3,"Expected scene, texture and Lua script assets");
    auto texture=initial.index().findByPath("Sprites/hero.ppm");
    check(texture && texture->type==AssetType::Texture && texture->guid.size()==32 &&
          !texture->contentHash.empty() && texture->sourceBytes>0,
          "Imported texture metadata incomplete");
    const std::string originalGuid=texture->guid;
    const std::string id=texture->id;
    const std::string hash=texture->contentHash;
    check(initial.index().findByGuid(originalGuid)!=nullptr,"Cannot look up GUID");
    check(initial.index().findByPath("Scripts/Player.lua")->type==AssetType::Script,
          "Lua scripts were not indexed");
    check(initial.audit().valid(),"Imported workspace audit failed");

    check(!initial.move(id,"../escape.ppm",error),"Traversal path was allowed");
    check(!initial.move(id,"Sprites/missing/sub/hero.ppm",error),
          "Move to uncreated destination directory succeeded");
    check(!initial.move(id,"Scenes/hero.json",error),
          "Type-changing move was accepted");
    check(initial.move(id,"Sprites/hero-renamed.ppm",error),
          "Identity-preserving asset move failed: "+error);
    check(!std::filesystem::exists(oldTexture) &&
          std::filesystem::exists(assets/"Sprites/hero-renamed.ppm"),
          "Managed file move did not occur");

    AssetWorkspace reopened(assets);
    check(reopened.open(error),"Cannot reopen saved manifest: "+error);
    const auto* moved=reopened.index().findById(id);
    check(moved && moved->guid==originalGuid &&
          moved->contentHash==hash &&
          moved->path=="Sprites/hero-renamed.ppm",
          "Managed rename changed asset GUID, ID or source fingerprint");
    check(reopened.index().findByGuid(originalGuid)==moved,
          "GUID lookup failed after process restart");

    ResourceManager resources;
    Scene scene;
    check(loadSceneFromJson(scene,resources,
              (assets/"Scenes/Main.scene.json").string(),error),
          "Could not load scene using stable asset ID: "+error);
    const auto hero=scene.findEntityByName("Hero");
    check(hero!=InvalidEntity && scene.getSprite(hero),
          "Could not load sprite referring to renamed asset");
    const auto* sprite=scene.getSprite(hero);
    check(sprite->texture<resources.textureCount() &&
          resources.getTexture(sprite->texture).fileBacked &&
          resources.getTexture(sprite->texture).sourcePath.find("hero-renamed.ppm")!=
                std::string::npos,
          "Scene texture did not follow stable asset ID after relocation");

    write(assets/"Sprites/hero-renamed.ppm","P3\n1 1\n255\n0 255 0\n");
    check(reopened.sync(added,error),"Could not rescan modified asset: "+error);
    check(added==0,"A content change created a new asset ID");
    const auto* changed=reopened.index().findById(id);
    check(changed && changed->guid==originalGuid && changed->contentHash!=hash,
          "Source content fingerprint was not updated independently of GUID");

    // Missing and duplicate GUID metadata must fail clearly.
    std::filesystem::remove(assets/"Sprites/hero-renamed.ppm");
    check(!reopened.audit().valid(),"Missing indexed source did not fail audit");

    std::cout<<"Asset GUID, fingerprints, managed move, scene references, Lua scan and audit passed.\n";
    return 0;
} catch(const std::exception& exception) {
    std::cerr<<"Asset database regression failed: "<<exception.what()<<'\n';
    return 1;
}
}
