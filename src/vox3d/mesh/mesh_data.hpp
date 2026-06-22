#pragma once

#include "vox3d/chunk/chunk_grid.hpp"
#include "vox3d/core/result.hpp"
#include "vox3d/core/types.hpp"
#include "vox3d/mesh/face.hpp"
#include "vox3d/voxel/block.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace vox3d {

/**
 * @brief Renderer-independent 3D position used by generated mesh data.
 */
struct MeshPosition {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

/**
 * @brief One generated mesh vertex without renderer-specific resources.
 */
struct MeshVertex {
    MeshPosition position;
    BlockTypeId block_type = BlockTypeId::kEmpty;
    FaceDirection face_direction = FaceDirection::kUp;
};

/**
 * @brief One visible block face converted to indexed quad metadata.
 */
struct MeshFace {
    BlockCoord block;
    FaceDirection direction = FaceDirection::kUp;
    BlockTypeId block_type = BlockTypeId::kEmpty;
    std::uint32_t first_vertex = 0;
    std::uint32_t first_index = 0;
};

/**
 * @brief Renderer-independent mesh data for one chunk.
 */
struct ChunkMeshData {
    ChunkCoord coord;
    TileBounds bounds;
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<MeshFace> faces;

    /**
     * @brief Returns true when the chunk mesh has internally consistent buffers.
     *
     * @return True if face, vertex, and index counts match quad topology.
     */
    [[nodiscard]] bool IsValid() const;

    /**
     * @brief Returns the number of visible faces in this chunk mesh.
     *
     * @return Number of emitted quad faces.
     */
    [[nodiscard]] std::uint64_t FaceCount() const;
};

/**
 * @brief Summary of generated chunk mesh data.
 */
struct ChunkMeshBuildInfo {
    int map_width = 0;
    int map_height = 0;
    int chunk_size_x = 0;
    int chunk_size_y = 0;
    int chunks_x = 0;
    int chunks_y = 0;
    int total_chunks = 0;
    std::optional<LevelRange> levels;
    std::uint64_t solid_blocks = 0;
    std::uint64_t visible_faces = 0;
    std::uint64_t vertices = 0;
    std::uint64_t indices = 0;
    std::uint64_t non_empty_chunks = 0;

    /**
     * @brief Returns true when mesh build counters are internally consistent.
     *
     * @return True if dimensions and generated buffer counts are usable.
     */
    [[nodiscard]] bool IsValid() const;
};

/**
 * @brief Result of renderer-independent chunk mesh generation.
 */
struct ChunkMeshBuildResult {
    ChunkMeshBuildInfo info;
    std::vector<ChunkMeshData> chunks;
    Diagnostics diagnostics;

    /**
     * @brief Returns true when generated meshes can be consumed by a renderer.
     *
     * @return True if summary and chunk buffers are internally consistent.
     */
    [[nodiscard]] bool IsValid() const;
};

/**
 * @brief Builds a compact stable log string for chunk mesh diagnostics.
 *
 * @param result Chunk mesh build result.
 * @return Compact human-readable summary.
 */
[[nodiscard]] std::string ToLogString(const ChunkMeshBuildResult& result);

}  // namespace vox3d
