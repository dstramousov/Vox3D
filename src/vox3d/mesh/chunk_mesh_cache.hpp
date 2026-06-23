#pragma once

#include "vox3d/chunk/chunk_grid.hpp"
#include "vox3d/core/result.hpp"
#include "vox3d/core/types.hpp"
#include "vox3d/mesh/mesh_data.hpp"
#include "vox3d/voxel/voxel_world.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace vox3d {

/**
 * @brief Cached mesh counters for a chunk-mesh cache.
 */
struct ChunkMeshCacheInfo {
    ChunkMeshBuildMode mode = ChunkMeshBuildMode::kSimpleFaces;
    int map_width = 0;
    int map_height = 0;
    int chunk_size_x = 0;
    int chunk_size_y = 0;
    int chunks_x = 0;
    int chunks_y = 0;
    int total_chunks = 0;
    std::optional<LevelRange> levels;
    std::uint64_t non_empty_chunks = 0;
    std::uint64_t faces = 0;
    std::uint64_t vertices = 0;
    std::uint64_t indices = 0;
    std::uint64_t dirty_chunks = 0;

    /**
     * @brief Returns true when cache dimensions and counters are consistent.
     *
     * @return True if the cache can be used for chunk-local rebuilds.
     */
    [[nodiscard]] bool IsValid() const;
};

/**
 * @brief Report produced by a dirty chunk mesh rebuild pass.
 */
struct ChunkMeshRebuildReport {
    ChunkMeshBuildMode mode = ChunkMeshBuildMode::kSimpleFaces;
    bool attempted = false;
    bool valid = false;
    int total_chunks = 0;
    std::uint64_t dirty_chunks = 0;
    std::uint64_t rebuilt_chunks = 0;
    std::uint64_t reused_chunks = 0;
    std::uint64_t old_faces = 0;
    std::uint64_t new_faces = 0;
    std::uint64_t old_vertices = 0;
    std::uint64_t new_vertices = 0;
    std::uint64_t old_indices = 0;
    std::uint64_t new_indices = 0;
    Diagnostics diagnostics;

    /**
     * @brief Returns true when the rebuild pass completed with usable output.
     *
     * @return True if the cache was valid after the rebuild attempt.
     */
    [[nodiscard]] bool IsValid() const;

    /**
     * @brief Returns the ratio of chunks skipped by dirty rebuild caching.
     *
     * @return Ratio in range [0, 1], or 0 when the total chunk count is zero.
     */
    [[nodiscard]] double SavedRebuildWorkRatio() const;

    /**
     * @brief Returns the relative active-face count change after rebuild.
     *
     * Negative values mean fewer faces. Positive values mean more faces. Zero
     * is returned when the baseline face count is unavailable.
     *
     * @return Relative change from old_faces to new_faces.
     */
    [[nodiscard]] double FaceDeltaRatio() const;
};

/**
 * @brief CPU-side cache of generated chunk mesh data and dirty flags.
 *
 * The cache owns renderer-independent meshes only. It does not own GPU buffers,
 * raylib models, input state, or editor UI state.
 */
struct ChunkMeshCache {
    ChunkMeshCacheInfo info;
    std::vector<ChunkMeshData> chunks;
    std::vector<std::uint8_t> dirty;
    Diagnostics diagnostics;

    /**
     * @brief Returns true when cache info, meshes, and dirty flags are usable.
     *
     * @return True if this cache can be consumed by renderer upload code.
     */
    [[nodiscard]] bool IsValid() const;

    /**
     * @brief Returns the number of chunks currently marked dirty.
     *
     * @return Dirty chunk count.
     */
    [[nodiscard]] std::uint64_t DirtyCount() const;

    /**
     * @brief Returns a cached chunk mesh by chunk-grid coordinate.
     *
     * @param coord Chunk coordinate to look up.
     * @return Pointer to cached mesh, or nullptr when coord is outside grid.
     */
    [[nodiscard]] const ChunkMeshData* FindChunk(ChunkCoord coord) const;

    /**
     * @brief Marks one chunk dirty if it exists in the cache.
     *
     * @param coord Chunk coordinate to mark.
     * @return True if a chunk was found and marked dirty.
     */
    bool MarkChunkDirty(ChunkCoord coord);

    /**
     * @brief Marks the chunk containing a tile and border neighbours dirty.
     *
     * If the tile lies on a chunk border, the adjacent chunk sharing that border
     * is also marked dirty. This is required because face visibility depends on
     * neighbour blocks across chunk boundaries.
     *
     * @param tile Tile coordinate affected by a future block edit.
     * @param chunks Chunk grid used to resolve tile bounds.
     * @return Number of chunks that were newly marked dirty.
     */
    std::uint64_t MarkTileAndBorderChunksDirty(TileCoord tile, const ChunkGrid& chunks);

    /**
     * @brief Marks every cached chunk dirty.
     */
    void MarkAllDirty();

    /**
     * @brief Clears all dirty flags in the cache.
     */
    void ClearDirty();
};

/**
 * @brief Builds a chunk mesh cache from a voxel world and chunk grid.
 *
 * @param world Source voxel world.
 * @param chunks Chunk grid describing chunk bounds.
 * @param mode Mesh generation algorithm used for all cache entries.
 * @return Fully populated mesh cache with all dirty flags cleared.
 */
[[nodiscard]] ChunkMeshCache BuildChunkMeshCache(
    const VoxelWorld& world,
    const ChunkGrid& chunks,
    ChunkMeshBuildMode mode);

/**
 * @brief Rebuilds only chunks marked dirty in a mesh cache.
 *
 * @param world Source voxel world.
 * @param chunks Chunk grid describing chunk bounds.
 * @param cache Cache to update in place. Must not be nullptr.
 * @return Dirty rebuild report with saved-work diagnostics.
 */
[[nodiscard]] ChunkMeshRebuildReport RebuildDirtyChunkMeshes(
    const VoxelWorld& world,
    const ChunkGrid& chunks,
    ChunkMeshCache* cache);

/**
 * @brief Converts a mesh cache to the existing build-result value type.
 *
 * The returned value owns a copy of the cached mesh data and can be passed to
 * renderer code that still consumes ChunkMeshBuildResult.
 *
 * @param cache Source cache to copy from.
 * @return Chunk mesh build result view of the cache contents.
 */
[[nodiscard]] ChunkMeshBuildResult ToChunkMeshBuildResult(const ChunkMeshCache& cache);

/**
 * @brief Builds a compact stable log string for chunk mesh cache diagnostics.
 *
 * @param cache Mesh cache to describe.
 * @return Compact human-readable summary.
 */
[[nodiscard]] std::string ToLogString(const ChunkMeshCache& cache);

/**
 * @brief Builds a compact stable log string for dirty rebuild diagnostics.
 *
 * @param report Dirty rebuild report.
 * @return Compact human-readable summary.
 */
[[nodiscard]] std::string ToLogString(const ChunkMeshRebuildReport& report);

}  // namespace vox3d
