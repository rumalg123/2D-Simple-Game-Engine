#pragma once

#include "ECS.h"

#include <cstddef>
#include <utility>

namespace TilemapTools {

// Keep editor operations bounded so imported maps cannot allocate unbounded
// memory or freeze an ImGui frame during fill/resize.
constexpr int MaxDimension = 256;
constexpr int EmptyTile = -1;

struct CellRange {
    int firstColumn = 0;
    int lastColumn = -1;
    int firstRow = 0;
    int lastRow = -1;
    bool empty() const { return lastColumn < firstColumn || lastRow < firstRow; }
};

bool inBounds(const TilemapComponent& map, int column, int row);
int get(const TilemapComponent& map, int column, int row);
bool paint(TilemapComponent& map, int column, int row, int atlasIndex);
bool resize(TilemapComponent& map, int columns, int rows, int empty = EmptyTile);
std::size_t floodFill(TilemapComponent& map, int column, int row, int atlasIndex);
std::size_t paintLine(TilemapComponent& map, int x0, int y0,
                      int x1, int y1, int atlasIndex);
std::size_t checkerboard(TilemapComponent& map, int first, int second);

// Map origin is its top-left edge; Y grows upward in world space but rows
// grow downward. Matches PhysicsSystem::tileTransformFor and renderer layout.
std::pair<int, int> worldToCell(const TransformComponent& origin,
                                const TilemapComponent& map, float x, float y);
CellRange overlappingCells(const TransformComponent& origin,
                           const TilemapComponent& map,
                           const TransformComponent& center,
                           const ColliderComponent& collider);
} // namespace TilemapTools
