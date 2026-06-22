#include "vox3d/chunk/chunk_grid.hpp"

#include <algorithm>
#include <cstddef>
#include <sstream>

namespace vox3d {
namespace {

[[nodiscard]] int CeilDiv(int value, int divisor)
{
    if (value <= 0 || divisor <= 0) {
        return 0;
    }
    return (value + divisor - 1) / divisor;
}

[[nodiscard]] std::size_t GridIndex(int x, int y, int width)
{
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
}

[[nodiscard]] bool HasUsableHeightGrid(const RuntimeMap& map)
{
    return map.info.elevation_loaded && map.height.IsValid();
}

[[nodiscard]] bool HasUsableCollisionGrid(const RuntimeMap& map)
{
    return map.info.collision_loaded && map.collision.IsValid();
}

void FillChunkStatistics(const RuntimeMap& map, ChunkInfo& chunk)
{
    const bool has_height = HasUsableHeightGrid(map);
    const bool has_collision = HasUsableCollisionGrid(map);

    bool found_height = false;
    LevelRange levels{};

    for (int y = chunk.bounds.min_y; y < chunk.bounds.max_y; ++y) {
        for (int x = chunk.bounds.min_x; x < chunk.bounds.max_x; ++x) {
            if (has_collision && map.collision.cells[GridIndex(x, y, map.collision.width)] != 0) {
                ++chunk.blocked_cells;
            }

            if (has_height) {
                const int level = map.height.cells[GridIndex(x, y, map.height.width)];
                if (!found_height) {
                    levels = LevelRange{level, level};
                    found_height = true;
                } else {
                    levels.min = std::min(levels.min, level);
                    levels.max = std::max(levels.max, level);
                }
            }
        }
    }

    if (found_height) {
        chunk.levels = levels;
    }
}

}  // namespace

int TileBounds::Width() const
{
    return max_x - min_x;
}

int TileBounds::Height() const
{
    return max_y - min_y;
}

bool TileBounds::IsValid() const
{
    return Width() > 0 && Height() > 0;
}

bool TileBounds::Contains(TileCoord coord) const
{
    return coord.x >= min_x && coord.y >= min_y && coord.x < max_x && coord.y < max_y;
}

bool ChunkGridOptions::IsValid() const
{
    return chunk_size_x > 0 && chunk_size_y > 0;
}

std::size_t ChunkInfo::TileCount() const
{
    if (!bounds.IsValid()) {
        return 0;
    }
    return static_cast<std::size_t>(bounds.Width()) * static_cast<std::size_t>(bounds.Height());
}

bool ChunkInfo::IsValid() const
{
    return bounds.IsValid();
}

bool ChunkGridInfo::IsValid() const
{
    return map_width > 0 && map_height > 0 && chunk_size_x > 0 && chunk_size_y > 0 && chunks_x > 0 && chunks_y > 0
        && total_chunks == chunks_x * chunks_y;
}

bool ChunkGrid::IsValid() const
{
    return info.IsValid() && chunks.size() == static_cast<std::size_t>(info.total_chunks)
        && std::all_of(chunks.begin(), chunks.end(), [](const ChunkInfo& chunk) {
               return chunk.IsValid();
           });
}

const ChunkInfo* ChunkGrid::FindChunk(ChunkCoord coord) const
{
    if (coord.x < 0 || coord.y < 0 || coord.x >= info.chunks_x || coord.y >= info.chunks_y) {
        return nullptr;
    }

    const std::size_t index = static_cast<std::size_t>(coord.y) * static_cast<std::size_t>(info.chunks_x)
        + static_cast<std::size_t>(coord.x);
    if (index >= chunks.size()) {
        return nullptr;
    }
    return &chunks[index];
}

ChunkGrid BuildChunkGrid(const RuntimeMap& map, ChunkGridOptions options)
{
    ChunkGrid grid;

    if (!options.IsValid()) {
        grid.diagnostics.AddWarning(
            "invalid chunk grid options chunk_size=" + std::to_string(options.chunk_size_x) + "x"
            + std::to_string(options.chunk_size_y));
        return grid;
    }

    if (!map.IsValid()) {
        grid.diagnostics.AddWarning("cannot build chunk grid from invalid runtime map");
        return grid;
    }

    if (!map.HasCoreGrids()) {
        grid.diagnostics.AddWarning("chunk grid built without complete runtime map core grids");
    }

    grid.info.map_width = map.info.width;
    grid.info.map_height = map.info.height;
    grid.info.chunk_size_x = options.chunk_size_x;
    grid.info.chunk_size_y = options.chunk_size_y;
    grid.info.chunks_x = CeilDiv(map.info.width, options.chunk_size_x);
    grid.info.chunks_y = CeilDiv(map.info.height, options.chunk_size_y);
    grid.info.total_chunks = grid.info.chunks_x * grid.info.chunks_y;

    if (map.info.levels.has_value()) {
        grid.info.levels = *map.info.levels;
    }

    grid.chunks.reserve(static_cast<std::size_t>(grid.info.total_chunks));
    for (int chunk_y = 0; chunk_y < grid.info.chunks_y; ++chunk_y) {
        for (int chunk_x = 0; chunk_x < grid.info.chunks_x; ++chunk_x) {
            ChunkInfo chunk;
            chunk.coord = ChunkCoord{chunk_x, chunk_y};
            chunk.bounds = TileBounds{
                chunk_x * options.chunk_size_x,
                chunk_y * options.chunk_size_y,
                std::min(map.info.width, (chunk_x + 1) * options.chunk_size_x),
                std::min(map.info.height, (chunk_y + 1) * options.chunk_size_y),
            };
            FillChunkStatistics(map, chunk);
            grid.info.blocked_cells += chunk.blocked_cells;
            grid.chunks.push_back(chunk);
        }
    }

    if (!grid.IsValid()) {
        grid.diagnostics.AddWarning("chunk grid validation failed after build");
    }
    return grid;
}

std::string ToLogString(const ChunkGrid& grid)
{
    std::ostringstream out;
    out << "status=" << (grid.IsValid() ? "loaded" : "invalid");
    if (grid.info.map_width > 0 && grid.info.map_height > 0) {
        out << " map=" << grid.info.map_width << 'x' << grid.info.map_height;
    }
    if (grid.info.chunk_size_x > 0 && grid.info.chunk_size_y > 0) {
        out << " chunk_size=" << grid.info.chunk_size_x << 'x' << grid.info.chunk_size_y;
    }
    if (grid.info.chunks_x > 0 && grid.info.chunks_y > 0) {
        out << " chunks=" << grid.info.chunks_x << 'x' << grid.info.chunks_y << " total=" << grid.info.total_chunks;
    }
    if (grid.info.levels.has_value()) {
        out << " levels=" << grid.info.levels->min << ".." << grid.info.levels->max;
    }
    out << " blocked=" << grid.info.blocked_cells;
    if (!grid.diagnostics.warnings.empty()) {
        out << " warnings=" << grid.diagnostics.warnings.size();
    }
    return out.str();
}

}  // namespace vox3d
