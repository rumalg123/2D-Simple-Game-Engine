#include "AnimationTools.h"
#include "PhysicsSystem.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "SceneEditSession.h"
#include "SceneSerializer.h"
#include "TilemapTools.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void ensure(bool value,const std::string& message) {
    if(!value) throw std::runtime_error(message);
}
void near(float a,float b,const char* message) {
    ensure(std::abs(a-b)<0.0001f,message);
}
TilemapComponent makeMap(int columns,int rows) {
    TilemapComponent map;
    map.columns=columns;map.rows=rows;
    map.tileWidth=1;map.tileHeight=1;
    map.atlasColumns=4;map.atlasRows=4;
    map.tiles.assign(static_cast<std::size_t>(columns)*rows,-1);
    map.collisionEnabled=true;
    return map;
}
}

int main() {
try {
    auto map=makeMap(5,4);
    ensure(TilemapTools::paint(map,2,1,3),"Pencil failed");
    ensure(!TilemapTools::paint(map,2,1,3),"Same tile should not modify map");
    ensure(!TilemapTools::paint(map,-1,0,0),"Out of bounds paint accepted");
    ensure(!TilemapTools::paint(map,0,0,16),"Out of atlas paint accepted");
    ensure(TilemapTools::get(map,2,1)==3,"Paint did not persist");

    ensure(TilemapTools::resize(map,7,6),"Resize failed");
    ensure(map.tiles.size()==42 && TilemapTools::get(map,2,1)==3,
           "Resize did not preserve authored cells");
    ensure(TilemapTools::get(map,6,5)==-1,"Extended cells are not empty");
    ensure(!TilemapTools::resize(map,257,1),"Unbounded resize accepted");

    ensure(TilemapTools::paintLine(map,0,0,4,4,2)==5,
           "Line brush did not paint expected diagonal");
    ensure(TilemapTools::get(map,4,4)==2,"Line end not painted");
    ensure(TilemapTools::checkerboard(map,0,1)>0,"Checkerboard brush failed");
    ensure(TilemapTools::get(map,0,0)==0 && TilemapTools::get(map,1,0)==1,
           "Checkerboard pattern changed unexpectedly");

    auto fill=makeMap(5,5);
    for(int y=0;y<5;++y) TilemapTools::paint(fill,2,y,2);
    ensure(TilemapTools::floodFill(fill,0,0,1)==10,
           "Flood fill crossed barrier or missed connected cells");
    ensure(TilemapTools::get(fill,4,4)==-1 && TilemapTools::get(fill,2,2)==2,
           "Flood fill altered blocked or disconnected tiles");
    ensure(TilemapTools::floodFill(fill,0,0,1)==0,"Redundant fill modified map");
    ensure(TilemapTools::paint(fill,0,0,-1),"Eraser failed");

    const TransformComponent origin{10,20};
    ensure(TilemapTools::worldToCell(origin,map,10.25f,19.5f)==std::make_pair(0,0),
           "World mapping origin incorrect");
    ensure(TilemapTools::worldToCell(origin,map,12.5f,18.5f)==std::make_pair(2,1),
           "World mapping row direction incorrect");
    const ColliderComponent probe{0.4f,0.4f,true};
    auto range=TilemapTools::overlappingCells(origin,map,{12.5f,18.5f},probe);
    ensure(!range.empty() && range.firstColumn<=2 && range.lastColumn>=2 &&
           range.firstRow<=1 && range.lastRow>=1,
           "Nearby collision cell range misses target");
    range=TilemapTools::overlappingCells(origin,map,{100,100},probe);
    ensure(range.empty(),"Far away collision cell selection must be empty");

    SpriteAnimationComponent animation;
    animation.columns=4;animation.rows=2;animation.firstFrame=3;
    animation.frameCount=5;animation.currentFrame=0;
    animation.secondsPerFrame=0.1f;animation.playing=true;animation.loop=true;
    AnimationTools::advance(animation,0.25f);
    ensure(animation.currentFrame==2,"Animation did not advance two frames");
    const auto uv=AnimationTools::uv(animation);
    near(uv.uMin,0.25f,"Incorrect atlas U coordinate");
    near(uv.vMax,0.5f,"Incorrect atlas V coordinate");
    AnimationTools::step(animation,-1);
    ensure(animation.currentFrame==1,"Manual step failed");
    AnimationTools::restart(animation);
    ensure(animation.currentFrame==0 && animation.playing,
           "Animation restart failed");
    animation.currentFrame=4;animation.loop=false;
    AnimationTools::advance(animation,0.2f);
    ensure(animation.currentFrame==4 && !animation.playing,
           "Nonlooping animation did not stop on last frame");
    animation.columns=0;animation.rows=-100;animation.secondsPerFrame=0;
    AnimationTools::normalize(animation);
    ensure(animation.columns==1 && animation.rows==1 &&
           animation.secondsPerFrame>=0.001f,"Invalid animation values not clamped");

    Scene scene;
    const Entity tilemapEntity=scene.createEntity("AuthorMap",{0,0});
    auto solid=makeMap(128,128);
    TilemapTools::paint(solid,90,90,0);
    scene.setTilemap(tilemapEntity,solid);
    PhysicsSystem physics;
    const ColliderComponent collider{0.2f,0.2f,false,true};
    const auto hits=physics.queryAabb(scene,{90.5f,-90.5f},collider);
    ensure(hits.size()==1 && hits[0]==tilemapEntity,
           "Large map query did not find the only occupied tile");
    ensure(physics.queryAabb(scene,{20.5f,-20.5f},collider).empty(),
           "Large map collision query reported empty tiles");

    SceneHistory history;
    history.record(scene);
    TilemapTools::paint(*scene.getTilemap(tilemapEntity),90,90,-1);
    ensure(physics.queryAabb(scene,{90.5f,-90.5f},collider).empty(),
           "Erased tile remained collidable");
    ensure(history.undo(scene),"Undo after tile paint failed");
    ensure(TilemapTools::get(*scene.getTilemap(tilemapEntity),90,90)==0,
           "Undo did not restore painted tile");

    const auto temp=std::filesystem::temp_directory_path() /
        ("simpleengine_tilemap_test_" +
          std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(temp);
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {std::error_code ec;std::filesystem::remove_all(path,ec);}
    } cleanup{temp};
    ResourceManager resources;
    std::string error;
    const auto sceneFile=(temp/"painted.scene.json").string();
    ensure(saveSceneToJson(scene,resources,sceneFile,error),
           "Could not save painted scene: "+error);
    Scene reloaded;
    ensure(loadSceneFromJson(reloaded,resources,sceneFile,error),
           "Could not reload painted scene: "+error);
    const auto* recovered=reloaded.getTilemap(
        reloaded.findEntityByName("AuthorMap"));
    ensure(recovered && TilemapTools::get(*recovered,90,90)==0,
           "Painted tile failed JSON roundtrip");

    std::cout<<"Tile painting, flood fill, animation, collision culling,"
                " history and scene serialization tests passed.\n";
    return 0;
} catch(const std::exception& e) {
    std::cerr<<"Tilemap/animation test failure: "<<e.what()<<'\n';
    return 1;
}
}
