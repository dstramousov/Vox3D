#include "vox3d/path/path_probe.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace vox3d {
namespace {

struct DirectionDelta {
    int dx = 0;
    int dy = 0;
};

struct EdgeTransitionInfo {
    bool present = false;
    TransitionFeatureKind kind = TransitionFeatureKind::kRamp;
    bool passable = true;
};

struct EdgeCost {
    double base = 1.0;
    double terrain = 0.0;
    double elevation = 0.0;
    double transition = 0.0;
    double safety = 0.0;

    [[nodiscard]] double Total() const
    {
        return base + terrain + elevation + transition + safety;
    }
};

struct QueueNode {
    int index = 0;
    double priority = 0.0;

    [[nodiscard]] bool operator>(const QueueNode& other) const
    {
        return priority > other.priority;
    }
};

constexpr std::array<DirectionDelta, 4> kCardinalDirections{{
    DirectionDelta{0, -1},
    DirectionDelta{0, 1},
    DirectionDelta{-1, 0},
    DirectionDelta{1, 0},
}};

[[nodiscard]] bool SameTile(TileCoord lhs, TileCoord rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

[[nodiscard]] int TileIndex(const RuntimeMap& map, TileCoord tile)
{
    return tile.y * map.info.width + tile.x;
}

[[nodiscard]] TileCoord TileFromIndex(const RuntimeMap& map, int index)
{
    return TileCoord{index % map.info.width, index / map.info.width};
}

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

[[nodiscard]] std::string TerrainAt(const RuntimeMap& map, TileCoord tile)
{
    if (!map.terrain.IsValid() || !map.terrain.Contains(tile)) {
        return {};
    }
    return map.terrain.cells[GridIndex(map, tile)];
}

[[nodiscard]] std::string Lowercase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

[[nodiscard]] bool ContainsText(const std::string& text, std::string_view token)
{
    return text.find(token) != std::string::npos;
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

[[nodiscard]] std::uint64_t EdgeKey(int lhs, int rhs)
{
    const std::uint32_t a = static_cast<std::uint32_t>(std::min(lhs, rhs));
    const std::uint32_t b = static_cast<std::uint32_t>(std::max(lhs, rhs));
    return (static_cast<std::uint64_t>(a) << 32U) | static_cast<std::uint64_t>(b);
}

[[nodiscard]] std::unordered_map<std::uint64_t, EdgeTransitionInfo> BuildTransitionLookup(
    const RuntimeMap& map,
    const TransitionFeatureSet& transitions)
{
    std::unordered_map<std::uint64_t, EdgeTransitionInfo> lookup;
    if (!transitions.IsValid()) {
        return lookup;
    }

    lookup.reserve(transitions.features.size());
    for (const TransitionFeature& feature : transitions.features) {
        if (!map.height.Contains(feature.from_tile) || !map.height.Contains(feature.to_tile)) {
            continue;
        }
        lookup[EdgeKey(TileIndex(map, feature.from_tile), TileIndex(map, feature.to_tile))] = EdgeTransitionInfo{
            true,
            feature.kind,
            feature.passable,
        };
    }
    return lookup;
}

[[nodiscard]] EdgeTransitionInfo FindTransitionInfo(
    const RuntimeMap& map,
    const std::unordered_map<std::uint64_t, EdgeTransitionInfo>& lookup,
    TileCoord from,
    TileCoord to,
    int abs_delta)
{
    const auto it = lookup.find(EdgeKey(TileIndex(map, from), TileIndex(map, to)));
    if (it != lookup.end()) {
        return it->second;
    }
    if (abs_delta == 0) {
        return EdgeTransitionInfo{};
    }
    return EdgeTransitionInfo{true, KindFromHeightDelta(abs_delta), true};
}

[[nodiscard]] bool HasAdjacentDrop(const RuntimeMap& map, TileCoord tile)
{
    if (!map.height.Contains(tile)) {
        return false;
    }
    const int base_height = HeightAt(map, tile);
    for (const DirectionDelta direction : kCardinalDirections) {
        const TileCoord neighbour{tile.x + direction.dx, tile.y + direction.dy};
        if (!map.height.Contains(neighbour)) {
            return true;
        }
        if (std::abs(HeightAt(map, neighbour) - base_height) > 2) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] double TerrainPenalty(PathProfile profile, const std::string& terrain)
{
    if (profile == PathProfile::kShortest) {
        return 0.0;
    }

    const std::string normalized = Lowercase(terrain);
    if (ContainsText(normalized, "water") || ContainsText(normalized, "swamp")
        || ContainsText(normalized, "mud") || ContainsText(normalized, "marsh")) {
        return 2.0;
    }
    if (ContainsText(normalized, "ruin") || ContainsText(normalized, "debris")) {
        return 0.6;
    }
    if (ContainsText(normalized, "forest") || ContainsText(normalized, "mushroom")) {
        return 0.4;
    }
    return 0.0;
}

[[nodiscard]] double ElevationPenalty(PathProfile profile, int delta_levels)
{
    const int abs_delta = std::abs(delta_levels);
    if (delta_levels > 0) {
        return profile == PathProfile::kSafe ? 0.45 * static_cast<double>(abs_delta)
                                             : 0.10 * static_cast<double>(abs_delta);
    }
    if (delta_levels < 0) {
        return profile == PathProfile::kSafe ? 0.15 * static_cast<double>(abs_delta)
                                             : 0.05 * static_cast<double>(abs_delta);
    }
    return 0.0;
}

[[nodiscard]] double TransitionPenalty(PathProfile profile, EdgeTransitionInfo transition)
{
    if (!transition.present) {
        return 0.0;
    }

    switch (transition.kind) {
        case TransitionFeatureKind::kRamp:
            return profile == PathProfile::kSafe ? 0.35 : 0.10;
        case TransitionFeatureKind::kStairs:
            return profile == PathProfile::kSafe ? 1.00 : 0.35;
        case TransitionFeatureKind::kBridge:
            return profile == PathProfile::kSafe ? 0.45 : 0.20;
        case TransitionFeatureKind::kDrop:
            return std::numeric_limits<double>::infinity();
    }
    return 0.0;
}

[[nodiscard]] double SafetyPenalty(PathProfile profile, const RuntimeMap& map, TileCoord from, TileCoord to)
{
    if (profile == PathProfile::kShortest) {
        return HasAdjacentDrop(map, to) ? 0.10 : 0.0;
    }

    double penalty = 0.0;
    if (HasAdjacentDrop(map, from)) {
        penalty += 1.25;
    }
    if (HasAdjacentDrop(map, to)) {
        penalty += 2.00;
    }
    return penalty;
}

[[nodiscard]] double Heuristic(TileCoord from, TileCoord to)
{
    return static_cast<double>(std::abs(from.x - to.x) + std::abs(from.y - to.y));
}

[[nodiscard]] bool BuildEdgeCost(
    const RuntimeMap& map,
    const std::unordered_map<std::uint64_t, EdgeTransitionInfo>& transition_lookup,
    PathProfile profile,
    TileCoord from,
    TileCoord to,
    EdgeCost* out_cost)
{
    if (out_cost == nullptr || !map.height.Contains(to) || !map.collision.Contains(to)) {
        return false;
    }
    if (IsBlocked(map, from) || IsBlocked(map, to)) {
        return false;
    }

    const int delta = HeightAt(map, to) - HeightAt(map, from);
    const int abs_delta = std::abs(delta);
    const EdgeTransitionInfo transition = FindTransitionInfo(map, transition_lookup, from, to, abs_delta);
    if (abs_delta > 2 || (transition.present && transition.kind == TransitionFeatureKind::kDrop)
        || (transition.present && !transition.passable)) {
        return false;
    }

    EdgeCost cost;
    cost.terrain = TerrainPenalty(profile, TerrainAt(map, to));
    cost.elevation = ElevationPenalty(profile, delta);
    cost.transition = TransitionPenalty(profile, transition);
    cost.safety = SafetyPenalty(profile, map, from, to);
    if (!std::isfinite(cost.Total())) {
        return false;
    }

    *out_cost = cost;
    return true;
}

void AccumulateCost(const EdgeCost& edge, PathCostBreakdown& cost)
{
    cost.base += edge.base;
    cost.terrain += edge.terrain;
    cost.elevation += edge.elevation;
    cost.transition += edge.transition;
    cost.safety += edge.safety;
    cost.total += edge.Total();
}

[[nodiscard]] PathCostBreakdown RebuildCostBreakdown(
    const RuntimeMap& map,
    const std::unordered_map<std::uint64_t, EdgeTransitionInfo>& transition_lookup,
    PathProfile profile,
    const std::vector<TileCoord>& tiles)
{
    PathCostBreakdown cost;
    for (std::size_t index = 1; index < tiles.size(); ++index) {
        EdgeCost edge;
        if (BuildEdgeCost(map, transition_lookup, profile, tiles[index - 1], tiles[index], &edge)) {
            AccumulateCost(edge, cost);
        }
    }
    return cost;
}

[[nodiscard]] std::vector<TileCoord> ReconstructPathTiles(
    const RuntimeMap& map,
    const std::vector<int>& parent,
    int goal_index)
{
    std::vector<TileCoord> reversed;
    int current = goal_index;
    while (current >= 0) {
        reversed.push_back(TileFromIndex(map, current));
        current = parent[static_cast<std::size_t>(current)];
    }
    std::reverse(reversed.begin(), reversed.end());
    return reversed;
}

[[nodiscard]] std::vector<PathStep> BuildPathSteps(
    const RuntimeMap& map,
    const std::unordered_map<std::uint64_t, EdgeTransitionInfo>& transition_lookup,
    PathProfile profile,
    const std::vector<TileCoord>& tiles)
{
    std::vector<PathStep> steps;
    steps.reserve(tiles.size());
    double accumulated = 0.0;
    for (std::size_t index = 0; index < tiles.size(); ++index) {
        double step_cost = 0.0;
        if (index > 0) {
            EdgeCost edge;
            if (BuildEdgeCost(map, transition_lookup, profile, tiles[index - 1], tiles[index], &edge)) {
                step_cost = edge.Total();
                accumulated += step_cost;
            }
        }
        steps.push_back(PathStep{
            tiles[index],
            static_cast<std::uint32_t>(index),
            step_cost,
            accumulated,
        });
    }
    return steps;
}

void ValidatePathRequest(
    const RuntimeMap& map,
    TileCoord start,
    TileCoord goal,
    PathProbeResult& result)
{
    if (!map.HasCoreGrids()) {
        result.diagnostics.AddWarning("cannot run path probe without runtime core grids");
        return;
    }
    if (!map.height.Contains(start) || !map.collision.Contains(start)) {
        result.diagnostics.AddWarning("path probe start tile is outside runtime map bounds");
        return;
    }
    if (!map.height.Contains(goal) || !map.collision.Contains(goal)) {
        result.diagnostics.AddWarning("path probe goal tile is outside runtime map bounds");
        return;
    }
    if (IsBlocked(map, start)) {
        result.diagnostics.AddWarning("path probe start tile is blocked");
        return;
    }
    if (IsBlocked(map, goal)) {
        result.diagnostics.AddWarning("path probe goal tile is blocked");
        return;
    }

    result.valid = true;
}

}  // namespace

std::string_view ToString(PathProfile profile)
{
    switch (profile) {
        case PathProfile::kShortest:
            return "shortest";
        case PathProfile::kSafe:
            return "safe";
    }
    return "unknown";
}

std::string_view ToString(PathProbeStatus status)
{
    switch (status) {
        case PathProbeStatus::kNotRun:
            return "not_run";
        case PathProbeStatus::kFound:
            return "found";
        case PathProbeStatus::kNotFound:
            return "not_found";
        case PathProbeStatus::kInvalidRequest:
            return "invalid_request";
    }
    return "unknown";
}

bool PathProbeResult::HasPath() const
{
    return valid && status == PathProbeStatus::kFound && !path.empty();
}

bool PathProbeResult::IsValid() const
{
    return valid && status != PathProbeStatus::kNotRun;
}

PathProbeResult RunPathProbe(
    const RuntimeMap& map,
    const TransitionFeatureSet& transitions,
    TileCoord start,
    TileCoord goal,
    PathProbeOptions options)
{
    PathProbeResult result;
    result.profile = options.profile;
    result.start = start;
    result.goal = goal;
    result.status = PathProbeStatus::kInvalidRequest;

    ValidatePathRequest(map, start, goal, result);
    if (!result.valid) {
        return result;
    }

    const int tile_count = map.info.width * map.info.height;
    const int start_index = TileIndex(map, start);
    const int goal_index = TileIndex(map, goal);
    const auto transition_lookup = BuildTransitionLookup(map, transitions);

    std::vector<double> best_cost(static_cast<std::size_t>(tile_count), std::numeric_limits<double>::infinity());
    std::vector<int> parent(static_cast<std::size_t>(tile_count), -1);
    std::vector<std::uint8_t> closed(static_cast<std::size_t>(tile_count), 0U);
    std::priority_queue<QueueNode, std::vector<QueueNode>, std::greater<QueueNode>> frontier;

    best_cost[static_cast<std::size_t>(start_index)] = 0.0;
    frontier.push(QueueNode{start_index, Heuristic(start, goal)});

    const std::size_t max_stored_visited = static_cast<std::size_t>(options.max_stored_visited_tiles);
    while (!frontier.empty()) {
        const QueueNode current_node = frontier.top();
        frontier.pop();
        const int current_index = current_node.index;
        if (closed[static_cast<std::size_t>(current_index)] != 0U) {
            continue;
        }

        closed[static_cast<std::size_t>(current_index)] = 1U;
        const TileCoord current_tile = TileFromIndex(map, current_index);
        ++result.stats.visited_nodes;
        if (result.visited_tiles.size() < max_stored_visited) {
            result.visited_tiles.push_back(current_tile);
        } else {
            result.stats.visited_storage_truncated = true;
        }

        if (current_index == goal_index) {
            const std::vector<TileCoord> path_tiles = ReconstructPathTiles(map, parent, goal_index);
            result.path = BuildPathSteps(map, transition_lookup, options.profile, path_tiles);
            result.cost = RebuildCostBreakdown(map, transition_lookup, options.profile, path_tiles);
            result.status = PathProbeStatus::kFound;
            return result;
        }

        ++result.stats.expanded_nodes;
        for (const DirectionDelta direction : kCardinalDirections) {
            const TileCoord neighbour{current_tile.x + direction.dx, current_tile.y + direction.dy};
            ++result.stats.generated_edges;
            EdgeCost edge;
            if (!BuildEdgeCost(map, transition_lookup, options.profile, current_tile, neighbour, &edge)) {
                ++result.stats.blocked_edges;
                continue;
            }

            const int neighbour_index = TileIndex(map, neighbour);
            if (closed[static_cast<std::size_t>(neighbour_index)] != 0U) {
                continue;
            }
            const double next_cost = best_cost[static_cast<std::size_t>(current_index)] + edge.Total();
            if (next_cost >= best_cost[static_cast<std::size_t>(neighbour_index)]) {
                continue;
            }

            best_cost[static_cast<std::size_t>(neighbour_index)] = next_cost;
            parent[static_cast<std::size_t>(neighbour_index)] = current_index;
            frontier.push(QueueNode{neighbour_index, next_cost + Heuristic(neighbour, goal)});
        }
    }

    result.status = PathProbeStatus::kNotFound;
    return result;
}

int FindPathStepIndex(const PathProbeResult& result, TileCoord tile)
{
    if (!result.HasPath()) {
        return -1;
    }
    for (std::size_t index = 0; index < result.path.size(); ++index) {
        if (SameTile(result.path[index].tile, tile)) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

std::string ToLogString(const PathProbeResult& result)
{
    std::ostringstream out;
    out << "status=" << ToString(result.status);
    out << " profile=" << ToString(result.profile);
    out << " start=" << result.start.x << ',' << result.start.y;
    out << " goal=" << result.goal.x << ',' << result.goal.y;
    out << " steps=" << result.path.size();
    out << " cost=" << result.cost.total;
    out << " visited=" << result.stats.visited_nodes;
    out << " expanded=" << result.stats.expanded_nodes;
    out << " blocked_edges=" << result.stats.blocked_edges;
    if (result.stats.visited_storage_truncated) {
        out << " visited_storage=truncated";
    }
    if (!result.diagnostics.warnings.empty()) {
        out << " warning=\"" << result.diagnostics.warnings.front() << '"';
    }
    return out.str();
}

}  // namespace vox3d
