#include "vox3d/validation/passability_validator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <sstream>

namespace vox3d {
namespace {

constexpr std::size_t kMaxStoredIssues = 4096U;

struct EdgeDelta {
    int dx = 0;
    int dy = 0;
};

constexpr std::array<EdgeDelta, 2> kUndirectedEdgeDeltas{{
    EdgeDelta{1, 0},
    EdgeDelta{0, 1},
}};

constexpr std::array<EdgeDelta, 4> kCardinalDeltas{{
    EdgeDelta{0, -1},
    EdgeDelta{0, 1},
    EdgeDelta{-1, 0},
    EdgeDelta{1, 0},
}};

[[nodiscard]] std::size_t GridIndex(const RuntimeMap& map, TileCoord tile)
{
    return static_cast<std::size_t>(tile.y) * static_cast<std::size_t>(map.info.width)
        + static_cast<std::size_t>(tile.x);
}

[[nodiscard]] bool IsBlocked(const RuntimeMap& map, TileCoord tile)
{
    return map.collision.cells[GridIndex(map, tile)] != 0U;
}

[[nodiscard]] bool IsOpen(const RuntimeMap& map, TileCoord tile)
{
    return !IsBlocked(map, tile);
}

[[nodiscard]] int HeightAt(const RuntimeMap& map, TileCoord tile)
{
    return map.height.cells[GridIndex(map, tile)];
}

[[nodiscard]] bool SameTile(TileCoord lhs, TileCoord rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

[[nodiscard]] bool IsSameUndirectedEdge(
    const TransitionFeature& feature,
    TileCoord from_tile,
    TileCoord to_tile)
{
    return (SameTile(feature.from_tile, from_tile) && SameTile(feature.to_tile, to_tile))
        || (SameTile(feature.from_tile, to_tile) && SameTile(feature.to_tile, from_tile));
}

[[nodiscard]] const TransitionFeature* FindTransition(
    const TransitionFeatureSet& transitions,
    TileCoord from_tile,
    TileCoord to_tile)
{
    if (!transitions.IsValid()) {
        return nullptr;
    }

    for (const TransitionFeature& feature : transitions.features) {
        if (IsSameUndirectedEdge(feature, from_tile, to_tile)) {
            return &feature;
        }
    }
    return nullptr;
}

[[nodiscard]] TransitionFeatureKind ExpectedKindFromAbsDelta(int abs_delta)
{
    if (abs_delta <= 1) {
        return TransitionFeatureKind::kRamp;
    }
    if (abs_delta == 2) {
        return TransitionFeatureKind::kStairs;
    }
    return TransitionFeatureKind::kDrop;
}

[[nodiscard]] bool IsTransitionKindConsistent(const TransitionFeature& feature)
{
    const int abs_delta = std::abs(feature.delta_levels);
    switch (feature.kind) {
        case TransitionFeatureKind::kRamp:
            return abs_delta == 1;
        case TransitionFeatureKind::kStairs:
            return abs_delta == 2;
        case TransitionFeatureKind::kBridge:
            return abs_delta <= 2;
        case TransitionFeatureKind::kDrop:
            return abs_delta > 2;
    }
    return false;
}

void StoreIssue(PassabilityValidationReport& report, PassabilityIssue issue)
{
    if (report.issues.size() >= kMaxStoredIssues) {
        report.stats.issue_storage_truncated = true;
        return;
    }
    report.issues.push_back(issue);
}

void StoreIssueForEdge(
    PassabilityValidationReport& report,
    PassabilityIssueKind kind,
    TileCoord from_tile,
    TileCoord to_tile,
    int from_elevation,
    int to_elevation,
    TransitionFeatureKind transition_kind,
    MovementBlockReason block_reason)
{
    PassabilityIssue issue;
    issue.kind = kind;
    issue.from_tile = from_tile;
    issue.to_tile = to_tile;
    issue.from_elevation = from_elevation;
    issue.to_elevation = to_elevation;
    issue.delta_levels = to_elevation - from_elevation;
    issue.transition_kind = transition_kind;
    issue.block_reason = block_reason;
    StoreIssue(report, issue);
}

void ValidateEdge(
    const RuntimeMap& map,
    const TransitionFeatureSet& transitions,
    TileCoord from_tile,
    TileCoord to_tile,
    PassabilityValidationReport& report)
{
    ++report.stats.checked_edges;

    const bool endpoints_open = IsOpen(map, from_tile) && IsOpen(map, to_tile);
    const int from_elevation = HeightAt(map, from_tile);
    const int to_elevation = HeightAt(map, to_tile);
    const int abs_delta = std::abs(to_elevation - from_elevation);
    const TransitionFeatureKind expected_kind = ExpectedKindFromAbsDelta(abs_delta);
    const TransitionFeature* transition = FindTransition(transitions, from_tile, to_tile);
    const TransitionFeatureKind actual_kind = transition != nullptr ? transition->kind : expected_kind;

    if (transition != nullptr && !IsTransitionKindConsistent(*transition)) {
        ++report.stats.invalid_transitions;
        StoreIssueForEdge(
            report,
            PassabilityIssueKind::kInvalidTransition,
            from_tile,
            to_tile,
            from_elevation,
            to_elevation,
            actual_kind,
            MovementBlockReason::kTransitionBlocked);
    }

    const bool is_passable = endpoints_open && abs_delta <= 2
        && actual_kind != TransitionFeatureKind::kDrop
        && (transition == nullptr || transition->passable);
    if (is_passable) {
        ++report.stats.passable_edges;
        return;
    }

    ++report.stats.blocked_edges;
    MovementBlockReason reason = MovementBlockReason::kTransitionBlocked;
    if (!IsOpen(map, from_tile)) {
        reason = MovementBlockReason::kSourceBlocked;
    } else if (!IsOpen(map, to_tile)) {
        reason = MovementBlockReason::kTargetBlocked;
    } else if (actual_kind == TransitionFeatureKind::kDrop || abs_delta > 2) {
        reason = MovementBlockReason::kDrop;
    }

    switch (actual_kind) {
        case TransitionFeatureKind::kRamp:
            if (abs_delta > 0) {
                ++report.stats.blocked_ramps;
                StoreIssueForEdge(
                    report,
                    PassabilityIssueKind::kBlockedRamp,
                    from_tile,
                    to_tile,
                    from_elevation,
                    to_elevation,
                    actual_kind,
                    reason);
            }
            break;
        case TransitionFeatureKind::kStairs:
            ++report.stats.blocked_stairs;
            StoreIssueForEdge(
                report,
                PassabilityIssueKind::kBlockedStairs,
                from_tile,
                to_tile,
                from_elevation,
                to_elevation,
                actual_kind,
                reason);
            break;
        case TransitionFeatureKind::kBridge:
            break;
        case TransitionFeatureKind::kDrop:
            ++report.stats.suspicious_drops;
            StoreIssueForEdge(
                report,
                PassabilityIssueKind::kSuspiciousDrop,
                from_tile,
                to_tile,
                from_elevation,
                to_elevation,
                actual_kind,
                reason);
            break;
    }
}

[[nodiscard]] bool HasPassableNeighbour(const RuntimeMap& map, TileCoord tile)
{
    const int from_elevation = HeightAt(map, tile);
    for (const EdgeDelta delta : kCardinalDeltas) {
        const TileCoord neighbour{tile.x + delta.dx, tile.y + delta.dy};
        if (!map.height.Contains(neighbour) || !map.collision.Contains(neighbour) || !IsOpen(map, neighbour)) {
            continue;
        }
        const int abs_delta = std::abs(HeightAt(map, neighbour) - from_elevation);
        if (abs_delta <= 2) {
            return true;
        }
    }
    return false;
}

void ValidateIsolatedTiles(const RuntimeMap& map, PassabilityValidationReport& report)
{
    for (int y = 0; y < map.info.height; ++y) {
        for (int x = 0; x < map.info.width; ++x) {
            const TileCoord tile{x, y};
            if (!IsOpen(map, tile) || HasPassableNeighbour(map, tile)) {
                continue;
            }

            ++report.stats.isolated_tiles;
            StoreIssueForEdge(
                report,
                PassabilityIssueKind::kIsolatedTile,
                tile,
                tile,
                HeightAt(map, tile),
                HeightAt(map, tile),
                TransitionFeatureKind::kRamp,
                MovementBlockReason::kTransitionBlocked);
        }
    }
}

}  // namespace

std::string_view ToString(PassabilityIssueKind kind)
{
    switch (kind) {
        case PassabilityIssueKind::kInvalidTransition:
            return "invalid_transition";
        case PassabilityIssueKind::kBlockedRamp:
            return "blocked_ramp";
        case PassabilityIssueKind::kBlockedStairs:
            return "blocked_stairs";
        case PassabilityIssueKind::kSuspiciousDrop:
            return "suspicious_drop";
        case PassabilityIssueKind::kIsolatedTile:
            return "isolated_tile";
    }
    return "unknown";
}

bool PassabilityValidationStats::IsValid() const
{
    return checked_edges == passable_edges + blocked_edges;
}

bool PassabilityValidationReport::IsValid() const
{
    return valid && stats.IsValid() && stats.stored_issues == issues.size();
}

PassabilityValidationReport ValidatePassability(
    const RuntimeMap& map,
    const TransitionFeatureSet& transitions)
{
    PassabilityValidationReport report;

    if (!map.HasCoreGrids()) {
        report.diagnostics.AddWarning("cannot validate passability without runtime core grids");
        return report;
    }
    if (!transitions.IsValid()) {
        report.diagnostics.AddWarning("passability validation uses an unavailable transition feature set");
    }

    report.valid = true;
    const std::uint64_t max_edges = map.info.width > 0 && map.info.height > 0
        ? (static_cast<std::uint64_t>(std::max(0, map.info.width - 1)) * static_cast<std::uint64_t>(map.info.height))
            + (static_cast<std::uint64_t>(std::max(0, map.info.height - 1))
                * static_cast<std::uint64_t>(map.info.width))
        : 0ULL;
    if (max_edges > 0 && max_edges <= static_cast<std::uint64_t>(std::vector<PassabilityIssue>{}.max_size())) {
        report.issues.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(max_edges, kMaxStoredIssues)));
    }

