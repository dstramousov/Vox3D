#include "vox3d/transition/transition_feature.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <sstream>

namespace vox3d {
namespace {

[[nodiscard]] std::size_t GridIndex(int x, int y, int width)
{
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
}

[[nodiscard]] bool IsTileOpen(const RuntimeMap& map, TileCoord tile)
{
    return map.collision.cells[GridIndex(tile.x, tile.y, map.info.width)] == 0U;
}

[[nodiscard]] int TileHeight(const RuntimeMap& map, TileCoord tile)
{
    return map.height.cells[GridIndex(tile.x, tile.y, map.info.width)];
}

[[nodiscard]] FaceDirection DirectionFromDelta(int dx, int dy)
{
    if (dx < 0) {
        return FaceDirection::kWest;
    }
    if (dx > 0) {
        return FaceDirection::kEast;
    }
    if (dy < 0) {
        return FaceDirection::kNorth;
    }
    return FaceDirection::kSouth;
}

[[nodiscard]] TransitionFeatureKind KindFromDelta(int abs_delta)
{
    if (abs_delta <= 1) {
        return TransitionFeatureKind::kRamp;
    }
    if (abs_delta == 2) {
        return TransitionFeatureKind::kStairs;
    }
    return TransitionFeatureKind::kDrop;
}

void AccumulateFeatureStats(const TransitionFeature& feature, TransitionFeatureStats& stats)
{
    ++stats.total;
    switch (feature.kind) {
        case TransitionFeatureKind::kRamp:
            ++stats.ramps;
            break;
        case TransitionFeatureKind::kStairs:
            ++stats.stairs;
            break;
        case TransitionFeatureKind::kBridge:
            ++stats.bridges;
            break;
        case TransitionFeatureKind::kDrop:
            ++stats.drops;
            break;
    }
    if (feature.passable) {
        ++stats.passable;
    } else {
        ++stats.blocked;
    }
}

void AddFeatureBetweenTiles(
    const RuntimeMap& map,
    TileCoord from_tile,
    TileCoord to_tile,
    int dx,
    int dy,
    TransitionFeatureSet& result)
{
    const int from_level = TileHeight(map, from_tile);
    const int to_level = TileHeight(map, to_tile);
    const int delta = to_level - from_level;
    const int abs_delta = std::abs(delta);
    if (abs_delta == 0) {
        return;
    }

    const bool endpoints_open = IsTileOpen(map, from_tile) && IsTileOpen(map, to_tile);
    TransitionFeature feature;
    feature.kind = KindFromDelta(abs_delta);
    feature.from_tile = from_tile;
    feature.to_tile = to_tile;
    feature.from_level = from_level;
    feature.to_level = to_level;
    feature.delta_levels = delta;
    feature.direction = DirectionFromDelta(dx, dy);
    feature.passable = endpoints_open && feature.kind != TransitionFeatureKind::kDrop;

    AccumulateFeatureStats(feature, result.stats);
    result.features.push_back(feature);
}

}  // namespace

std::string_view ToString(TransitionFeatureKind kind)
{
    switch (kind) {
        case TransitionFeatureKind::kRamp:
            return "ramp";
        case TransitionFeatureKind::kStairs:
            return "stairs";
        case TransitionFeatureKind::kBridge:
            return "bridge";
        case TransitionFeatureKind::kDrop:
            return "drop";
    }
    return "unknown";
}

bool TransitionFeatureStats::IsValid() const
{
    return total == ramps + stairs + bridges + drops
        && total == passable + blocked;
}

bool TransitionFeatureSet::IsValid() const
{
    return stats.IsValid() && stats.total == features.size();
}

TransitionFeatureSet BuildTransitionFeatures(const RuntimeMap& map)
{
    TransitionFeatureSet result;

    if (!map.IsValid()) {
        result.diagnostics.AddWarning("cannot build transition features from invalid runtime map");
        return result;
    }
    if (!map.height.IsValid() || !map.collision.IsValid()) {
        result.diagnostics.AddWarning("cannot build transition features without height and collision grids");
        return result;
    }
    if (map.height.width != map.collision.width || map.height.height != map.collision.height) {
        result.diagnostics.AddWarning("cannot build transition features because height and collision dimensions differ");
        return result;
    }

    const std::uint64_t horizontal_pairs = map.info.width > 1
        ? static_cast<std::uint64_t>(map.info.width - 1) * static_cast<std::uint64_t>(map.info.height)
        : 0ULL;
    const std::uint64_t vertical_pairs = map.info.height > 1
        ? static_cast<std::uint64_t>(map.info.height - 1) * static_cast<std::uint64_t>(map.info.width)
        : 0ULL;
    const std::uint64_t max_pairs = horizontal_pairs + vertical_pairs;
    if (max_pairs <= static_cast<std::uint64_t>(std::vector<TransitionFeature>{}.max_size())) {
        result.features.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(max_pairs, 8192ULL)));
    }

    for (int y = 0; y < map.info.height; ++y) {
        for (int x = 0; x < map.info.width; ++x) {
            const TileCoord tile{x, y};
            if (x + 1 < map.info.width) {
                AddFeatureBetweenTiles(map, tile, TileCoord{x + 1, y}, 1, 0, result);
            }
            if (y + 1 < map.info.height) {
                AddFeatureBetweenTiles(map, tile, TileCoord{x, y + 1}, 0, 1, result);
            }
        }
    }

    if (!result.IsValid()) {
        result.diagnostics.AddWarning("transition feature validation failed after build");
    }
    return result;
}

std::string ToLogString(const TransitionFeatureSet& features)
{
    std::ostringstream out;
    out << "status=" << (features.IsValid() ? "loaded" : "unavailable");
    out << " total=" << features.stats.total;
    out << " ramps=" << features.stats.ramps;
    out << " stairs=" << features.stats.stairs;
    out << " bridges=" << features.stats.bridges;
    out << " drops=" << features.stats.drops;
    out << " passable=" << features.stats.passable;
    out << " blocked=" << features.stats.blocked;
    if (!features.diagnostics.warnings.empty()) {
        out << " warnings=" << features.diagnostics.warnings.size();
    }
    return out.str();
}

}  // namespace vox3d
