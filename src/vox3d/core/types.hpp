#pragma once

#include <cstdint>

namespace vox3d {

/**
 * @brief Tile-space position inside a runtime map.
 */
struct TileCoord {
    int x = 0;
    int y = 0;
};

/**
 * @brief Inclusive integer elevation range used by voxel map data.
 */
struct LevelRange {
    int min = 0;
    int max = 0;
};

}  // namespace vox3d
