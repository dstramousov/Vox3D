#pragma once

#include "vox3d/chunk/chunk_grid.hpp"
#include "vox3d/core/result.hpp"
#include "vox3d/core/types.hpp"
#include "vox3d/map/runtime_map.hpp"
#include "vox3d/transition/transition_feature.hpp"

#include <cstdint>
#include <string>

namespace vox3d {

/**
 * @brief Aggregate transition counters touching one inspected tile.
 */
struct TileTransitionInspectStats {
    std::uint64_t total = 0;
    std::uint64_t ramps = 0;
    std::uint64_t stairs = 0;
    std::uint64_t bridges = 0;
    std::uint64_t drops = 0;
    std::uint64_t passable = 0;
    std::uint64_t blocked = 0;

    /**
     * @brief Returns true when the inspected tile has at least one transition.
     *
     * @return True if total is greater than zero.
     */
    [[nodiscard]] bool IsValid() const;
};

/**
 * @brief Renderer-independent inspection result for one runtime-map tile.
 */
struct TileInspectResult {
    bool valid = false;
    TileCoord tile;
    std::string terrain;
    int elevation = 0;
    bool structure_height_available = false;
    std::uint8_t structure_height = 0;
    bool blocked = false;
    bool movement_cost_available = false;
    int movement_cost = -1;
    bool projectile_block_available = false;
    bool projectile_blocked = false;
    bool vision_block_available = false;
    bool vision_blocked = false;
    bool cover_available = false;
    std::uint8_t cover = 0;
    bool concealment_available = false;
    std::uint8_t concealment = 0;
    bool chunk_found = false;
    ChunkCoord chunk;
    TileBounds chunk_bounds;
    TileTransitionInspectStats transitions;
    Diagnostics diagnostics;

    /**
     * @brief Returns true when the result references a valid map tile.
     *
     * @return True if inspection succeeded.
     */
    [[nodiscard]] bool IsValid() const;
};

/**
 * @brief Inspects one tile using runtime map, chunk, and transition data.
 *
 * The function is renderer-independent. It performs only bounds checking and
 * reads already-built data layers. The returned result is suitable for editor
 * panels, logging, and gameplay diagnostics.
 *
 * @param map Runtime map that owns dense terrain, height, and collision grids.
 * @param chunks Chunk grid built for the same runtime map.
 * @param transitions Transition feature set built for the same runtime map.
 * @param tile Tile coordinate to inspect.
 * @return Inspection result with diagnostics for invalid input.
 */
[[nodiscard]] TileInspectResult InspectTile(
    const RuntimeMap& map,
    const ChunkGrid& chunks,
    const TransitionFeatureSet& transitions,
    TileCoord tile);

/**
 * @brief Builds a compact stable log string for tile inspection.
 *
 * @param result Tile inspection result.
 * @return Compact human-readable summary.
 */
[[nodiscard]] std::string ToLogString(const TileInspectResult& result);

}  // namespace vox3d
