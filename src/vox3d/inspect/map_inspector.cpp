#include "vox3d/inspect/map_inspector.hpp"

#include <cstddef>
#include <sstream>

namespace vox3d {
namespace {

[[nodiscard]] std::size_t TileIndex(const RuntimeMap& map, TileCoord tile)
{
    return static_cast<std::size_t>(tile.y) * static_cast<std::size_t>(map.info.width)
        + static_cast<std::size_t>(tile.x);
}

[[nodiscard]] bool SameTile(TileCoord lhs, TileCoord rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

void AccumulateTransition(TransitionFeatureKind kind, bool passable, TileTransitionInspectStats& stats)
{
    ++stats.total;
    switch (kind) {
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

    if (passable) {
        ++stats.passable;
    } else {
        ++stats.blocked;
    }
}

[[nodiscard]] const ChunkInfo* FindChunkForTile(const ChunkGrid& chunks, TileCoord tile)
{
    if (!chunks.IsValid()) {
        return nullptr;
    }

    const int chunk_size_x = chunks.info.chunk_size_x > 0 ? chunks.info.chunk_size_x : 1;
    const int chunk_size_y = chunks.info.chunk_size_y > 0 ? chunks.info.chunk_size_y : 1;
    const ChunkCoord coord{tile.x / chunk_size_x, tile.y / chunk_size_y};
    const ChunkInfo* chunk = chunks.FindChunk(coord);
    if (chunk != nullptr && chunk->bounds.Contains(tile)) {
        return chunk;
    }

    for (const ChunkInfo& candidate : chunks.chunks) {
        if (candidate.bounds.Contains(tile)) {
            return &candidate;
        }
    }
    return nullptr;
}

}  // namespace

bool TileTransitionInspectStats::IsValid() const
{
    return total > 0;
}

bool TileInspectResult::IsValid() const
{
    return valid;
}

TileInspectResult InspectTile(
    const RuntimeMap& map,
    const ChunkGrid& chunks,
    const TransitionFeatureSet& transitions,
    TileCoord tile)
{
    TileInspectResult result;
    result.tile = tile;

    if (!map.HasCoreGrids()) {
        result.diagnostics.AddWarning("runtime map core grids are unavailable");
        return result;
    }
    if (!map.terrain.Contains(tile) || !map.height.Contains(tile) || !map.collision.Contains(tile)) {
        result.diagnostics.AddWarning("tile is outside runtime map bounds");
        return result;
    }

    const std::size_t index = TileIndex(map, tile);
    result.valid = true;
    result.terrain = map.terrain.cells[index];
    result.elevation = map.height.cells[index];
    result.blocked = map.collision.cells[index] != 0U;
    if (map.movement_cost.Contains(tile)) {
        result.movement_cost_available = true;
        result.movement_cost = map.movement_cost.cells[index];
    }
    if (map.projectile_block.Contains(tile)) {
        result.projectile_block_available = true;
        result.projectile_blocked = map.projectile_block.cells[index] != 0U;
    }
    if (map.vision_block.Contains(tile)) {
        result.vision_block_available = true;
        result.vision_blocked = map.vision_block.cells[index] != 0U;
    }
    if (map.cover.Contains(tile)) {
        result.cover_available = true;
        result.cover = map.cover.cells[index];
    }
    if (map.concealment.Contains(tile)) {
        result.concealment_available = true;
        result.concealment = map.concealment.cells[index];
    }

    if (const ChunkInfo* chunk = FindChunkForTile(chunks, tile); chunk != nullptr) {
        result.chunk_found = true;
        result.chunk = chunk->coord;
        result.chunk_bounds = chunk->bounds;
    } else {
        result.diagnostics.AddWarning("tile chunk was not found");
    }

    if (transitions.IsValid()) {
        for (const TransitionFeature& feature : transitions.features) {
            if (!SameTile(feature.from_tile, tile) && !SameTile(feature.to_tile, tile)) {
                continue;
            }
            AccumulateTransition(feature.kind, feature.passable, result.transitions);
        }
    }

    return result;
}

std::string ToLogString(const TileInspectResult& result)
{
    std::ostringstream out;
    out << "status=" << (result.IsValid() ? "selected" : "unavailable");
    out << " tile=" << result.tile.x << ',' << result.tile.y;
    if (!result.IsValid()) {
        if (!result.diagnostics.warnings.empty()) {
            out << " warning=\"" << result.diagnostics.warnings.front() << '"';
        }
        return out.str();
    }

    out << " terrain=" << result.terrain;
    out << " elevation=" << result.elevation;
    out << " blocked=" << (result.blocked ? "yes" : "no");
    if (result.movement_cost_available) {
        out << " movement_cost=" << result.movement_cost;
    }
    if (result.projectile_block_available) {
        out << " projectile_blocked=" << (result.projectile_blocked ? "yes" : "no");
    }
    if (result.vision_block_available) {
        out << " vision_blocked=" << (result.vision_blocked ? "yes" : "no");
    }
    if (result.cover_available) {
        out << " cover=" << static_cast<int>(result.cover);
    }
    if (result.concealment_available) {
        out << " concealment=" << static_cast<int>(result.concealment);
    }
    if (result.chunk_found) {
        out << " chunk=" << result.chunk.x << ',' << result.chunk.y;
    } else {
        out << " chunk=none";
    }
    out << " transitions=" << result.transitions.total;
    out << " ramps=" << result.transitions.ramps;
    out << " stairs=" << result.transitions.stairs;
    out << " bridges=" << result.transitions.bridges;
    out << " drops=" << result.transitions.drops;
    out << " passable=" << result.transitions.passable;
    out << " blocked_transitions=" << result.transitions.blocked;
    return out.str();
}

}  // namespace vox3d
