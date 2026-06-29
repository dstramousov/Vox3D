#pragma once

#include "vox3d/core/result.hpp"
#include "vox3d/core/types.hpp"
#include "vox3d/map/runtime_map.hpp"
#include "vox3d/transition/transition_feature.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace vox3d {

/**
 * @brief High-level cost strategy used by the debug path probe.
 */
enum class PathProfile : std::uint8_t {
    kShortest,
    kSafe,
};

/**
 * @brief Final status of one path probe run.
 */
enum class PathProbeStatus : std::uint8_t {
    kNotRun,
    kFound,
    kNotFound,
    kInvalidRequest,
};

/**
 * @brief Converts a path profile to a stable diagnostic name.
 *
 * @param profile Path cost profile identifier.
 * @return Stable lowercase string representation.
 */
[[nodiscard]] std::string_view ToString(PathProfile profile);

/**
 * @brief Converts a path probe status to a stable diagnostic name.
 *
 * @param status Path probe status identifier.
 * @return Stable lowercase string representation.
 */
[[nodiscard]] std::string_view ToString(PathProbeStatus status);

/**
 * @brief Detailed cost components accumulated by a path probe.
 */
struct PathCostBreakdown {
    double base = 0.0;
    double terrain = 0.0;
    double elevation = 0.0;
    double transition = 0.0;
    double safety = 0.0;
    double total = 0.0;
};

/**
 * @brief One tile on a reconstructed path with local and accumulated cost.
 */
struct PathStep {
    TileCoord tile;
    std::uint32_t step_index = 0;
    double step_cost = 0.0;
    double accumulated_cost = 0.0;
};

/**
 * @brief Options used by the renderer-independent path probe.
 */
struct PathProbeOptions {
    PathProfile profile = PathProfile::kShortest;
    std::uint32_t max_stored_visited_tiles = 8192;
};

/**
 * @brief Aggregate counters from one path probe run.
 */
struct PathProbeStats {
    std::uint64_t visited_nodes = 0;
    std::uint64_t expanded_nodes = 0;
    std::uint64_t generated_edges = 0;
    std::uint64_t blocked_edges = 0;
    bool visited_storage_truncated = false;
};

/**
 * @brief Renderer-independent weighted A* path probe result.
 */
struct PathProbeResult {
    bool valid = false;
    PathProfile profile = PathProfile::kShortest;
    PathProbeStatus status = PathProbeStatus::kNotRun;
    TileCoord start;
    TileCoord goal;
    std::vector<PathStep> path;
    std::vector<TileCoord> visited_tiles;
    PathCostBreakdown cost;
    PathProbeStats stats;
    Diagnostics diagnostics;

    /**
     * @brief Returns true when the probe finished and found a usable route.
     *
     * @return True if status is kFound and at least one path step exists.
     */
    [[nodiscard]] bool HasPath() const;

    /**
     * @brief Returns true when the probe contains a finished diagnostic result.
     *
     * @return True if the result is valid and no longer in kNotRun state.
     */
    [[nodiscard]] bool IsValid() const;
};

/**
 * @brief Runs a weighted A* debug route probe between two runtime-map tiles.
 *
 * The path probe is diagnostic only. It uses cardinal movement, runtime
 * collision/height grids, and transition features. The selected profile changes
 * edge costs, not passability: blocked cells and drops remain non-passable.
 *
 * @param map Runtime map containing dense terrain, height, and collision grids.
 * @param transitions Transition feature set built for the same runtime map.
 * @param start Start tile coordinate.
 * @param goal Goal tile coordinate.
 * @param options Cost profile and storage limits.
 * @return Path probe result with route, visited tiles, and cost breakdown.
 */
[[nodiscard]] PathProbeResult RunPathProbe(
    const RuntimeMap& map,
    const TransitionFeatureSet& transitions,
    TileCoord start,
    TileCoord goal,
    PathProbeOptions options = {});

/**
 * @brief Finds the zero-based path step index for a tile.
 *
 * @param result Path probe result.
 * @param tile Tile coordinate to search for.
 * @return Path step index, or -1 when the tile is not on the path.
 */
[[nodiscard]] int FindPathStepIndex(const PathProbeResult& result, TileCoord tile);

/**
 * @brief Builds a compact stable log string for path probe diagnostics.
 *
 * @param result Path probe result.
 * @return Compact human-readable summary.
 */
[[nodiscard]] std::string ToLogString(const PathProbeResult& result);

}  // namespace vox3d
