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

/**
 * @brief Diagnostic base layer displayed by the interactive 2D map view.
 */
enum class Map2DBaseLayer {
    kTerrain,
    kElevation,
    kCollision,
};

}  // namespace vox3d
