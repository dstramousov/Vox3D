#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace vox3d {

/**
 * @brief Axis-aligned face direction of a voxel block.
 */
enum class FaceDirection : std::uint8_t {
    kWest,
    kEast,
    kNorth,
    kSouth,
    kDown,
    kUp,
};

/**
 * @brief Number of axis-aligned voxel face directions.
 */
inline constexpr std::size_t kFaceDirectionCount = 6;

/**
 * @brief Per-direction visible face counters.
 */
struct FaceDirectionCounts {
    std::array<std::uint64_t, kFaceDirectionCount> values{};

    /**
     * @brief Returns the counter for one direction.
     *
     * @param direction Face direction to read.
     * @return Number of visible faces recorded for the direction.
     */
    [[nodiscard]] std::uint64_t Get(FaceDirection direction) const;

    /**
     * @brief Adds one visible face for the provided direction.
     *
     * @param direction Face direction to increment.
     */
    void Increment(FaceDirection direction);
};

/**
 * @brief Converts a face direction to a stable lowercase diagnostic name.
 *
 * @param direction Face direction identifier.
 * @return Stable string representation.
 */
[[nodiscard]] std::string_view ToString(FaceDirection direction);

}  // namespace vox3d
