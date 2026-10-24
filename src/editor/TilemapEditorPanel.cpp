#include "Engine.h"
#include "TilemapTools.h"
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <string>

void Engine::renderTilemapEditorPanel() {
    ImGui::Begin("Tilemap Painter");
    if (!scene.isValidEntity(selectedEntity) || !scene.getTilemap(selectedEntity)) {
        tileStrokeActive = false;
        ImGui::TextDisabled("Select an entity with a Tilemap component.");
        ImGui::End();
        return;
    }
    TilemapComponent& map = *scene.getTilemap(selectedEntity);
    if (tileResizeSelection != selectedEntity) {
        tileResizeSelection = selectedEntity;
        tileResizeWidth = std::clamp(map.columns,1,TilemapTools::MaxDimension);
        tileResizeHeight = std::clamp(map.rows,1,TilemapTools::MaxDimension);
        tileStrokeActive = false;
    }
    const bool editable = !playSession.active();
    if (!editable) ImGui::TextDisabled("Stop Play Mode to paint the authored scene.");

    ImGui::Text("Canvas: %d x %d, atlas: %d x %d",
                map.columns, map.rows, map.atlasColumns, map.atlasRows);
    ImGui::SeparatorText("4-neighbor Terrain Autotiling");
    if(!map.terrainEnabled) {
        static int terrainBase=0;
        ImGui::InputInt("Terrain Atlas Base",&terrainBase);
        ImGui::TextDisabled("Requires 16 consecutive atlas tiles in N/E/S/W mask order.");
        if(ImGui::Button("Prepare 4x4 Atlas") && editable) {
            recordSceneEdit();
            map.atlasColumns=std::max(4,map.atlasColumns);
            map.atlasRows=std::max(4,map.atlasRows);
        }
        ImGui::SameLine();
        if(ImGui::Button("Enable Terrain") && editable) {
            // Attempt on a temporary copy so invalid settings do not alter the scene.
            TilemapComponent candidate=map;
            if(TilemapTools::enableTerrain(candidate,terrainBase)) {
                recordSceneEdit();
                map=std::move(candidate);
                tileTool=5;
            } else editorStatus="Terrain needs a 16-tile atlas and a map at most 256x256.";
        }
    } else {
        ImGui::Text("Terrain active, base atlas tile %d",map.terrainBaseTile);
        if(ImGui::Button("Bake Terrain to Static Tiles") && editable) {
            recordSceneEdit();
            TilemapTools::disableTerrain(map);
            tileTool=0;
        }
    }

    int width = std::clamp(map.columns,1,TilemapTools::MaxDimension);
    int height = std::clamp(map.rows,1,TilemapTools::MaxDimension);
    ImGui::SetNextItemWidth(90);
    ImGui::InputInt("Width", &tileResizeWidth, 1, 8);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90);
    ImGui::InputInt("Height", &tileResizeHeight, 1, 8);
    tileResizeWidth = std::clamp(tileResizeWidth,1,TilemapTools::MaxDimension);
    tileResizeHeight = std::clamp(tileResizeHeight,1,TilemapTools::MaxDimension);
    if (ImGui::Button("Resize (Preserve)") && editable) {
        if (tileResizeWidth != width || tileResizeHeight != height) {
            recordSceneEdit();
            TilemapTools::resize(map, tileResizeWidth, tileResizeHeight);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear to Empty") && editable) {
        recordSceneEdit();
        TilemapTools::resize(map, width, height);
        if(map.terrainEnabled) {
            std::fill(map.terrainCells.begin(),map.terrainCells.end(),0);
            TilemapTools::rebuildTerrain(map);
        } else std::fill(map.tiles.begin(),map.tiles.end(),TilemapTools::EmptyTile);
    }
    ImGui::SameLine();
    if (ImGui::Button("Checkerboard") && editable && !map.terrainEnabled) {
        recordSceneEdit();
        TilemapTools::checkerboard(map,0,
            std::min(1,std::clamp(map.atlasColumns,1,16)*std::clamp(map.atlasRows,1,16)-1));
    }

    const char* tools[] = {"Pencil", "Eraser", "Flood Fill", "Line", "Eyedropper",
                            "Terrain Paint", "Terrain Erase"};
    ImGui::Combo("Tool", &tileTool, tools, 7);
    if(map.terrainEnabled && tileTool<5) tileTool=5;
    if(!map.terrainEnabled && tileTool>=5) tileTool=0;
    const int atlasCount = std::clamp(map.atlasColumns,1,16) *
                           std::clamp(map.atlasRows,1,16);
    tileBrush = std::clamp(tileBrush,0,atlasCount-1);
    ImGui::SliderInt("Atlas Brush", &tileBrush,0,atlasCount-1);
    ImGui::SliderFloat("Canvas Zoom", &tileCanvasZoom,10.0f,40.0f,"%.0f px");
    ImGui::TextDisabled("Paint with left mouse. Line: drag from start to end.");
    ImGui::TextDisabled("Eraser writes -1. Fill changes a connected region.");
    if (ImGui::BeginChild("TileCanvasScroll",ImVec2(0,390),true,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        const float size=tileCanvasZoom;
        const ImVec2 origin=ImGui::GetCursorScreenPos();
        const ImVec2 canvasSize(std::max(1,map.columns)*size,
                                std::max(1,map.rows)*size);
        ImGui::InvisibleButton("TileCanvas",canvasSize);
        const bool hovered=ImGui::IsItemHovered();
        ImDrawList* draw=ImGui::GetWindowDrawList();
        const ImVec2 mouse=ImGui::GetIO().MousePos;
        const int column=static_cast<int>(std::floor((mouse.x-origin.x)/size));
        const int row=static_cast<int>(std::floor((mouse.y-origin.y)/size));
        const bool inside=hovered && TilemapTools::inBounds(map,column,row);

        const ImVec2 clipMin=draw->GetClipRectMin(),clipMax=draw->GetClipRectMax();
        const int firstX=std::max(0,static_cast<int>(std::floor((clipMin.x-origin.x)/size)));
        const int firstY=std::max(0,static_cast<int>(std::floor((clipMin.y-origin.y)/size)));
        const int lastX=std::min(map.columns-1,static_cast<int>((clipMax.x-origin.x)/size));
        const int lastY=std::min(map.rows-1,static_cast<int>((clipMax.y-origin.y)/size));
        for(int y=firstY;y<=lastY;++y) for(int x=firstX;x<=lastX;++x) {
            const int tile=TilemapTools::get(map,x,y);
            const ImVec2 a(origin.x+x*size,origin.y+y*size);
            const ImVec2 b(a.x+size,a.y+size);
            ImU32 color=IM_COL32(42,45,53,255);
            if(tile>=0) {
                const float hue=static_cast<float>((tile*43+17)%360)/360.0f;
                color=ImColor::HSV(hue,0.62f,0.74f);
            }
            draw->AddRectFilled(a,b,color);
            draw->AddRect(a,b,IM_COL32(18,20,26,255));
            if(tile>=0 && size>=19)
                draw->AddText(ImVec2(a.x+3,a.y+3),IM_COL32(255,255,255,255),
                              std::to_string(tile).c_str());
        }
        if(inside) {
            const ImVec2 a(origin.x+column*size,origin.y+row*size);
            draw->AddRect(a,ImVec2(a.x+size,a.y+size),
                          IM_COL32(255,244,108,255),0,0,2.0f);
        }

        const bool down=ImGui::IsMouseDown(ImGuiMouseButton_Left);
        if (editable && inside && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            tileStrokeActive=true;
            tileStrokeStartX=column;
            tileStrokeStartY=row;
            if(tileTool==4) {
                tileBrush=std::max(0,TilemapTools::get(map,column,row));
                tileStrokeActive=false;
            } else {
                recordSceneEdit(); // exactly one history checkpoint per gesture
                if(tileTool==2) {
                    TilemapTools::floodFill(map,column,row,tileBrush);
                    tileStrokeActive=false;
                } else if(tileTool==0||tileTool==1) {
                    TilemapTools::paint(map,column,row,
                        tileTool==1 ? TilemapTools::EmptyTile : tileBrush);
                } else if(tileTool==5||tileTool==6) {
                    TilemapTools::paintTerrain(map,column,row,tileTool==5);
                }
            }
        }
        if(editable && down && inside && tileStrokeActive &&
           (tileTool==0||tileTool==1||tileTool==5||tileTool==6)) {
            if(tileTool>=5)
                TilemapTools::paintTerrain(map,column,row,tileTool==5);
            else TilemapTools::paint(map,column,row,
                tileTool==1 ? TilemapTools::EmptyTile : tileBrush);
        }
        if(tileStrokeActive && tileTool==3 && down) {
            const ImVec2 start(origin.x+(tileStrokeStartX+0.5f)*size,
                               origin.y+(tileStrokeStartY+0.5f)*size);
            draw->AddLine(start,mouse,IM_COL32(255,224,85,255),2.0f);
        }
        if(tileStrokeActive && !down) {
            if(editable && inside && tileTool==3)
                TilemapTools::paintLine(map,tileStrokeStartX,tileStrokeStartY,
                                        column,row,tileBrush);
            tileStrokeActive=false;
        }
        if(!editable) tileStrokeActive=false;
    }
    ImGui::EndChild();
    if(tileTool == 4) ImGui::TextDisabled("Selected tile index: %d",tileBrush);
    ImGui::End();
}
