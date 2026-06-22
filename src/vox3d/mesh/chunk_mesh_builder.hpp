#pragma once

#include "vox3d/chunk/chunk_grid.hpp"
#include "vox3d/mesh/mesh_data.hpp"
#include "vox3d/voxel/voxel_world.hpp"

namespace vox3d {

/**
 * @brief Builds renderer-independent visible-face mesh data for each chunk.
 *
 * Each visible voxel face is emitted as one indexed quad with four vertices and
 * six indices. The builder does not create renderer buffers, textures, camera
 * state, greedy meshes, or dirty-chunk rebuild state.
 *
 * @param world Voxel world used as the source of implicit blocks.
 * @param chunks Chunk grid defining per-chunk tile bounds.
 * @return Generated chunk meshes and diagnostics.
 */
[[nodiscard]] ChunkMeshBuildResult BuildChunkMeshes(const VoxelWorld& world, const ChunkGrid& chunks);

}  // namespace vox3d