    for (int y = 0; y < map.info.height; ++y) {
        for (int x = 0; x < map.info.width; ++x) {
            const TileCoord from_tile{x, y};
            for (const EdgeDelta delta : kUndirectedEdgeDeltas) {
                const TileCoord to_tile{x + delta.dx, y + delta.dy};
                if (!map.height.Contains(to_tile) || !map.collision.Contains(to_tile)) {
                    continue;
                }
                ValidateEdge(map, transitions, from_tile, to_tile, report);
            }
        }
    }

    ValidateIsolatedTiles(map, report);
    report.stats.stored_issues = report.issues.size();

    if (!report.stats.IsValid()) {
        report.diagnostics.AddWarning("passability validation counters are inconsistent");
    }
    return report;
}

std::string ToLogString(const PassabilityValidationReport& report)
{
    std::ostringstream out;
    out << "status=" << (report.IsValid() ? "loaded" : "unavailable");
    out << " checked_edges=" << report.stats.checked_edges;
    out << " passable=" << report.stats.passable_edges;
    out << " blocked=" << report.stats.blocked_edges;
    out << " invalid=" << report.stats.invalid_transitions;
    out << " drops=" << report.stats.suspicious_drops;
    out << " blocked_ramps=" << report.stats.blocked_ramps;
    out << " blocked_stairs=" << report.stats.blocked_stairs;
    out << " isolated=" << report.stats.isolated_tiles;
    out << " stored=" << report.stats.stored_issues;
    if (report.stats.issue_storage_truncated) {
        out << " truncated=1";
    }
    if (!report.diagnostics.warnings.empty()) {
        out << " warnings=" << report.diagnostics.warnings.size();
    }
    return out.str();
}

}  // namespace vox3d
