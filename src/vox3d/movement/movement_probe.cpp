#include "vox3d/movement/movement_probe.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <sstream>

namespace vox3d {
namespace {

struct DirectionDelta {
    FaceDirection direction = FaceDirection::kNorth;
    int dx = 0;
    int dy = 0;
};

constexpr std::array<DirectionDelta, 4> kCardinalDirections{{
    DirectionDelta{FaceDirection::kNorth, 0, -1},
    DirectionDelta{FaceDirection::kSouth, 0, 1},
    DirectionDelta{FaceDirection::kWest, -1, 0},
    DirectionDelta{FaceDirection::kEast, 1, 0},
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

[[nodiscard]] int HeightAt(const RuntimeMap& map, TileCoord tile)
{
    return map.height.cells[GridIndex(map, tile)];
}

[[nodiscard]] TransitionFeatureKind KindFromHeightDelta(int abs_delta)
{
    if (abs_delta <= 1) {
        return TransitionFeatureKind::kRamp;
    }
    if (abs_delta == 2) {
        return TransitionFeatureKind::kStairs;
    }
    return TransitionFeatureKind::kDrop;
}

[[nodiscard]] bool SameTile(TileCoord lhs, TileCoord rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
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
        const bool same_direction = SameTile(feature.from_tile, from_tile) && SameTile(feature.to_tile, to_tile);
        const bool reverse_direction = SameTile(feature.from_tile, to_tile) && SameTile(feature.to_tile, from_tile);
        if (same_direction || reverse_direction) {
            return &feature;
        }
    }
    return nullptr;
}

void AccumulateStep(const MovementProbeStep& step, MovementProbeStats& stats)
{
    ++stats.checked;
    if (step.passable) {
        ++stats.passable;
    } else {
        ++stats.blocked;
    }
}

[[nodiscard]] MovementProbeStep BuildStep(
    const RuntimeMap& map,
    const TransitionFeatureSet& transitions,
    TileCoord source_tile,
    bool source_blocked,
    DirectionDelta direction)
{
    MovementProbeStep step;
    step.direction = direction.direction;
    step.from_tile = source_tile;
    step.to_tile = TileCoord{source_tile.x + direction.dx, source_tile.y + direction.dy};
    step.from_elevation = HeightAt(map, source_tile);
    step.to_elevation = step.from_elevation;

    if (!map.height.Contains(step.to_tile) || !map.collision.Contains(step.to_tile)) {
        step.block_reason = MovementBlockReason::kOutOfBounds;
        return step;
    }

    step.target_in_bounds = true;
    step.target_blocked = IsBlocked(map, step.to_tile);
    step.to_elevation = HeightAt(map, step.to_tile);
    step.delta_levels = step.to_elevation - step.from_elevation;

    const int abs_delta = std::abs(step.delta_levels);
    const TransitionFeature* transition = FindTransition(transitions, step.from_tile, step.to_tile);
    if (abs_delta > 0) {
        step.has_transition = true;
        step.transition_kind = transition != nullptr ? transition->kind : KindFromHeightDelta(abs_delta);
    }

    if (source_blocked) {
        step.block_reason = MovementBlockReason::kSourceBlocked;
        return step;
    }
    if (step.target_blocked) {
        step.block_reason = MovementBlockReason::kTargetBlocked;
        return step;
    }
    if (abs_delta == 0) {
        step.passable = true;
        step.block_reason = MovementBlockReason::kNone;
        return step;
    }
    if (step.transition_kind == TransitionFeatureKind::kDrop) {
        step.block_reason = MovementBlockReason::kDrop;
        return step;
    }
    if (transition != nullptr && !transition->passable) {
        step.block_reason = MovementBlockReason::kTransitionBlocked;
        return step;
    }

    step.passable = true;
    step.block_reason = MovementBlockReason::kNone;
    return step;
}

}  // namespace

std::string_view ToString(MovementBlockReason reason)
{
    switch (reason) {
        case MovementBlockReason::kNone:
            return "none";
        case MovementBlockReason::kMissingData:
            return "missing_data";
        case MovementBlockReason::kSourceBlocked:
            return "source_blocked";
        case MovementBlockReason::kOutOfBounds:
            return "out_of_bounds";
        case MovementBlockReason::kTargetBlocked:
            return "target_blocked";
        case MovementBlockReason::kDrop:
            return "drop";
        case MovementBlockReason::kTransitionBlocked:
            return "transition_blocked";
    }
    return "unknown";
}

bool MovementProbeStats::IsValid() const
{
    return checked > 0 && checked == passable + blocked;
}

bool MovementProbeResult::IsValid() const
{
    return valid && step_count > 0 && stats.IsValid();
}

MovementProbeResult BuildMovementProbe(
    const RuntimeMap& map,
    const TransitionFeatureSet& transitions,
    TileCoord source_tile)
{
    MovementProbeResult result;
    result.source_tile = source_tile;

    if (!map.HasCoreGrids()) {
        result.diagnostics.AddWarning("cannot build movement probe without runtime core grids");
        return result;
    }
    if (!map.height.Contains(source_tile) || !map.collision.Contains(source_tile)) {
        result.diagnostics.AddWarning("movement probe source tile is outside runtime map bounds");
        return result;
    }

    result.valid = true;
    result.source_blocked = IsBlocked(map, source_tile);
    result.source_elevation = HeightAt(map, source_tile);
    result.step_count = static_cast<int>(kCardinalDirections.size());
    for (int index = 0; index < result.step_count; ++index) {
        result.steps[static_cast<std::size_t>(index)] = BuildStep(
            map,
            transitions,
            source_tile,
            result.source_blocked,
            kCardinalDirections[static_cast<std::size_t>(index)]);
        AccumulateStep(result.steps[static_cast<std::size_t>(index)], result.stats);
    }
    return result;
}

std::string ToLogString(const MovementProbeResult& result)
{
    std::ostringstream out;
    out << "status=" << (result.IsValid() ? "loaded" : "unavailable");
    out << " tile=" << result.source_tile.x << ',' << result.source_tile.y;
    out << " checked=" << result.stats.checked;
    out << " passable=" << result.stats.passable;
    out << " blocked=" << result.stats.blocked;
    if (!result.diagnostics.warnings.empty()) {
        out << " warning=\"" << result.diagnostics.warnings.front() << '"';
    }
    return out.str();
}

}  // namespace vox3d
