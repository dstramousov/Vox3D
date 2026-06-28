#pragma once

#include "vox3d/core/result.hpp"
#include "vox3d/core/types.hpp"
#include "vox3d/map/runtime_map.hpp"
#include "vox3d/mesh/face.hpp"
#include "vox3d/transition/transition_feature.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace vox3d {

/**
 * @brief Reason why one movement probe step is not passable.
 */
enum class MovementBlockReason : std::uint8_t {
    kNone,
    kMissingData,
    kSourceBlocked,
    kOutOfBounds,
    kTargetBlocked,
    kDrop,
    kTransitionBlocked,
};

/**
 * @brief Converts a movement block reason to a stable diagnostic name.
 *
 * @param reason Block reason identifier.
 * @return Stable lowercase string representation.
 */
[[nodiscard]] std::string_view ToString(MovementBlockReason reason);

/**
 * @brief One checked neighbour step from a source tile.
 */
struct MovementProbeStep {
    FaceDirection direction = FaceDirection::kNorth;
    TileCoord from_tile;
    TileCoord to_tile;
    bool target_in_bounds = false;
    bool target_blocked = false;
    int from_elevation = 0;
    int to_elevation = 0;
    int delta_levels = 0;
    bool has_transition = false;
    TransitionFeatureKind transition_kind = TransitionFeatureKind::kRamp;
    bool passable = false;
    MovementBlockReason block_reason = MovementBlockReason::kMissingData;
};

/**
 * @brief Aggregate counters for a movement probe around one selected tile.
 */
struct MovementProbeStats {
    std::uint64_t checked = 0;
    std::uint64_t passable = 0;
    std::uint64_t blocked = 0;

    /**
     * @brief Returns true when at least one neighbour was checked.
     *
     * @return True if checked is greater than zero and counters are consistent.
     */
    [[nodiscard]] bool IsValid() const;
};

/**
 * @brief Renderer-independent movement probe result for one selected tile.
 */
struct MovementProbeResult {
    bool valid = false;
    TileCoord source_tile;
    bool source_blocked = false;
    int source_elevation = 0;
    std::array<MovementProbeStep, 4> steps{};
    int step_count = 0;
    MovementProbeStats stats;
    Diagnostics diagnostics;

    /**
     * @brief Returns true when the probe references a valid source tile.
     *
     * @return True if the source tile and checked steps are usable.
     */
    [[nodiscard]] bool IsValid() const;
};

/**
 * @brief Builds a 4-way movement probe around one selected tile.
 *
 * The probe is gameplay-diagnostic only. It checks cardinal neighbours using
 * dense runtime height/collision grids and transition features. Flat steps,
 * ramps, stairs, and bridges are passable when both endpoints are open. Drops,
 * blocked endpoints, out-of-bounds neighbours, and explicitly blocked
 * transitions are reported with a block reason.
 *
 * @param map Runtime map containing dense height and collision grids.
 * @param transitions Transition feature set built for the same runtime map.
 * @param source_tile Tile coordinate used as movement source.
 * @return Movement probe result with per-neighbour diagnostics.
 */
[[nodiscard]] MovementProbeResult BuildMovementProbe(
    const RuntimeMap& map,
    const TransitionFeatureSet& transitions,
    TileCoord source_tile);

/**
 * @brief Builds a compact stable log string for movement probe diagnostics.
 *
 * @param result Movement probe result.
 * @return Compact human-readable summary.
 */
[[nodiscard]] std::string ToLogString(const MovementProbeResult& result);

}  // namespace vox3d
