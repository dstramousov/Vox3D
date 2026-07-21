#pragma once

#include "vox3d/chunk/chunk_grid.hpp"
#include "vox3d/map/runtime_map.hpp"
#include "vox3d/mesh/mesh_data.hpp"

#include <cstdint>
#include <vector>

namespace vox3d {

/**
 * @brief Builds renderer-independent stepped-heightfield terrain meshes.
 *
 * The builder uses the runtime map as the source of truth. It emits merged
 * top-surface spans for adjacent tiles with the same height and block type,
 * and merged wall/cliff spans only where neighbouring tiles are lower or
 * outside the map. It does not generate full voxel columns, ramps, stairs,
 * bridges, renderer resources, or GPU buffers.
 *
 * @param map Runtime map containing dense terrain, collision, and height grids.
 * @param chunks Chunk grid defining per-chunk tile bounds.
 * @return Generated terrain chunk meshes and diagnostics.
 */
[[nodiscard]] ChunkMeshBuildResult BuildTerrainChunkMeshes(const RuntimeMap& map, const ChunkGrid& chunks);

/**
 * @brief Builds stepped-heightfield terrain meshes only for selected chunk indices.
 *
 * Unselected chunks are returned as valid empty mesh entries. This keeps the
 * result shape compatible with full-grid renderer code while avoiding full-map
 * startup mesh generation on large maps.
 *
 * @param map Runtime map containing dense terrain, collision, and height grids.
 * @param chunks Chunk grid defining per-chunk tile bounds.
 * @param selected_chunks Per-chunk mask, non-zero means build this chunk.
 * @return Partially populated terrain chunk meshes and diagnostics.
 */
[[nodiscard]] ChunkMeshBuildResult BuildTerrainChunkMeshesForSelectedChunks(
    const RuntimeMap& map,
    const ChunkGrid& chunks,
    const std::vector<std::uint8_t>& selected_chunks);

}  // namespace vox3d
