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
    LevelRange levels;
    if (map.info.levels.has_value()) {
        levels = *map.info.levels;
    } else {
        if (!map.height.IsValid()) {
            return LevelRange{};
        }

        const auto [min_it, max_it] = std::minmax_element(map.height.cells.begin(), map.height.cells.end());
        if (min_it == map.height.cells.end() || max_it == map.height.cells.end()) {
            return LevelRange{};
        }
        levels = LevelRange{*min_it, *max_it};
    }

    if (map.structure_height.IsValid() && map.structure_height.cells.size() == map.height.cells.size()) {
        for (std::size_t index = 0; index < map.height.cells.size(); ++index) {
            levels.max = std::max(
                levels.max,
                map.height.cells[index] + static_cast<int>(map.structure_height.cells[index]));
        }
    }
    return levels;
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
        case BlockTypeId::kRuinStructure:
            return "ruin_structure";
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
    if (column->structure_height > 0U && coord.z > column->ground_surface_level) {
        block.type = BlockTypeId::kRuinStructure;
    } else if (coord.z == column->ground_surface_level) {
        block.type = column->surface_block_type;
    } else {
        block.type = BlockTypeId::kSubsurface;
    }
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
            const int raw_ground_surface_level = map.height.cells[index];
            const std::uint8_t structure_height = map.structure_height.IsValid()
                ? map.structure_height.cells[index]
                : 0U;
            const int raw_surface_level = raw_ground_surface_level + static_cast<int>(structure_height);
            const int ground_surface_level = ClampLevel(raw_ground_surface_level, levels);
            const int surface_level = ClampLevel(raw_surface_level, levels);
            if (ground_surface_level != raw_ground_surface_level || surface_level != raw_surface_level) {
                ++clamped_surface_levels;
            }

            VoxelColumn column;
            column.tile = TileCoord{x, y};
            column.base_level = levels.min;
            column.ground_surface_level = ground_surface_level;
            column.surface_level = surface_level;
            column.structure_height = static_cast<std::uint8_t>(
                std::max(0, surface_level - ground_surface_level));
            column.terrain = map.terrain.cells[index];
            column.blocked = blocked;
            column.surface_block_type = SurfaceBlockType(blocked);

            world.info.solid_blocks += column.SolidBlockCount();
            world.info.empty_blocks += vertical_levels - column.SolidBlockCount();
            world.info.structure_blocks += column.structure_height;
            if (blocked) {
                ++world.info.blocked_columns;
            }
            if (column.structure_height > 0U) {
                ++world.info.structure_columns;
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
    out << " structure_columns=" << world.info.structure_columns;
    out << " structure_blocks=" << world.info.structure_blocks;
    if (world.info.chunks_x > 0 && world.info.chunks_y > 0) {
        out << " chunks=" << world.info.chunks_x << 'x' << world.info.chunks_y << " total=" << world.info.total_chunks;
    }
    if (!world.diagnostics.warnings.empty()) {
        out << " warnings=" << world.diagnostics.warnings.size();
    }
    return out.str();
}

}  // namespace vox3d
