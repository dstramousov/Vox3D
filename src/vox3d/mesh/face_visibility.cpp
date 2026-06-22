#include "vox3d/mesh/face_visibility.hpp"

#include <array>
#include <iomanip>
#include <sstream>
#include <string>

namespace vox3d {
namespace {

[[nodiscard]] std::size_t FaceDirectionIndex(FaceDirection direction)
{
    return static_cast<std::size_t>(direction);
}

struct FaceNeighborOffset {
    FaceDirection direction = FaceDirection::kWest;
    int dx = 0;
    int dy = 0;
    int dz = 0;
};

constexpr std::array<FaceNeighborOffset, kFaceDirectionCount> kNeighborOffsets{{
    {FaceDirection::kWest, -1, 0, 0},
    {FaceDirection::kEast, 1, 0, 0},
    {FaceDirection::kNorth, 0, -1, 0},
    {FaceDirection::kSouth, 0, 1, 0},
    {FaceDirection::kDown, 0, 0, -1},
    {FaceDirection::kUp, 0, 0, 1},
}};

[[nodiscard]] std::uint64_t SafeNaiveFaceCount(std::uint64_t solid_blocks)
{
    constexpr std::uint64_t kFacesPerBlock = 6;
    return solid_blocks * kFacesPerBlock;
}

[[nodiscard]] std::uint64_t SumVisibleDirections(const FaceDirectionCounts& counts)
{
    std::uint64_t total = 0;
    for (const std::uint64_t value : counts.values) {
        total += value;
    }
    return total;
}

void CopyWorldShape(const VoxelWorldInfo& source, FaceVisibilityInfo& target)
{
    target.map_width = source.map_width;
    target.map_height = source.map_height;
    target.chunk_size_x = source.chunk_size_x;
    target.chunk_size_y = source.chunk_size_y;
    target.chunks_x = source.chunks_x;
    target.chunks_y = source.chunks_y;
    target.total_chunks = source.total_chunks;
    target.levels = source.levels;
    target.solid_blocks = source.solid_blocks;
    target.naive_faces = SafeNaiveFaceCount(source.solid_blocks);
}

void CountBlockFaces(const VoxelWorld& world, BlockCoord coord, FaceVisibilityInfo& info)
{
    for (const FaceNeighborOffset& offset : kNeighborOffsets) {
        const BlockCoord neighbor_coord{coord.x + offset.dx, coord.y + offset.dy, coord.z + offset.dz};
        const VoxelBlock neighbor = world.GetBlock(neighbor_coord);
        if (neighbor.IsSolid()) {
            ++info.culled_faces;
            continue;
        }

        ++info.visible_faces;
        info.visible_by_direction.Increment(offset.direction);
    }
}

}  // namespace

std::uint64_t FaceDirectionCounts::Get(FaceDirection direction) const
{
    const std::size_t index = FaceDirectionIndex(direction);
    if (index >= values.size()) {
        return 0;
    }
    return values[index];
}

void FaceDirectionCounts::Increment(FaceDirection direction)
{
    const std::size_t index = FaceDirectionIndex(direction);
    if (index >= values.size()) {
        return;
    }
    ++values[index];
}

std::string_view ToString(FaceDirection direction)
{
    switch (direction) {
        case FaceDirection::kWest:
            return "west";
        case FaceDirection::kEast:
            return "east";
        case FaceDirection::kNorth:
            return "north";
        case FaceDirection::kSouth:
            return "south";
        case FaceDirection::kDown:
            return "down";
        case FaceDirection::kUp:
            return "up";
    }
    return "unknown";
}

bool FaceVisibilityInfo::IsValid() const
{
    return map_width > 0 && map_height > 0 && levels.has_value() && levels->max >= levels->min
        && naive_faces == visible_faces + culled_faces && visible_faces == SumVisibleDirections(visible_by_direction);
}

double FaceVisibilityInfo::CullRatio() const
{
    if (naive_faces == 0) {
        return 0.0;
    }
    return static_cast<double>(culled_faces) / static_cast<double>(naive_faces);
}

bool FaceVisibilityResult::IsValid() const
{
    return info.IsValid();
}

FaceVisibilityResult BuildFaceVisibility(const VoxelWorld& world)
{
    FaceVisibilityResult result;

    if (!world.IsValid()) {
        result.diagnostics.AddWarning("cannot build face visibility from invalid voxel world");
        return result;
    }
    if (!world.info.levels.has_value()) {
        result.diagnostics.AddWarning("cannot build face visibility without valid level range");
        return result;
    }

    CopyWorldShape(world.info, result.info);

    for (const VoxelColumn& column : world.columns) {
        for (int level = column.base_level; level <= column.surface_level; ++level) {
            CountBlockFaces(world, BlockCoord{column.tile.x, column.tile.y, level}, result.info);
        }
    }

    if (!result.IsValid()) {
        result.diagnostics.AddWarning("face visibility validation failed after build");
    }

    return result;
}

std::string ToLogString(const FaceVisibilityResult& result)
{
    std::ostringstream out;
    out << "status=" << (result.IsValid() ? "loaded" : "invalid");
    if (result.info.map_width > 0 && result.info.map_height > 0) {
        out << " map=" << result.info.map_width << 'x' << result.info.map_height;
    }
    if (result.info.levels.has_value()) {
        out << " levels=" << result.info.levels->min << ".." << result.info.levels->max;
    }
    out << " solid=" << result.info.solid_blocks;
    out << " naive_faces=" << result.info.naive_faces;
    out << " visible=" << result.info.visible_faces;
    out << " culled=" << result.info.culled_faces;
    out << " cull_ratio=" << std::fixed << std::setprecision(1) << result.info.CullRatio() * 100.0 << '%';
    out << " top=" << result.info.visible_by_direction.Get(FaceDirection::kUp);
    out << " bottom=" << result.info.visible_by_direction.Get(FaceDirection::kDown);
    out << " west=" << result.info.visible_by_direction.Get(FaceDirection::kWest);
    out << " east=" << result.info.visible_by_direction.Get(FaceDirection::kEast);
    out << " north=" << result.info.visible_by_direction.Get(FaceDirection::kNorth);
    out << " south=" << result.info.visible_by_direction.Get(FaceDirection::kSouth);
    if (result.info.chunks_x > 0 && result.info.chunks_y > 0) {
        out << " chunks=" << result.info.chunks_x << 'x' << result.info.chunks_y << " total=" << result.info.total_chunks;
    }
    if (!result.diagnostics.warnings.empty()) {
        out << " warnings=" << result.diagnostics.warnings.size();
    }
    return out.str();
}

}  // namespace vox3d
