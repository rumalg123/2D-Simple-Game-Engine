#include "AssetImportPipeline.h"
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
#include <vector>

namespace {
void require(bool test,const std::string& message) {
    if(!test) throw std::runtime_error(message);
}
struct Fixture {
    std::filesystem::path root;
    Fixture() {
        root=std::filesystem::temp_directory_path()/(
            "simpleengine_import_"+std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root/"Assets/Sprites");
        std::filesystem::create_directories(root/"Assets/Scenes");
    }
    ~Fixture() {std::error_code ec;std::filesystem::remove_all(root,ec);}
};
void write(const std::filesystem::path& path,const std::string& data) {
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    out<<data;
    require(bool(out),"Cannot create test file: "+path.string());
}
const ImportedArtifact* find(const ImportBuildReport& result,const std::string& id) {
    for(const auto& entry:result.artifacts)
        if(entry.id==id) return &entry;
    return nullptr;
}
}
int main() {
try {
    Fixture f;
    const auto root=f.root;
    const auto texture=root/"Assets/Sprites/player.ppm";
    write(texture,"P3\n2 2\n255\n255 0 0  0 255 0\n0 0 255  255 255 0\n");
    write(root/"Assets/Scenes/Main.scene.json",R"JSON({
        "entities":[{"id":1,"sprite":{
            "textureName":"texture:sprites/player",
            "texturePath":"Assets/Sprites/player.ppm"}}]
    })JSON");
    std::string error;
    AssetWorkspace workspace(root/"Assets");
    std::size_t added=0;
    require(workspace.open(error) && workspace.sync(added,error),
            "Could not seed asset catalog: "+error);
    require(added==2,"Expected two imported source files");

    const auto first=AssetImportPipeline::build(root);
    require(first.ok() && first.built==2 && first.cached==0,
            "Cold build did not import both source artifacts");
    const auto* image=find(first,"texture:sprites/player");
    const auto* scene=find(first,"scene:scenes/main");
    require(image && scene && std::filesystem::exists(image->file) &&
            image->file.extension()==".seimg",
            "Missing compiled texture/scene artifacts");
    ResourceManager resources;
    const auto resource=resources.loadTextureFromFile("compiled",image->file.string());
    const auto& pixels=resources.getTexture(resource);
    require(pixels.width==2 && pixels.height==2 &&
            pixels.pixels.size()==16 &&
            pixels.pixels[0]==255 && pixels.pixels[1]==0 &&
            pixels.pixels[8]==0 && pixels.pixels[10]==255,
            "RGBA artifact was not decoded correctly by the runtime");
    const std::string sourceGuid=image->guid;
    const std::string textureKey=image->cacheKey;
    const std::string sceneKey=scene->cacheKey;
    const auto second=AssetImportPipeline::build(root);
    require(second.ok() && second.built==0 && second.cached==2,
            "Unchanged assets should hit the content-addressed cache");

    TextureImportSettings settings;
    settings.maxDimension=1;settings.flipY=true;
    require(workspace.setTextureSettings("texture:sprites/player",settings,error),
            "Failed to persist texture import settings");
    const auto transformed=AssetImportPipeline::build(root);
    const auto* compiled=find(transformed,"texture:sprites/player");
    const auto* derivedScene=find(transformed,"scene:scenes/main");
    require(transformed.ok() && transformed.built==2 && compiled &&
            derivedScene && compiled->cacheKey!=textureKey &&
            derivedScene->cacheKey!=sceneKey,
            "Changing importer settings did not rebuild dependent scene");
    ResourceManager transformedResources;
    const auto transformedHandle=transformedResources.loadTextureFromFile(
        "small",compiled->file.string());
    const auto& small=transformedResources.getTexture(transformedHandle);
    require(small.width==1 && small.height==1 &&
            small.pixels[0]==0 && small.pixels[2]==255,
            "Max dimension and vertical flip not applied to compiled texture");

    // Portable exports keep textureName for catalog identity, but the player
    // must honor an explicit compiled texturePath under Assets/Imported.
    std::filesystem::create_directories(root/"Assets/Imported");
    const auto imported=root/"Assets/Imported"/(compiled->guid+".seimg");
    std::filesystem::copy_file(compiled->file,imported,
         std::filesystem::copy_options::overwrite_existing);
    const auto compiledScene=root/"Assets/Scenes/Compiled.scene.json";
    write(compiledScene,
        "{\"entities\":[{\"id\":1,\"name\":{\"name\":\"Compiled Hero\"},"
        "\"sprite\":{\"textureName\":\"texture:sprites/player\","
        "\"texturePath\":\"Assets/Imported/"+compiled->guid+".seimg\"}}]}");
    {
        const auto oldWorkingDir=std::filesystem::current_path();
        struct RestoreDirectory {
            std::filesystem::path original;
            ~RestoreDirectory(){std::error_code ec;
                std::filesystem::current_path(original,ec);}
        } restore{oldWorkingDir};
        std::filesystem::current_path(root);
        Scene loaded;
        ResourceManager loadedResources;
        std::string sceneError;
        require(loadSceneFromJson(loaded,loadedResources,
                compiledScene.string(),sceneError),
                "Compiled scene could not load: "+sceneError);
        const auto* sprite=loaded.getSprite(
            loaded.findEntityByName("Compiled Hero"));
        require(sprite && sprite->texture<loadedResources.textureCount(),
                "Compiled scene sprite was not decoded");
        require(loadedResources.getTexture(sprite->texture).sourcePath.find(
                    "Assets/Imported/")!=std::string::npos,
                "Catalog lookup incorrectly overrode explicit compiled texture");
    }

    // Source edits cascade to dependent scenes even when GUIDs are unchanged.
    write(texture,"P3\n2 2\n255\n50 50 50  0 255 0\n0 0 255  255 255 0\n");
    const auto edited=AssetImportPipeline::build(root);
    require(edited.ok() && edited.built==2 &&
            find(edited,"texture:sprites/player")->guid==sourceGuid &&
            find(edited,"texture:sprites/player")->cacheKey!=compiled->cacheKey &&
            find(edited,"scene:scenes/main")->cacheKey!=derivedScene->cacheKey,
            "Source change did not invalidate texture and dependent scene");

    const auto linux=AssetImportPipeline::build(root,"linux-x64");
    require(linux.ok() && linux.built==2 &&
            linux.artifacts.front().file!=edited.artifacts.front().file,
            "Platform-specific cache keys were not isolated");
    const auto linuxSecond=AssetImportPipeline::build(root,"linux-x64");
    require(linuxSecond.ok() && linuxSecond.cached==2,
            "Second platform build should hit its own cache");
    require(!AssetImportPipeline::build(root,"../escape").ok(),
            "Unsafe platform token accepted");

    require(AssetImportPipeline::clean(root,"windows-x64",error),
            "Cannot clean Windows cache: "+error);
    require(!std::filesystem::exists(
        AssetImportPipeline::cacheRoot(root,"windows-x64")),
        "Cache clean did not delete selected platform artifacts");
    require(std::filesystem::exists(
        AssetImportPipeline::cacheRoot(root,"linux-x64")),
        "Cleaning Windows cache removed Linux artifacts");
    const auto rebuilt=AssetImportPipeline::build(root);
    require(rebuilt.ok() && rebuilt.built==2,
            "Clean build did not recreate output artifacts");

    // Do not accept truncated or oversized images as valid resource files.
    const auto corrupt=root/"corrupt.seimg";
    write(corrupt,"SEIMG001");
    ResourceManager invalid;
    bool rejected=false;
    try {invalid.loadTextureFromFile("bad",corrupt.string());}
    catch(const std::exception&) {rejected=true;}
    require(rejected,"Truncated compiled texture was accepted");

    std::cout<<"Import cache, decoded RGBA, settings, source/dependency/platform invalidation and cleanup passed.\n";
    return 0;
} catch(const std::exception& e) {
    std::cerr<<"Asset importer test failed: "<<e.what()<<'\n';
    return 1;
}
}
