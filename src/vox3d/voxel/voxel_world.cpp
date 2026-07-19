#include "vox3d/voxel/voxel_world.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

namespace vox3d {
namespace {

[[nodiscard]] std::size_t GridIndex(int x, int y, int width)
{
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
}

[[nodiscard]] std::uint64_t ExpectedColumnCount(int width, int height)
{
    if (width <= 0 || height <= 0) {
        return 0;
    }
    return static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
}

[[nodiscard]] std::uint64_t VerticalLevelCount(LevelRange levels)
{
    if (levels.max < levels.min) {
        return 0;
    }
    return static_cast<std::uint64_t>(levels.max - levels.min + 1);
}

[[nodiscard]] LevelRange ResolveLevels(const RuntimeMap& map)
{
    if (map.info.levels.has_value()) {
        return *map.info.levels;
    }

    if (!map.height.IsValid()) {
        return LevelRange{};
    }

    const auto [min_it, max_it] = std::minmax_element(map.height.cells.begin(), map.height.cells.end());
    if (min_it == map.height.cells.end() || max_it == map.height.cells.end()) {
        return LevelRange{};
    }
    return LevelRange{*min_it, *max_it};
}

[[nodiscard]] bool HasValidLevels(LevelRange levels)
{
    return levels.max >= levels.min;
}

[[nodiscard]] int ClampLevel(int level, LevelRange levels)
{
    return std::clamp(level, levels.min, levels.max);
}

[[nodiscard]] BlockTypeId SurfaceBlockType(bool blocked)
{
    return blocked ? BlockTypeId::kBlockedSurface : BlockTypeId::kTerrainSurface;
}

}  // namespace

bool VoxelBlock::IsSolid() const
{
    return solid;
}

std::string_view ToString(BlockTypeId type)
{
    switch (type) {
        case BlockTypeId::kEmpty:
            return "empty";
        case BlockTypeId::kSubsurface:
            return "subsurface";
        case BlockTypeId::kTerrainSurface:
            return "terrain_surface";
        case BlockTypeId::kBlockedSurface:
            return "blocked_surface";
    }
    return "unknown";
}

std::uint64_t VoxelColumn::SolidBlockCount() const
{
    if (surface_level < base_level) {
        return 0;
    }
    return static_cast<std::uint64_t>(surface_level - base_level + 1);
}

bool VoxelColumn::ContainsSolidLevel(int level) const
{
    return level >= base_level && level <= surface_level;
}

bool VoxelWorldInfo::IsValid() const
{
    return map_width > 0 && map_height > 0 && levels.has_value() && levels->max >= levels->min
        && total_columns == ExpectedColumnCount(map_width, map_height);
}

bool VoxelWorld::IsValid() const
{
    return info.IsValid() && columns.size() == static_cast<std::size_t>(info.total_columns);
}

bool VoxelWorld::ContainsTile(TileCoord coord) const
{
    return coord.x >= 0 && coord.y >= 0 && coord.x < info.map_width && coord.y < info.map_height;
}

const VoxelColumn* VoxelWorld::FindColumn(TileCoord coord) const
{
    if (!ContainsTile(coord)) {
        return nullptr;
    }
    const std::size_t index = GridIndex(coord.x, coord.y, info.map_width);
    if (index >= columns.size()) {
        return nullptr;
    }
    return &columns[index];
}

VoxelBlock VoxelWorld::GetBlock(BlockCoord coord) const
{
    VoxelBlock block;
    block.coord = coord;

    const VoxelColumn* column = FindColumn(TileCoord{coord.x, coord.y});
    if (column == nullptr || !info.levels.has_value() || coord.z < info.levels->min || coord.z > info.levels->max) {
        return block;
    }

    if (!column->ContainsSolidLevel(coord.z)) {
        return block;
    }

    block.solid = true;
    block.blocked = column->blocked;
    block.destructible = true;
    block.type = coord.z == column->surface_level ? column->surface_block_type : BlockTypeId::kSubsurface;
    return block;
}

VoxelWorld BuildVoxelWorld(const RuntimeMap& map, const ChunkGrid& chunks)
{
    VoxelWorld world;

    if (!map.IsValid()) {
        world.diagnostics.AddWarning("cannot build voxel world from invalid runtime map");
        return world;
    }
    if (!map.HasCoreGrids()) {
        world.diagnostics.AddWarning("cannot build complete voxel world without runtime map core grids");
        return world;
    }

    const LevelRange levels = ResolveLevels(map);
    if (!HasValidLevels(levels)) {
        world.diagnostics.AddWarning("cannot build voxel world without valid level range");
        return world;
    }

    if (!chunks.IsValid()) {
        world.diagnostics.AddWarning("voxel world built without valid chunk grid");
    }

    world.info.map_width = map.info.width;
    world.info.map_height = map.info.height;
    world.info.levels = levels;
    world.info.total_columns = ExpectedColumnCount(map.info.width, map.info.height);

    if (chunks.info.IsValid()) {
        world.info.chunk_size_x = chunks.info.chunk_size_x;
        world.info.chunk_size_y = chunks.info.chunk_size_y;
        world.info.chunks_x = chunks.info.chunks_x;
        world.info.chunks_y = chunks.info.chunks_y;
        world.info.total_chunks = chunks.info.total_chunks;
    }

    const std::uint64_t vertical_levels = VerticalLevelCount(levels);
    int clamped_surface_levels = 0;

    world.columns.reserve(static_cast<std::size_t>(world.info.total_columns));
    for (int y = 0; y < map.info.height; ++y) {
        for (int x = 0; x < map.info.width; ++x) {
            const std::size_t index = GridIndex(x, y, map.info.width);
            const bool blocked = map.collision.cells[index] != 0;
            const int raw_surface_level = map.height.cells[index];
            const int surface_level = ClampLevel(raw_surface_level, levels);
            if (surface_level != raw_surface_level) {
                ++clamped_surface_levels;
            }

            VoxelColumn column;
            column.tile = TileCoord{x, y};
            column.base_level = levels.min;
            column.surface_level = surface_level;
            column.blocked = blocked;
            column.surface_block_type = SurfaceBlockType(blocked);

            world.info.solid_blocks += column.SolidBlockCount();
            world.info.empty_blocks += vertical_levels - column.SolidBlockCount();
            if (blocked) {
                ++world.info.blocked_columns;
            }
            world.columns.push_back(std::move(column));
        }
    }

    if (clamped_surface_levels > 0) {
        world.diagnostics.AddWarning("voxel world clamped surface levels count=" + std::to_string(clamped_surface_levels));
    }
    if (!world.IsValid()) {
        world.diagnostics.AddWarning("voxel world validation failed after build");
    }

    return world;
}

std::string ToLogString(const VoxelWorld& world)
{
    std::ostringstream out;
    out << "status=" << (world.IsValid() ? "loaded" : "invalid");
    if (world.info.map_width > 0 && world.info.map_height > 0) {
        out << " map=" << world.info.map_width << 'x' << world.info.map_height;
    }
    if (world.info.levels.has_value()) {
        out << " levels=" << world.info.levels->min << ".." << world.info.levels->max;
    }
    out << " columns=" << world.info.total_columns;
    out << " solid=" << world.info.solid_blocks;
    out << " empty=" << world.info.empty_blocks;
    out << " blocked_columns=" << world.info.blocked_columns;
    if (world.info.chunks_x > 0 && world.info.chunks_y > 0) {
        out << " chunks=" << world.info.chunks_x << 'x' << world.info.chunks_y << " total=" << world.info.total_chunks;
    }
    if (!world.diagnostics.warnings.empty()) {
        out << " warnings=" << world.diagnostics.warnings.size();
    }
    return out.str();
}

}  // namespace vox3d
