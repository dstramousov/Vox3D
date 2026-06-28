#pragma once

#include "vox3d/core/result.hpp"
#include "vox3d/core/types.hpp"
#include "vox3d/map/runtime_map.hpp"
#include "vox3d/movement/movement_probe.hpp"
#include "vox3d/transition/transition_feature.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace vox3d {

/**
 * @brief Semantic kind of one map-wide passability validation issue.
 */
enum class PassabilityIssueKind : std::uint8_t {
    kInvalidTransition,
    kBlockedRamp,
    kBlockedStairs,
    kSuspiciousDrop,
    kIsolatedTile,
};

/**
 * @brief Converts a passability validation issue kind to a stable name.
 *
 * @param kind Issue kind identifier.
 * @return Stable lowercase string representation.
 */
[[nodiscard]] std::string_view ToString(PassabilityIssueKind kind);

/**
 * @brief One map-wide passability validation issue.
 *
 * The issue is renderer-independent and stores enough tile/elevation context
 * for debug overlays, logs, and future validation reports. It does not modify
 * the map, collision grid, or transition feature set.
 */
struct PassabilityIssue {
    PassabilityIssueKind kind = PassabilityIssueKind::kInvalidTransition;
    TileCoord from_tile;
    TileCoord to_tile;
    int from_elevation = 0;
    int to_elevation = 0;
    int delta_levels = 0;
    TransitionFeatureKind transition_kind = TransitionFeatureKind::kRamp;
    MovementBlockReason block_reason = MovementBlockReason::kNone;
};

/**
 * @brief Aggregate counters for map-wide passability validation.
 */
struct PassabilityValidationStats {
    std::uint64_t checked_edges = 0;
    std::uint64_t passable_edges = 0;
    std::uint64_t blocked_edges = 0;
    std::uint64_t invalid_transitions = 0;
    std::uint64_t suspicious_drops = 0;
    std::uint64_t blocked_ramps = 0;
    std::uint64_t blocked_stairs = 0;
    std::uint64_t isolated_tiles = 0;
    std::uint64_t stored_issues = 0;
    bool issue_storage_truncated = false;

    /**
     * @brief Returns true when edge counters are internally consistent.
     *
     * @return True if checked edges equal passable plus blocked edges.
     */
    [[nodiscard]] bool IsValid() const;
};

/**
 * @brief Renderer-independent map-wide passability validation report.
 */
struct PassabilityValidationReport {
    bool valid = false;
    PassabilityValidationStats stats;
    std::vector<PassabilityIssue> issues;
    Diagnostics diagnostics;

    /**
     * @brief Returns true when the report was built from usable runtime data.
     *
     * @return True if the report is valid and counters match stored issues.
     */
    [[nodiscard]] bool IsValid() const;
};

/**
 * @brief Validates local passability rules across the whole runtime map.
 *
 * The validator scans each east/south neighbour pair exactly once. Flat,
 * ramp, stair, and bridge edges are considered passable only when both
 * endpoints are open and the elevation delta is within supported movement
 * rules. Drops, blocked endpoints, and inconsistent transition features are
 * reported as issues for debug overlays and diagnostics.
 *
 * @param map Runtime map containing dense height and collision grids.
 * @param transitions Transition features built for the same runtime map.
 * @return Passability validation report with counters and stored issues.
 */
[[nodiscard]] PassabilityValidationReport ValidatePassability(
    const RuntimeMap& map,
    const TransitionFeatureSet& transitions);

/**
 * @brief Builds a compact stable log string for passability diagnostics.
 *
 * @param report Passability validation report.
 * @return Compact human-readable summary.
 */
[[nodiscard]] std::string ToLogString(const PassabilityValidationReport& report);

}  // namespace vox3d
