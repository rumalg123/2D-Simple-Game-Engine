#include "AnimationTools.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "SceneSerializer.h"
#include "TilemapTools.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void check(bool pass, const std::string& message) {
    if (!pass) throw std::runtime_error(message);
}
struct TempDir {
    std::filesystem::path path;
    TempDir() {
        path=std::filesystem::temp_directory_path() /
            ("simpleengine_terrain_clip_" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(path);
    }
    ~TempDir() { std::error_code ec;std::filesystem::remove_all(path,ec); }
};
TilemapComponent tilemap(int columns,int rows) {
    TilemapComponent map;
    map.columns=columns;map.rows=rows;
    map.atlasColumns=4;map.atlasRows=4;
    map.tiles.assign(static_cast<std::size_t>(columns)*rows,-1);
    return map;
}
}

int main() {
    try {
        auto map=tilemap(3,3);
        check(TilemapTools::enableTerrain(map,0),"Valid terrain atlas was rejected");
        check(TilemapTools::paintTerrain(map,1,1,true),"Terrain center paint failed");
        check(TilemapTools::get(map,1,1)==0,"Single terrain cell has neighbors");
        check(TilemapTools::paintTerrain(map,2,1,true),"Adjacent terrain paint failed");
        check(TilemapTools::get(map,1,1)==2 &&
              TilemapTools::get(map,2,1)==8,
              "East and west autotile mask update incorrect");
        check(TilemapTools::paintTerrain(map,1,0,true),"North terrain paint failed");
        check(TilemapTools::get(map,1,1)==3 &&
              TilemapTools::get(map,1,0)==4,
              "North/south terrain mask incorrect");
        check(!TilemapTools::paintTerrain(map,1,0,true),
              "Redundant terrain paint should not mutate the map");
        check(TilemapTools::paintTerrain(map,2,1,false),"Terrain eraser failed");
        check(TilemapTools::get(map,1,1)==1 &&
              TilemapTools::get(map,2,1)==-1,
              "Terrain eraser did not rebuild neighboring atlas indices");
        check(TilemapTools::resize(map,5,4),"Terrain map resize rejected");
        check(TilemapTools::terrainAt(map,1,1) &&
              TilemapTools::terrainAt(map,1,0) &&
              !TilemapTools::terrainAt(map,4,3),
              "Terrain occupancy was not preserved during resize");
        check(TilemapTools::get(map,1,1)==1,"Autotile was not rebuilt after resize");
        check(!TilemapTools::enableTerrain(map,15),
              "Terrain base allowing out-of-atlas variants was accepted");
        TilemapTools::disableTerrain(map);
        check(!map.terrainEnabled && map.terrainCells.empty() &&
              TilemapTools::get(map,1,1)==1,"Baking terrain changed rendered art");

        auto insufficient=tilemap(1,1);
        insufficient.atlasColumns=2;insufficient.atlasRows=2;
        check(!TilemapTools::enableTerrain(insufficient,0),
              "Terrain enabled without sixteen atlas tiles");

        // Two independently rendered layers: the existing entity+draw-layer
        // structure preserves multiple maps without changing the renderer ABI.
        Scene scene;
        const auto lower=scene.createEntity("Background Terrain",{0,0});
        const auto upper=scene.createEntity("Foreground Terrain",{0,0});
        auto bottom=tilemap(3,3);
        auto top=tilemap(3,3);
        check(TilemapTools::enableTerrain(bottom,0),"Terrain enable rejected");
        TilemapTools::paintTerrain(bottom,1,1,true);
        bottom.layer=-30;top.layer=-10;
        TilemapTools::paint(top,2,2,5);
        scene.setTilemap(lower,bottom);
        scene.setTilemap(upper,top);
        const std::string lowerGuid=scene.guid(lower),upperGuid=scene.guid(upper);
        check(lowerGuid!=upperGuid,"Map layers share a persistent GUID");

        SpriteAnimationComponent animation;
        animation.columns=4;animation.rows=2;
        check(AnimationTools::addClip(animation,{"Idle",0,2,0.12f,true}),
              "Could not create Idle clip");
        check(AnimationTools::addClip(animation,{"Run",2,4,0.05f,true}),
              "Could not create Run clip");
        check(!AnimationTools::addClip(animation,{"Run",2,4,0.05f,true}),
              "Duplicate clip name accepted");
        check(!AnimationTools::addClip(animation,{"OutOfBounds",7,2,0.10f,true}),
              "Clip exceeding sprite sheet accepted");
        check(AnimationTools::selectClip(animation,"Run"),"Cannot switch to Run clip");
        check(animation.activeClip=="Run" && animation.firstFrame==2 &&
              animation.frameCount==4 && animation.playing,
              "Clip selection did not update playback range");
        AnimationTools::advance(animation,0.11f);
        check(animation.currentFrame==2,"Run clip did not advance two frames");
        check(AnimationTools::selectClip(animation,"Idle"),
              "Cannot switch back to Idle");
        check(animation.currentFrame==0 && animation.firstFrame==0,
              "Clip switch did not restart the selected range");
        check(AnimationTools::updateClip(animation,{"Idle",0,3,0.1f,false}),
              "Could not update named clip");
        check(animation.frameCount==3 && !animation.loop,
              "Active clip property update did not take effect");
        check(!AnimationTools::selectClip(animation,"DoesNotExist"),
              "Missing clip should return false");
        check(AnimationTools::removeClip(animation,"Idle"),
              "Cannot delete active named clip");
        check(animation.activeClip.empty(),"Deleted clip left stale state");

        check(AnimationTools::selectClip(animation,"Run"),"Run disappeared");
        scene.setSpriteAnimation(upper,animation);

        TempDir temporary;
        ResourceManager resources;
        std::string error;
        const std::string path=(temporary.path/"layers.scene.json").string();
        check(saveSceneToJson(scene,resources,path,error),
              "Layered terrain scene save failed: "+error);
        Scene loaded;
        check(loadSceneFromJson(loaded,resources,path,error),
              "Layered terrain scene load failed: "+error);
        const auto loadedLower=loaded.findEntityByGuid(lowerGuid);
        const auto loadedUpper=loaded.findEntityByGuid(upperGuid);
        check(loadedLower!=InvalidEntity && loadedUpper!=InvalidEntity,
              "Layer entities lost stable identity");
        const auto* map1=loaded.getTilemap(loadedLower);
        const auto* map2=loaded.getTilemap(loadedUpper);
        check(map1 && map1->terrainEnabled && map1->layer==-30 &&
              TilemapTools::terrainAt(*map1,1,1) &&
              TilemapTools::get(*map1,1,1)==0,
              "Terrain occupancy/variants failed JSON roundtrip");
        check(map2 && !map2->terrainEnabled && map2->layer==-10 &&
              TilemapTools::get(*map2,2,2)==5,
              "Ordinary tilemap layer failed JSON roundtrip");
        const auto* savedClip=loaded.getSpriteAnimation(loadedUpper);
        check(savedClip && savedClip->activeClip=="Run" &&
              savedClip->clips.size()==1 && savedClip->clips[0].name=="Run",
              "Named animation clips failed scene roundtrip");

        // Backwards compatibility for simple old scenes.
        const std::string oldPath=(temporary.path/"legacy.scene.json").string();
        {
            std::ofstream output(oldPath);
            output << R"({"entities":[{"id":0,"name":{"name":"Legacy"},
                "tilemap":{"columns":1,"rows":1,"tiles":[7]},
                "spriteAnimation":{"columns":2,"rows":1,"firstFrame":0,"frameCount":2}}]})";
        }
        Scene legacy;
        check(loadSceneFromJson(legacy,resources,oldPath,error),
              "Old pre-terrain scene cannot load: "+error);
        const auto old=legacy.findEntityByName("Legacy");
        check(old!=InvalidEntity && legacy.getTilemap(old) &&
              !legacy.getTilemap(old)->terrainEnabled &&
              TilemapTools::get(*legacy.getTilemap(old),0,0)==7 &&
              legacy.getSpriteAnimation(old)->clips.empty(),
              "Legacy scene behavior changed");

        std::cout<<"Terrain adjacency, tile layers, named clips, JSON and legacy scenes pass.\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"Terrain/clip tests failed: "<<error.what()<<'\n';
        return 1;
    }
}
