#pragma once

#include "vox3d/chunk/chunk_grid.hpp"
#include "vox3d/map/runtime_map.hpp"
#include "vox3d/mesh/mesh_data.hpp"

namespace vox3d {

/**
 * @brief Checks whether a runtime map contains usable 4x4 structure micro geometry.
 *
 * @param map Runtime map to inspect.
 * @return True when type, height, and micro-mask grids are loaded and valid.
 */
[[nodiscard]] bool HasStructureMicroGeometry(const RuntimeMap& map);

/**
 * @brief Appends visible 4x4 structure micro geometry for one chunk.
 *
 * The function emits merged top and side quads for vertical structure columns.
 * Neighbour occupancy is queried across tile and chunk boundaries, so internal
 * faces are removed without expanding the terrain voxel world to micro scale.
 *
 * @param map Runtime map containing structure type, height, and micro-mask grids.
 * @param chunk Chunk whose tile bounds own the emitted micro columns.
 * @param mesh Destination chunk mesh. Existing terrain or voxel faces are preserved.
 * @param info Optional mesh-build counters to update. May be nullptr.
 * @param diagnostics Destination for recoverable build warnings.
 */
void AppendStructureMicroChunkMesh(
    const RuntimeMap& map,
    const ChunkInfo& chunk,
    ChunkMeshData& mesh,
    ChunkMeshBuildInfo* info,
    Diagnostics& diagnostics);

}  // namespace vox3d
