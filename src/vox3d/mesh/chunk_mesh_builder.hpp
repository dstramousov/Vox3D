#pragma once

#include "vox3d/chunk/chunk_grid.hpp"
#include "vox3d/mesh/mesh_data.hpp"
#include "vox3d/voxel/voxel_world.hpp"

namespace vox3d {

/**
 * @brief Result of renderer-independent mesh generation for one chunk.
 */
struct ChunkMeshBuildChunkResult {
    ChunkMeshData mesh;
    Diagnostics diagnostics;

    /**
     * @brief Returns true when generated chunk mesh data is valid.
     *
     * @return True if mesh buffers are internally consistent.
     */
    [[nodiscard]] bool IsValid() const;
};

/**
 * @brief Builds renderer-independent mesh data for one chunk.
 *
 * This is the chunk-local entry point used by dirty rebuild caches. It does
 * not allocate renderer resources or inspect other chunks beyond block
 * neighbour queries performed through VoxelWorld.
 *
 * @param world Voxel world used as the source of implicit blocks.
 * @param chunk Chunk to build.
 * @param mode Mesh build algorithm to use.
 * @return Generated single-chunk mesh and diagnostics.
 */
[[nodiscard]] ChunkMeshBuildChunkResult BuildChunkMeshForChunk(
    const VoxelWorld& world,
    const ChunkInfo& chunk,
    ChunkMeshBuildMode mode = ChunkMeshBuildMode::kSimpleFaces);

/**
 * @brief Creates a valid deferred mesh source without generating chunk faces.
 *
 * The returned result contains one placeholder per chunk with generated=false.
 * Map shape and solid-block metadata are available immediately, while mesh
 * buffers can be filled incrementally with BuildChunkMeshForChunk().
 * Terrain-surface mode is not accepted because it must be built from RuntimeMap.
 *
 * @param world Voxel world defining map shape and level range.
 * @param chunks Chunk grid defining placeholder coordinates and bounds.
 * @param mode Simple or greedy voxel mesh mode.
 * @return Deferred mesh source ready for camera-centered CPU streaming.
 */
[[nodiscard]] ChunkMeshBuildResult CreateDeferredChunkMeshes(
    const VoxelWorld& world,
    const ChunkGrid& chunks,
    ChunkMeshBuildMode mode = ChunkMeshBuildMode::kSimpleFaces);

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
