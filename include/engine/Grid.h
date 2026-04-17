#pragma once

#include "ECS.h"

#include <algorithm>
#include <cstddef>

struct GridCell {
    int x = 0;
    int y = 0;
};

struct GridLayout {
    int columns = 1;
    int rows = 1;
    float cellWidth = 1.0f;
    float cellHeight = 1.0f;
    TransformComponent center{0.0f, 0.0f};
    bool yDown = true;
};

inline bool operator==(GridCell first, GridCell second) {
    return first.x == second.x && first.y == second.y;
}

inline bool operator!=(GridCell first, GridCell second) {
    return !(first == second);
}

inline bool gridContains(GridCell cell, int columns, int rows) {
    return cell.x >= 0 && cell.x < columns && cell.y >= 0 && cell.y < rows;
}

inline bool gridContains(const GridLayout& layout, GridCell cell) {
    return gridContains(cell, layout.columns, layout.rows);
}

inline int gridCellCount(const GridLayout& layout) {
    return std::max(0, layout.columns) * std::max(0, layout.rows);
}

inline std::size_t gridToIndex(GridCell cell, int columns) {
    return static_cast<std::size_t>(cell.y * columns + cell.x);
}

inline GridCell gridFromIndex(int index, int columns) {
    const int safeColumns = std::max(1, columns);
    return {index % safeColumns, index / safeColumns};
}

inline TransformComponent gridToWorld(const GridLayout& layout, GridCell cell) {
    const float columns = static_cast<float>(std::max(1, layout.columns));
    const float rows = static_cast<float>(std::max(1, layout.rows));
    const float x = layout.center.x + (static_cast<float>(cell.x) - columns * 0.5f + 0.5f) * layout.cellWidth;

    const float yOffset = layout.yDown ?
        (rows * 0.5f - static_cast<float>(cell.y) - 0.5f) * layout.cellHeight :
        (static_cast<float>(cell.y) - rows * 0.5f + 0.5f) * layout.cellHeight;

    return {x, layout.center.y + yOffset};
}
