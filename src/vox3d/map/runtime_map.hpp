#pragma once

#include "vox3d/core/result.hpp"
#include "vox3d/core/types.hpp"
#include "vox3d/map/map_package.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace vox3d {

/**
 * @brief Dense tile grid owned by a runtime map.
 *
 * The grid stores one value per map tile in row-major order. Coordinates are
 * expressed in tile space, where x grows to the right and y grows downward.
 *
 * @tparam T Cell value type stored by the grid.
 */
template <typename T>
struct RuntimeGrid {
    int width = 0;
    int height = 0;
    std::vector<T> cells;

    /**
     * @brief Returns true when dimensions and cell count match.
     *
     * @return True if the grid contains exactly width * height cells.
     */
    [[nodiscard]] bool IsValid() const
    {
        return width > 0 && height > 0
            && cells.size() == static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    }

    /**
     * @brief Returns the expected number of cells for the current dimensions.
     *
     * @return Expected row-major cell count.
     */
    [[nodiscard]] std::size_t ExpectedCellCount() const
    {
        if (width <= 0 || height <= 0) {
            return 0;
        }
        return static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    }

    /**
     * @brief Checks whether a tile coordinate is inside the grid bounds.
     *
     * @param coord Tile coordinate to test.
     * @return True if the coordinate is inside the grid.
     */
    [[nodiscard]] bool Contains(TileCoord coord) const
    {
        return coord.x >= 0 && coord.y >= 0 && coord.x < width && coord.y < height;
    }
};

/**
 * @brief Normalized runtime map summary used by future voxel/chunk builders.
 */

/**
 * @brief Compact object-marker class used by the 3D diagnostic overlay.
 */
enum class RuntimeObjectMarkerKind : std::uint8_t {
    kUnknown,
    kTree,
    kBush,
    kReed,
    kRuin,
    kCover,
    kLoot,
    kStructure,
    kTrench,
};

/**
 * @brief Lightweight map object marker extracted from runtime objects or vegetation visuals.
 */
struct RuntimeObjectMarker {
    TileCoord tile;
    RuntimeObjectMarkerKind kind = RuntimeObjectMarkerKind::kUnknown;
    std::string type;
    std::string role;
    int height = 1;
    bool blocks_movement = false;
    bool visual_only = false;
};

struct RuntimeMapInfo {
    int width = 0;
    int height = 0;
    int tile_size_px = 0;
    std::optional<LevelRange> levels;
    std::optional<TileCoord> start;
    std::optional<TileCoord> goal;
    std::string generator_version;
    std::string schema_version;
    bool terrain_loaded = false;
    bool elevation_loaded = false;
    bool collision_loaded = false;
    bool start_goal_loaded = false;
    bool object_markers_loaded = false;
    bool runtime_binary_checked = false;
    bool runtime_binary_valid = false;
    bool runtime_binary_loaded = false;
    std::string runtime_binary_fallback_reason;
    bool runtime_binary_json_compare_checked = false;
    bool runtime_binary_json_compare_ok = false;
    std::string runtime_binary_json_compare_reason;
    std::size_t runtime_binary_json_terrain_mismatches = 0;
    std::size_t runtime_binary_json_collision_mismatches = 0;
    std::size_t runtime_binary_json_height_mismatches = 0;
    std::size_t runtime_binary_json_point_mismatches = 0;
    int runtime_binary_json_load_ms = 0;
    int runtime_binary_json_compare_ms = 0;
    int blocked_cells = 0;
    int object_markers = 0;

    /**
     * @brief Returns true when the runtime map dimensions are usable.
     *
     * @return True if width, height, and tile size are positive.
     */
    [[nodiscard]] bool IsValid() const;
};

/**
 * @brief Runtime map data produced from a loaded map package.
 *
 * This object is the editor-independent data layer used by upcoming voxel,
 * chunk, and mesh builders. It owns dense terrain, collision, and height grids
 * parsed from the package files.
 */
struct RuntimeMap {
    RuntimeMapInfo info;
    MapOverview overview;
    RuntimeGrid<std::string> terrain;
    RuntimeGrid<std::uint8_t> collision;
    RuntimeGrid<int> height;
    std::vector<RuntimeObjectMarker> object_markers;
    Diagnostics diagnostics;

    /**
     * @brief Returns true when the runtime map contains usable dimensions.
     *
     * @return True if the runtime map info is valid.
     */
    [[nodiscard]] bool IsValid() const;

    /**
     * @brief Returns true when all core runtime grids were loaded.
     *
     * @return True if terrain, collision, and height grids are valid.
     */
    [[nodiscard]] bool HasCoreGrids() const;
};

/**
 * @brief Builds normalized runtime-map data from package information.
 *
 * The builder reads the real TopDownMapGen map package layout, validates dense
 * grid dimensions, and keeps diagnostics in the returned object. It does not
 * build voxels, chunks, meshes, or rendering resources.
 *
 * @param package Loaded map package information.
 * @return Runtime map data suitable for future engine layers.
 */
[[nodiscard]] RuntimeMap BuildRuntimeMap(const MapPackageInfo& package);

/**
 * @brief Builds a compact stable log string for runtime map diagnostics.
 *
 * @param map Runtime map data.
 * @return Compact human-readable summary.
 */
[[nodiscard]] std::string ToLogString(const RuntimeMap& map);

}  // namespace vox3d
