#pragma once

#include "vox3d/chunk/chunk_grid.hpp"
#include "vox3d/map/runtime_map.hpp"
#include "vox3d/mesh/chunk_mesh_builder.hpp"
#include "vox3d/mesh/mesh_data.hpp"

namespace vox3d {

/**
 * @brief Builds one stepped-heightfield terrain chunk.
 *
 * This chunk-local entry point is used by large-map CPU streaming. It reads
 * neighbouring height cells through RuntimeMap so chunk borders remain
 * geometrically consistent with a full-map terrain build.
 *
 * @param map Runtime map containing dense terrain, collision, and height grids.
 * @param chunks Chunk grid that owns the requested chunk.
 * @param chunk Chunk metadata to build.
 * @return Generated single-chunk mesh and diagnostics.
 */
[[nodiscard]] ChunkMeshBuildChunkResult BuildTerrainChunkMeshForChunk(
    const RuntimeMap& map,
    const ChunkGrid& chunks,
    const ChunkInfo& chunk);

/**
 * @brief Creates a deferred terrain mesh source without generating faces.
 *
 * The result contains one generated=false placeholder per chunk and can be
 * populated incrementally with BuildTerrainChunkMeshForChunk().
 *
 * @param map Runtime map defining map shape and level range.
 * @param chunks Chunk grid defining placeholder coordinates and bounds.
 * @return Deferred terrain mesh source ready for CPU streaming.
 */
[[nodiscard]] ChunkMeshBuildResult CreateDeferredTerrainChunkMeshes(
    const RuntimeMap& map,
    const ChunkGrid& chunks);

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

}  // namespace vox3d
