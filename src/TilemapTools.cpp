#include "TilemapTools.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <vector>

namespace TilemapTools {
namespace {
std::size_t indexOf(const TilemapComponent& map, int column, int row) {
    return static_cast<std::size_t>(row) * static_cast<std::size_t>(map.columns)
         + static_cast<std::size_t>(column);
}
bool hasValidSize(const TilemapComponent& map) {
    // Runtime collision queries must support legacy maps larger than the
    // visual painter's bounded creation/editing limits.
    return map.columns > 0 && map.rows > 0;
}
int boundedFloor(double value, int maximum) {
    if (!std::isfinite(value) || value < 0.0) return -1;
    if (value >= maximum) return maximum;
    return static_cast<int>(std::floor(value));
}
}

bool inBounds(const TilemapComponent& map, int column, int row) {
    return hasValidSize(map) && column >= 0 && row >= 0 &&
           column < map.columns && row < map.rows;
}

int get(const TilemapComponent& map, int column, int row) {
    if (!inBounds(map, column, row)) return EmptyTile;
    const std::size_t index = indexOf(map, column, row);
    return index < map.tiles.size() ? map.tiles[index] : EmptyTile;
}

bool paint(TilemapComponent& map, int column, int row, int atlasIndex) {
    if (!inBounds(map, column, row) ||
        map.columns > MaxDimension || map.rows > MaxDimension) return false;
    const int atlasCount = std::clamp(map.atlasColumns,1,16) *
                           std::clamp(map.atlasRows,1,16);
    if (atlasIndex < EmptyTile || atlasIndex >= atlasCount) return false;
    const std::size_t index = indexOf(map, column, row);
    const std::size_t size =
        static_cast<std::size_t>(map.columns) * static_cast<std::size_t>(map.rows);
    if (map.tiles.size() < size) map.tiles.resize(size, EmptyTile);
    if (map.tiles[index] == atlasIndex) return false;
    map.tiles[index] = atlasIndex;
    return true;
}

bool resize(TilemapComponent& map, int columns, int rows, int empty) {
    if (columns < 1 || rows < 1 || columns > MaxDimension ||
        rows > MaxDimension) return false;
    std::vector<int> next(static_cast<std::size_t>(columns) * rows, empty);
    const int oldColumns = std::max(0, map.columns);
    const int oldRows = std::max(0, map.rows);
    for (int y = 0; y < std::min(rows, oldRows); ++y) {
        for (int x = 0; x < std::min(columns, oldColumns); ++x) {
            const std::size_t oldIndex = static_cast<std::size_t>(y) * oldColumns + x;
            if (oldIndex < map.tiles.size())
                next[static_cast<std::size_t>(y) * columns + x] = map.tiles[oldIndex];
        }
    }
    map.columns = columns;
    map.rows = rows;
    map.tiles = std::move(next);
    return true;
}

std::size_t floodFill(TilemapComponent& map, int column, int row, int atlasIndex) {
    if (!inBounds(map, column, row) ||
        map.columns > MaxDimension || map.rows > MaxDimension) return 0;
    const int source = get(map, column, row);
    if (source == atlasIndex || atlasIndex < EmptyTile ||
        atlasIndex >= std::clamp(map.atlasColumns,1,16) *
                      std::clamp(map.atlasRows,1,16))
        return 0;
    std::deque<std::pair<int, int>> queue;
    queue.emplace_back(column, row);
    std::size_t changed = 0;
    while (!queue.empty()) {
        const auto cell = queue.front();
        queue.pop_front();
        if (get(map, cell.first, cell.second) != source) continue;
        if (!paint(map, cell.first, cell.second, atlasIndex)) continue;
        ++changed;
        for (const auto delta : {std::pair<int,int>{1,0}, {-1,0}, {0,1}, {0,-1}}) {
            const int x = cell.first + delta.first;
            const int y = cell.second + delta.second;
            if (inBounds(map,x,y) && get(map,x,y) == source)
                queue.emplace_back(x,y);
        }
    }
    return changed;
}

std::size_t paintLine(TilemapComponent& map, int x0, int y0, int x1, int y1,
                      int atlasIndex) {
    if (!inBounds(map, x0,y0) || !inBounds(map,x1,y1)) return 0;
    int dx = std::abs(x1-x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1-y0), sy = y0 < y1 ? 1 : -1;
    int err = dx+dy;
    std::size_t changed = 0;
    while (true) {
        if (paint(map,x0,y0,atlasIndex)) ++changed;
        if (x0==x1 && y0==y1) break;
        const int doubled = 2*err;
        if (doubled >= dy) { err+=dy; x0+=sx; }
        if (doubled <= dx) { err+=dx; y0+=sy; }
    }
    return changed;
}

std::size_t checkerboard(TilemapComponent& map, int first, int second) {
    if (!hasValidSize(map) ||
        map.columns > MaxDimension || map.rows > MaxDimension) return 0;
    std::size_t changed=0;
    for(int y=0;y<map.rows;++y)
        for(int x=0;x<map.columns;++x)
            if(paint(map,x,y,(x+y)%2==0 ? first : second)) ++changed;
    return changed;
}

std::pair<int,int> worldToCell(const TransformComponent& origin,
                                const TilemapComponent& map, float x, float y) {
    if (map.tileWidth <= 0 || map.tileHeight <= 0)
        return {-1,-1};
    return {static_cast<int>(std::floor((x-origin.x)/map.tileWidth)),
            static_cast<int>(std::floor((origin.y-y)/map.tileHeight))};
}

CellRange overlappingCells(const TransformComponent& origin,
                           const TilemapComponent& map,
                           const TransformComponent& center,
                           const ColliderComponent& collider) {
    CellRange result;
    if(!hasValidSize(map) || map.tileWidth<=0 || map.tileHeight<=0) return result;
    const double left = (center.x-collider.halfWidth-origin.x)/map.tileWidth;
    const double right = (center.x+collider.halfWidth-origin.x)/map.tileWidth;
    const double top = (origin.y-center.y-collider.halfHeight)/map.tileHeight;
    const double bottom = (origin.y-center.y+collider.halfHeight)/map.tileHeight;
    result.firstColumn = std::max(0,boundedFloor(left,map.columns));
    result.lastColumn = std::min(map.columns-1,boundedFloor(right,map.columns));
    result.firstRow = std::max(0,boundedFloor(top,map.rows));
    result.lastRow = std::min(map.rows-1,boundedFloor(bottom,map.rows));
    return result;
}
} // namespace TilemapTools
