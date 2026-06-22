#pragma once

#include "vox3d/core/result.hpp"
#include "vox3d/core/types.hpp"
#include "vox3d/mesh/face.hpp"
#include "vox3d/voxel/voxel_world.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace vox3d {

/**
 * @brief Summary of visible and culled faces in a voxel world.
 */
struct FaceVisibilityInfo {
    int map_width = 0;
    int map_height = 0;
    int chunk_size_x = 0;
    int chunk_size_y = 0;
    int chunks_x = 0;
    int chunks_y = 0;
    int total_chunks = 0;
    std::optional<LevelRange> levels;
    std::uint64_t solid_blocks = 0;
    std::uint64_t naive_faces = 0;
    std::uint64_t visible_faces = 0;
    std::uint64_t culled_faces = 0;
    FaceDirectionCounts visible_by_direction;

    /**
     * @brief Returns true when face counters are internally consistent.
     *
     * @return True if dimensions and face counters are usable.
     */
    [[nodiscard]] bool IsValid() const;

    /**
     * @brief Returns the ratio of culled faces in the naive face set.
     *
     * @return Value in range [0, 1], or 0 when no naive faces exist.
     */
    [[nodiscard]] double CullRatio() const;
};

/**
 * @brief Result of voxel face-visibility analysis.
 */
struct FaceVisibilityResult {
    FaceVisibilityInfo info;
    Diagnostics diagnostics;

    /**
     * @brief Returns true when the visibility result can be consumed by mesh builders.
     *
     * @return True if info is valid and diagnostics did not report structural failure.
     */
    [[nodiscard]] bool IsValid() const;
};

/**
 * @brief Computes visible and hidden axis-aligned faces for a voxel world.
 *
 * A face is visible when the neighboring block in the same direction is empty
 * or lies outside the world/level bounds. Internal faces between two solid
 * blocks are counted as culled. This function only produces statistics and does
 * not allocate mesh vertices or renderer resources.
 *
 * @param world Voxel world to analyze.
 * @return Face visibility counters and diagnostics.
 */
[[nodiscard]] FaceVisibilityResult BuildFaceVisibility(const VoxelWorld& world);

/**
 * @brief Builds a compact stable log string for face-visibility diagnostics.
 *
 * @param result Face visibility result.
 * @return Compact human-readable summary.
 */
[[nodiscard]] std::string ToLogString(const FaceVisibilityResult& result);

}  // namespace vox3d
