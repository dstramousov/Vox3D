#pragma once

#include "vox3d/chunk/chunk_grid.hpp"
#include "vox3d/core/result.hpp"
#include "vox3d/core/types.hpp"
#include "vox3d/mesh/face.hpp"
#include "vox3d/voxel/block.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vox3d {

/**
 * @brief Chunk mesh generation algorithm used by the CPU mesh builder.
 */
enum class ChunkMeshBuildMode : std::uint8_t {
    kSimpleFaces,
    kGreedyFaces,
    kTerrainSurface,
};

/**
 * @brief Converts a chunk mesh build mode to a stable diagnostic name.
 *
 * @param mode Build mode identifier.
 * @return Stable lowercase string representation.
 */
[[nodiscard]] std::string_view ToString(ChunkMeshBuildMode mode);

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
    ChunkMeshBuildMode mode = ChunkMeshBuildMode::kSimpleFaces;
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
    std::uint64_t terrain_top_faces = 0;
    std::uint64_t terrain_wall_faces = 0;
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
 * @brief High-level comparison between naive, culled, and greedy mesh sizes.
 */
struct MeshOptimizationStats {
    ChunkMeshBuildMode active_mode = ChunkMeshBuildMode::kSimpleFaces;
    std::uint64_t solid_blocks = 0;
    std::uint64_t naive_faces = 0;
    std::uint64_t culled_faces = 0;
    std::uint64_t simple_faces = 0;
    std::uint64_t greedy_faces = 0;
    std::uint64_t terrain_top_faces = 0;
    std::uint64_t terrain_wall_faces = 0;
    std::uint64_t terrain_faces = 0;
    std::uint64_t active_faces = 0;
    std::uint64_t active_vertices = 0;
    std::uint64_t active_indices = 0;
    std::uint64_t mesh_chunks = 0;
    std::uint64_t draw_models = 0;
    std::uint64_t skipped_chunks = 0;

    /**
     * @brief Returns the saved-face ratio from naive to simple visible faces.
     *
     * @return Ratio in range [0, 1], or 0 when naive face count is zero.
     */
    [[nodiscard]] double FaceCullingReductionRatio() const;

    /**
     * @brief Returns the saved-face ratio from simple to greedy mesh faces.
     *
     * @return Ratio in range [0, 1], or 0 when simple face count is zero.
     */
    [[nodiscard]] double GreedyReductionRatio() const;

    /**
     * @brief Returns the relative face-count change from greedy to terrain mesh.
     *
     * Negative values mean the terrain surface mesh emits fewer faces than
     * greedy voxel meshing. Positive values mean it emits more faces. Zero is
     * returned when greedy face count is unavailable.
     *
     * @return Relative change from greedy_faces to terrain_faces.
     */
    [[nodiscard]] double TerrainVsGreedyDeltaRatio() const;

    /**
     * @brief Returns the saved-face ratio from naive to active mesh faces.
     *
     * @return Ratio in range [0, 1], or 0 when naive face count is zero.
     */
    [[nodiscard]] double ActiveReductionRatio() const;
};

/**
 * @brief Builds a compact stable log string for chunk mesh diagnostics.
 *
 * @param result Chunk mesh build result.
 * @return Compact human-readable summary.
 */
[[nodiscard]] std::string ToLogString(const ChunkMeshBuildResult& result);

/**
 * @brief Builds a compact stable log string for mesh optimization diagnostics.
 *
 * @param stats Mesh optimization counters.
 * @return Compact human-readable summary.
 */
[[nodiscard]] std::string ToLogString(const MeshOptimizationStats& stats);

}  // namespace vox3d
