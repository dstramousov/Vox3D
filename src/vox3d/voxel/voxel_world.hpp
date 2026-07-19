#pragma once

#include "vox3d/chunk/chunk_grid.hpp"
#include "vox3d/core/result.hpp"
#include "vox3d/core/types.hpp"
#include "vox3d/map/runtime_map.hpp"
#include "vox3d/voxel/block.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace vox3d {

/**
 * @brief Compact implicit voxel column built from one runtime-map tile.
 *
 * The column does not store every block explicitly. Solid occupancy is implied
 * by the base level and surface level. This keeps the foundation cheap while
 * still providing enough information for upcoming face-culling and mesh code.
 */
struct VoxelColumn {
    TileCoord tile;
    int base_level = 0;
    int surface_level = 0;
    bool blocked = false;
    BlockTypeId surface_block_type = BlockTypeId::kTerrainSurface;

    /**
     * @brief Returns the number of solid blocks in the column.
     *
     * @return Count of occupied voxel levels from base_level to surface_level.
     */
    [[nodiscard]] std::uint64_t SolidBlockCount() const;

    /**
     * @brief Checks whether the column occupies the provided elevation level.
     *
     * @param level Elevation level to query.
     * @return True when the level is solid inside this column.
     */
    [[nodiscard]] bool ContainsSolidLevel(int level) const;
};

/**
 * @brief Summary of a voxel world built from runtime map and chunk data.
 */
struct VoxelWorldInfo {
    int map_width = 0;
    int map_height = 0;
    int chunk_size_x = 0;
    int chunk_size_y = 0;
    int chunks_x = 0;
    int chunks_y = 0;
    int total_chunks = 0;
    std::optional<LevelRange> levels;
    std::uint64_t total_columns = 0;
    std::uint64_t solid_blocks = 0;
    std::uint64_t empty_blocks = 0;
    int blocked_columns = 0;

    /**
     * @brief Returns true when the voxel-world dimensions and counts are usable.
     *
     * @return True if dimensions, level range, and column count are consistent.
     */
    [[nodiscard]] bool IsValid() const;
};

/**
 * @brief Editor-independent voxel-world foundation.
 *
 * The world stores compact block columns, lookup helpers, and statistics. It
 * intentionally does not own chunk meshes, renderer resources, or destruction
 * deltas yet.
 */
struct VoxelWorld {
    VoxelWorldInfo info;
    std::vector<VoxelColumn> columns;
    Diagnostics diagnostics;

    /**
     * @brief Returns true when the world can be consumed by later voxel layers.
     *
     * @return True if info and column storage are consistent.
     */
    [[nodiscard]] bool IsValid() const;

    /**
     * @brief Checks whether a tile coordinate is inside the voxel world.
     *
     * @param coord Tile coordinate to test.
     * @return True when coord is inside map bounds.
     */
    [[nodiscard]] bool ContainsTile(TileCoord coord) const;

    /**
     * @brief Returns a voxel column by tile coordinate.
     *
     * @param coord Tile coordinate to look up.
     * @return Pointer to the column, or nullptr when coord is outside bounds.
     */
    [[nodiscard]] const VoxelColumn* FindColumn(TileCoord coord) const;

    /**
     * @brief Returns the implicit voxel block at a map-space coordinate.
     *
     * @param coord Block coordinate to query.
     * @return Solid or empty block description for the coordinate.
     */
    [[nodiscard]] VoxelBlock GetBlock(BlockCoord coord) const;
};

/**
 * @brief Builds a compact voxel world from runtime map and chunk-grid data.
 *
 * Each map tile becomes one implicit voxel column. Solid occupancy spans from
 * the map minimum level up to the tile surface height. This function does not
 * perform face culling, greedy meshing, rendering, or block destruction.
 *
 * @param map Runtime map with dense terrain, collision, and height grids.
 * @param chunks Chunk grid built for the same runtime map.
 * @return Voxel world foundation with diagnostics for recoverable issues.
 */
[[nodiscard]] VoxelWorld BuildVoxelWorld(const RuntimeMap& map, const ChunkGrid& chunks);

/**
 * @brief Builds a compact stable log string for voxel-world diagnostics.
 *
 * @param world Voxel world data.
 * @return Compact human-readable summary.
 */
[[nodiscard]] std::string ToLogString(const VoxelWorld& world);

}  // namespace vox3d
