#pragma once

#include "vox3d/chunk/chunk_grid.hpp"
#include "vox3d/mesh/mesh_data.hpp"
#include "vox3d/voxel/voxel_world.hpp"

namespace vox3d {

/**
 * @brief Builds renderer-independent mesh data for each chunk.
 *
 * In simple mode each visible voxel face is emitted as one indexed quad. In
 * greedy mode adjacent coplanar faces with the same block type and normal are
 * merged into larger quads. The builder does not create renderer buffers,
 * textures, camera state, dirty-chunk rebuild state, or GPU resources.
 *
 * @param world Voxel world used as the source of implicit blocks.
 * @param chunks Chunk grid defining per-chunk tile bounds.
 * @param mode Mesh build algorithm to use.
 * @return Generated chunk meshes and diagnostics.
 */
[[nodiscard]] ChunkMeshBuildResult BuildChunkMeshes(
    const VoxelWorld& world,
    const ChunkGrid& chunks,
    ChunkMeshBuildMode mode = ChunkMeshBuildMode::kSimpleFaces);

}  // namespace vox3d
