#pragma once

#include "vox3d/chunk/chunk_grid.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace vox3d {

/**
 * @brief Renderer-independent chunk visibility mode.
 */
enum class ChunkVisibilityMode : std::uint8_t {
    kAllChunks,
    kRadiusFade,
    kHardCull,
    kFrustumCull,
};

/**
 * @brief Renderer-independent visibility class assigned to one chunk.
 */
enum class ChunkVisibilityClass : std::uint8_t {
    kVisible,
    kFade,
    kHidden,
};

/**
 * @brief Minimal 3D vector used by renderer-independent culling code.
 */
struct Vec3f {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

/**
 * @brief Axis-aligned bounds in world units.
 */
struct Aabb3f {
    Vec3f min;
    Vec3f max;

    /**
     * @brief Returns true when every axis has ordered finite bounds.
     *
     * @return True if the AABB can be used for culling.
     */
    [[nodiscard]] bool IsValid() const;
};

/**
 * @brief World-space plane using the equation dot(normal, point) + distance.
 */
struct Plane3f {
    Vec3f normal;
    float distance = 0.0F;

    /**
     * @brief Computes signed distance from a point to this plane.
     *
     * Positive values are considered inside for frustum culling.
     *
     * @param point World-space point to test.
     * @return Signed plane distance.
     */
    [[nodiscard]] float SignedDistance(Vec3f point) const;
};

/**
 * @brief Six-plane camera frustum used by chunk AABB culling.
 */
struct Frustum3f {
    std::array<Plane3f, 6> planes{};
    bool valid = false;

    /**
     * @brief Returns true when the frustum has valid planes.
     *
     * @return True if frustum tests can be trusted.
     */
    [[nodiscard]] bool IsValid() const;

    /**
     * @brief Tests whether an AABB intersects this frustum.
     *
     * @param bounds World-space AABB to test.
     * @return True when the box is at least partially inside the frustum.
     */
    [[nodiscard]] bool Intersects(Aabb3f bounds) const;
};

/**
 * @brief One renderable chunk entry consumed by visibility classification.
 */
struct ChunkVisibilityItem {
    ChunkCoord coord;
    Aabb3f bounds;
    std::uint64_t faces = 0;
};

/**
 * @brief Visibility classification options for one frame.
 */
struct ChunkVisibilityOptions {
    ChunkVisibilityMode mode = ChunkVisibilityMode::kAllChunks;
    ChunkCoord camera_chunk;
    int radius_chunks = 2;
    int fade_ring_chunks = 1;
    Frustum3f frustum;
};

/**
 * @brief Per-chunk visibility result in the same order as the input items.
 */
struct ChunkVisibilityEntry {
    ChunkCoord coord;
    ChunkVisibilityClass visibility_class = ChunkVisibilityClass::kVisible;
    std::uint64_t faces = 0;
};

/**
 * @brief Per-frame visibility report for a chunk set.
 */
struct ChunkVisibilityReport {
    ChunkVisibilityMode mode = ChunkVisibilityMode::kAllChunks;
    int radius_chunks = 0;
    int fade_ring_chunks = 0;
    std::uint64_t resident_chunks = 0;
    std::uint64_t visible_chunks = 0;
    std::uint64_t fade_chunks = 0;
    std::uint64_t hidden_chunks = 0;
    std::uint64_t drawn_models = 0;
    std::uint64_t culled_models = 0;
    std::uint64_t total_faces = 0;
    std::uint64_t drawn_faces = 0;
    std::uint64_t culled_faces = 0;
    std::vector<ChunkVisibilityEntry> entries;

    /**
     * @brief Returns true when counters reference a resident chunk set.
     *
     * @return True if resident chunks match drawn plus culled models.
     */
    [[nodiscard]] bool IsValid() const;

    /**
     * @brief Returns the share of resident chunks skipped by visibility.
     *
     * @return Ratio in range [0, 1], or zero when no resident chunks exist.
     */
    [[nodiscard]] double DrawSavedRatio() const;

    /**
     * @brief Returns the share of mesh faces skipped by visibility.
     *
     * @return Ratio in range [0, 1], or zero when no faces exist.
     */
    [[nodiscard]] double FaceSavedRatio() const;
};

/**
 * @brief Converts a visibility mode to a stable diagnostic name.
 *
 * @param mode Visibility mode identifier.
 * @return Stable lowercase string representation.
 */
[[nodiscard]] std::string_view ToString(ChunkVisibilityMode mode);

/**
 * @brief Converts a visibility class to a stable diagnostic name.
 *
 * @param visibility_class Per-chunk visibility class.
 * @return Stable lowercase string representation.
 */
[[nodiscard]] std::string_view ToString(ChunkVisibilityClass visibility_class);

/**
 * @brief Builds chunk visibility classifications and aggregate counters.
 *
 * The function is renderer-independent. It does not allocate GPU resources,
 * inspect UI state, or draw anything. If frustum mode is requested without a
 * valid frustum, chunks are treated as visible to fail open instead of hiding
 * the whole map.
 *
 * @param items Resident chunk entries with world-space bounds.
 * @param options Visibility classification options for the current frame.
 * @return Per-chunk classifications and aggregate counters.
 */
[[nodiscard]] ChunkVisibilityReport BuildChunkVisibility(
    std::span<const ChunkVisibilityItem> items,
    const ChunkVisibilityOptions& options);

/**
 * @brief Builds a compact stable log string for chunk visibility diagnostics.
 *
 * @param report Visibility report.
 * @return Compact human-readable summary.
 */
[[nodiscard]] std::string ToLogString(const ChunkVisibilityReport& report);

}  // namespace vox3d
