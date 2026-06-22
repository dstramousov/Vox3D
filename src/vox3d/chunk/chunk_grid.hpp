#pragma once

#include "vox3d/core/result.hpp"
#include "vox3d/core/types.hpp"
#include "vox3d/map/runtime_map.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace vox3d {

/**
 * @brief Tile-space inclusive-exclusive bounds for one chunk.
 */
struct TileBounds {
    int min_x = 0;
    int min_y = 0;
    int max_x = 0;
    int max_y = 0;

    /**
     * @brief Returns the bounds width in tiles.
     *
     * @return Positive width for valid bounds, otherwise zero or negative.
     */
    [[nodiscard]] int Width() const;

    /**
     * @brief Returns the bounds height in tiles.
     *
     * @return Positive height for valid bounds, otherwise zero or negative.
     */
    [[nodiscard]] int Height() const;

    /**
     * @brief Returns true when the bounds describe a non-empty tile rectangle.
     *
     * @return True if max coordinates are greater than min coordinates.
     */
    [[nodiscard]] bool IsValid() const;

    /**
     * @brief Checks whether a tile coordinate is inside the bounds.
     *
     * @param coord Tile coordinate to test.
     * @return True when coord is inside this inclusive-exclusive rectangle.
     */
    [[nodiscard]] bool Contains(TileCoord coord) const;
};

/**
 * @brief Immutable chunk coordinate in chunk-grid space.
 */
struct ChunkCoord {
    int x = 0;
    int y = 0;
};

/**
 * @brief Build options for chunk-grid construction.
 */
struct ChunkGridOptions {
    int chunk_size_x = 16;
    int chunk_size_y = 16;

    /**
     * @brief Returns true when chunk dimensions are positive.
     *
     * @return True if both chunk axes are greater than zero.
     */
    [[nodiscard]] bool IsValid() const;
};

/**
 * @brief Runtime statistics and bounds for one map chunk.
 */
struct ChunkInfo {
    ChunkCoord coord;
    TileBounds bounds;
    std::optional<LevelRange> levels;
    int blocked_cells = 0;
    bool dirty = false;

    /**
     * @brief Returns the number of map tiles covered by this chunk.
     *
     * @return Tile count inside the chunk bounds.
     */
    [[nodiscard]] std::size_t TileCount() const;

    /**
     * @brief Returns true when the chunk has valid bounds.
     *
     * @return True if the chunk covers at least one tile.
     */
    [[nodiscard]] bool IsValid() const;
};

/**
 * @brief Summary of a chunk grid built from a runtime map.
 */
struct ChunkGridInfo {
    int map_width = 0;
    int map_height = 0;
    int chunk_size_x = 0;
    int chunk_size_y = 0;
    int chunks_x = 0;
    int chunks_y = 0;
    int total_chunks = 0;
    int blocked_cells = 0;
    std::optional<LevelRange> levels;

    /**
     * @brief Returns true when the chunk grid dimensions are usable.
     *
     * @return True if map size, chunk size, and chunk count are positive.
     */
    [[nodiscard]] bool IsValid() const;
};

/**
 * @brief Chunk partition of a runtime map.
 *
 * The grid is editor-independent and contains only chunk bounds plus basic
 * per-chunk statistics. It does not own voxel blocks, meshes, or renderer data.
 */
struct ChunkGrid {
    ChunkGridInfo info;
    std::vector<ChunkInfo> chunks;
    Diagnostics diagnostics;

    /**
     * @brief Returns true when grid info and chunk storage are consistent.
     *
     * @return True if the chunk grid can be consumed by later engine layers.
     */
    [[nodiscard]] bool IsValid() const;

    /**
     * @brief Returns a chunk by chunk-grid coordinate.
     *
     * @param coord Chunk coordinate to look up.
     * @return Pointer to the chunk, or nullptr when the coordinate is outside the grid.
     */
    [[nodiscard]] const ChunkInfo* FindChunk(ChunkCoord coord) const;
};

/**
 * @brief Builds chunk bounds and per-chunk statistics from runtime-map data.
 *
 * This function is the foundation for future voxel, dirty-chunk, and mesh
 * builders. It does not build voxels or rendering resources.
 *
 * @param map Runtime map used as the source of dimensions and dense grids.
 * @param options Chunk-grid build options.
 * @return Chunk grid with diagnostics for recoverable issues.
 */
[[nodiscard]] ChunkGrid BuildChunkGrid(const RuntimeMap& map, ChunkGridOptions options = {});

/**
 * @brief Builds a compact stable log string for chunk-grid diagnostics.
 *
 * @param grid Chunk grid data.
 * @return Compact human-readable summary.
 */
[[nodiscard]] std::string ToLogString(const ChunkGrid& grid);

}  // namespace vox3d
