#include "vox3d/mesh/chunk_mesh_builder.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <sstream>
#include <string_view>
#include <vector>

namespace vox3d {
namespace {

struct FaceNeighborOffset {
    FaceDirection direction = FaceDirection::kWest;
    int dx = 0;
    int dy = 0;
    int dz = 0;
};

struct GreedyMaskCell {
    bool visible = false;
    BlockCoord block;
    BlockTypeId block_type = BlockTypeId::kEmpty;
};

struct GreedyRect {
    BlockCoord block;
    FaceDirection direction = FaceDirection::kUp;
    BlockTypeId block_type = BlockTypeId::kEmpty;
    int width = 1;
    int height = 1;
};

constexpr std::array<FaceNeighborOffset, kFaceDirectionCount> kNeighborOffsets{{
    {FaceDirection::kWest, -1, 0, 0},
    {FaceDirection::kEast, 1, 0, 0},
    {FaceDirection::kNorth, 0, -1, 0},
    {FaceDirection::kSouth, 0, 1, 0},
    {FaceDirection::kDown, 0, 0, -1},
    {FaceDirection::kUp, 0, 0, 1},
}};

[[nodiscard]] std::uint64_t ExpectedChunkCount(int chunks_x, int chunks_y)
{
    if (chunks_x <= 0 || chunks_y <= 0) {
        return 0;
    }
    return static_cast<std::uint64_t>(chunks_x) * static_cast<std::uint64_t>(chunks_y);
}

[[nodiscard]] bool CanAppendQuad(const ChunkMeshData& mesh)
{
    constexpr std::size_t kQuadVertices = 4;
    constexpr std::size_t kQuadIndices = 6;
    return mesh.vertices.size() <= static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) - kQuadVertices
        && mesh.indices.size() <= static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) - kQuadIndices;
}

[[nodiscard]] std::array<MeshPosition, 4> FaceCorners(BlockCoord block, FaceDirection direction)
{
    const float x0 = static_cast<float>(block.x);
    const float x1 = static_cast<float>(block.x + 1);
    const float y0 = static_cast<float>(block.y);
    const float y1 = static_cast<float>(block.y + 1);
    const float z0 = static_cast<float>(block.z);
    const float z1 = static_cast<float>(block.z + 1);

    switch (direction) {
        case FaceDirection::kWest:
            return {{{x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0}}};
        case FaceDirection::kEast:
            return {{{x1, y1, z0}, {x1, y1, z1}, {x1, y0, z1}, {x1, y0, z0}}};
        case FaceDirection::kNorth:
            return {{{x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1}, {x0, y0, z0}}};
        case FaceDirection::kSouth:
            return {{{x0, y1, z0}, {x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}}};
        case FaceDirection::kDown:
            return {{{x0, y1, z0}, {x1, y1, z0}, {x1, y0, z0}, {x0, y0, z0}}};
        case FaceDirection::kUp:
            return {{{x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}}};
    }
    return {{{x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}}};
}

[[nodiscard]] std::array<MeshPosition, 4> FaceRectCorners(const GreedyRect& rect)
{
    const float x0 = static_cast<float>(rect.block.x);
    const float x1 = static_cast<float>(rect.block.x + 1);
    const float y0 = static_cast<float>(rect.block.y);
    const float y1 = static_cast<float>(rect.block.y + 1);
    const float z0 = static_cast<float>(rect.block.z);
    const float z1 = static_cast<float>(rect.block.z + 1);
    const float rw = static_cast<float>(rect.width);
    const float rh = static_cast<float>(rect.height);

    switch (rect.direction) {
        case FaceDirection::kWest: {
            const float y_end = y0 + rw;
            const float z_end = z0 + rh;
            return {{{x0, y0, z0}, {x0, y0, z_end}, {x0, y_end, z_end}, {x0, y_end, z0}}};
        }
        case FaceDirection::kEast: {
            const float y_end = y0 + rw;
            const float z_end = z0 + rh;
            return {{{x1, y_end, z0}, {x1, y_end, z_end}, {x1, y0, z_end}, {x1, y0, z0}}};
        }
        case FaceDirection::kNorth: {
            const float x_end = x0 + rw;
            const float z_end = z0 + rh;
            return {{{x_end, y0, z0}, {x_end, y0, z_end}, {x0, y0, z_end}, {x0, y0, z0}}};
        }
        case FaceDirection::kSouth: {
            const float x_end = x0 + rw;
            const float z_end = z0 + rh;
            return {{{x0, y1, z0}, {x0, y1, z_end}, {x_end, y1, z_end}, {x_end, y1, z0}}};
        }
        case FaceDirection::kDown: {
            const float x_end = x0 + rw;
            const float y_end = y0 + rh;
            return {{{x0, y_end, z0}, {x_end, y_end, z0}, {x_end, y0, z0}, {x0, y0, z0}}};
        }
        case FaceDirection::kUp: {
            const float x_end = x0 + rw;
            const float y_end = y0 + rh;
            return {{{x0, y0, z1}, {x_end, y0, z1}, {x_end, y_end, z1}, {x0, y_end, z1}}};
        }
    }
    return {{{x0, y0, z1}, {x0 + rw, y0, z1}, {x0 + rw, y0 + rh, z1}, {x0, y0 + rh, z1}}};
}

void EmitQuad(
    ChunkMeshData& mesh,
    BlockCoord block_coord,
    BlockTypeId block_type,
    FaceDirection direction,
    const std::array<MeshPosition, 4>& corners)
{
    const auto first_vertex = static_cast<std::uint32_t>(mesh.vertices.size());
    const auto first_index = static_cast<std::uint32_t>(mesh.indices.size());

    MeshFace face;
    face.block = block_coord;
    face.direction = direction;
    face.block_type = block_type;
    face.first_vertex = first_vertex;
    face.first_index = first_index;
    mesh.faces.push_back(face);

    for (const MeshPosition& position : corners) {
        mesh.vertices.push_back(MeshVertex{position, block_type, direction});
    }

    mesh.indices.push_back(first_vertex + 0U);
    mesh.indices.push_back(first_vertex + 1U);
    mesh.indices.push_back(first_vertex + 2U);
    mesh.indices.push_back(first_vertex + 0U);
    mesh.indices.push_back(first_vertex + 2U);
    mesh.indices.push_back(first_vertex + 3U);
}

void EmitFace(ChunkMeshData& mesh, BlockCoord block_coord, const VoxelBlock& block, FaceDirection direction)
{
    EmitQuad(mesh, block_coord, block.type, direction, FaceCorners(block_coord, direction));
}

void EmitGreedyRect(ChunkMeshData& mesh, const GreedyRect& rect)
{
    EmitQuad(mesh, rect.block, rect.block_type, rect.direction, FaceRectCorners(rect));
}

void ReserveChunkBuffers(const ChunkInfo& chunk, ChunkMeshData& mesh)
{
    if (!chunk.bounds.IsValid()) {
        return;
    }

    constexpr std::size_t kExpectedFacesPerTile = 8;
    const std::size_t expected_faces = chunk.TileCount() * kExpectedFacesPerTile;
    mesh.faces.reserve(expected_faces);
    mesh.vertices.reserve(expected_faces * 4ULL);
    mesh.indices.reserve(expected_faces * 6ULL);
}

[[nodiscard]] std::optional<VoxelBlock> VisibleBlockFace(
    const VoxelWorld& world,
    BlockCoord block_coord,
    const FaceNeighborOffset& offset)
{
    const VoxelBlock block = world.GetBlock(block_coord);
    if (!block.IsSolid()) {
        return std::nullopt;
    }

    const BlockCoord neighbor_coord{
        block_coord.x + offset.dx,
        block_coord.y + offset.dy,
        block_coord.z + offset.dz,
    };
    if (world.GetBlock(neighbor_coord).IsSolid()) {
        return std::nullopt;
    }

    return block;
}

void BuildSimpleChunkMesh(
    const VoxelWorld& world,
    const ChunkInfo& chunk,
    ChunkMeshData& mesh,
    Diagnostics& diagnostics)
{
    if (!world.info.levels.has_value()) {
        return;
    }

    for (int y = chunk.bounds.min_y; y < chunk.bounds.max_y; ++y) {
        for (int x = chunk.bounds.min_x; x < chunk.bounds.max_x; ++x) {
            const VoxelColumn* column = world.FindColumn(TileCoord{x, y});
            if (column == nullptr) {
                continue;
            }

            for (int level = column->base_level; level <= column->surface_level; ++level) {
                const BlockCoord block_coord{x, y, level};
                for (const FaceNeighborOffset& offset : kNeighborOffsets) {
                    const std::optional<VoxelBlock> block = VisibleBlockFace(world, block_coord, offset);
                    if (!block.has_value()) {
                        continue;
                    }
                    if (!CanAppendQuad(mesh)) {
                        diagnostics.AddWarning("chunk mesh skipped faces because uint32 index space is exhausted");
                        return;
                    }
                    EmitFace(mesh, block_coord, *block, offset.direction);
                }
            }
        }
    }
}

[[nodiscard]] std::size_t MaskIndex(int u, int v, int width)
{
    return static_cast<std::size_t>(v) * static_cast<std::size_t>(width) + static_cast<std::size_t>(u);
}

[[nodiscard]] bool SameMaskType(const GreedyMaskCell& a, const GreedyMaskCell& b)
{
    return a.visible && b.visible && a.block_type == b.block_type;
}

void EmitGreedyMaskRects(
    ChunkMeshData& mesh,
    std::vector<GreedyMaskCell>& mask,
    int width,
    int height,
    FaceDirection direction,
    Diagnostics& diagnostics)
{
    for (int v = 0; v < height; ++v) {
        for (int u = 0; u < width; ++u) {
            GreedyMaskCell& start = mask[MaskIndex(u, v, width)];
            if (!start.visible) {
                continue;
            }

            int rect_width = 1;
            while (u + rect_width < width && SameMaskType(start, mask[MaskIndex(u + rect_width, v, width)])) {
                ++rect_width;
            }

            int rect_height = 1;
            bool can_extend = true;
            while (v + rect_height < height && can_extend) {
                for (int test_u = 0; test_u < rect_width; ++test_u) {
                    if (!SameMaskType(start, mask[MaskIndex(u + test_u, v + rect_height, width)])) {
                        can_extend = false;
                        break;
                    }
                }
                if (can_extend) {
                    ++rect_height;
                }
            }

            if (!CanAppendQuad(mesh)) {
                diagnostics.AddWarning("greedy chunk mesh skipped faces because uint32 index space is exhausted");
                return;
            }

            EmitGreedyRect(mesh, GreedyRect{start.block, direction, start.block_type, rect_width, rect_height});

            for (int clear_v = 0; clear_v < rect_height; ++clear_v) {
                for (int clear_u = 0; clear_u < rect_width; ++clear_u) {
                    mask[MaskIndex(u + clear_u, v + clear_v, width)].visible = false;
                }
            }
        }
    }
}

void BuildVerticalFacePlane(
    const VoxelWorld& world,
    const ChunkInfo& chunk,
    FaceDirection direction,
    int fixed_coord,
    std::vector<GreedyMaskCell>& mask)
{
    const FaceNeighborOffset* selected_offset = nullptr;
    for (const FaceNeighborOffset& offset : kNeighborOffsets) {
        if (offset.direction == direction) {
            selected_offset = &offset;
            break;
        }
    }
    if (selected_offset == nullptr || !chunk.levels.has_value()) {
        return;
    }

    const int level_min = chunk.levels->min;
    const int level_max = chunk.levels->max;
    const int height = level_max - level_min + 1;
    std::fill(mask.begin(), mask.end(), GreedyMaskCell{});

    for (int v = 0; v < height; ++v) {
        const int z = level_min + v;
        for (int u = 0; u < chunk.bounds.Height(); ++u) {
            const int y = chunk.bounds.min_y + u;
            const BlockCoord block_coord{fixed_coord, y, z};
            const std::optional<VoxelBlock> block = VisibleBlockFace(world, block_coord, *selected_offset);
            if (!block.has_value()) {
                continue;
            }
            mask[MaskIndex(u, v, chunk.bounds.Height())] = GreedyMaskCell{true, block_coord, block->type};
        }
    }
}

void BuildDepthFacePlane(
    const VoxelWorld& world,
    const ChunkInfo& chunk,
    FaceDirection direction,
    int fixed_coord,
    std::vector<GreedyMaskCell>& mask)
{
    const FaceNeighborOffset* selected_offset = nullptr;
    for (const FaceNeighborOffset& offset : kNeighborOffsets) {
        if (offset.direction == direction) {
            selected_offset = &offset;
            break;
        }
    }
    if (selected_offset == nullptr || !chunk.levels.has_value()) {
        return;
    }

    const int level_min = chunk.levels->min;
    const int level_max = chunk.levels->max;
    const int height = level_max - level_min + 1;
    std::fill(mask.begin(), mask.end(), GreedyMaskCell{});

    for (int v = 0; v < height; ++v) {
        const int z = level_min + v;
        for (int u = 0; u < chunk.bounds.Width(); ++u) {
            const int x = chunk.bounds.min_x + u;
            const BlockCoord block_coord{x, fixed_coord, z};
            const std::optional<VoxelBlock> block = VisibleBlockFace(world, block_coord, *selected_offset);
            if (!block.has_value()) {
                continue;
            }
            mask[MaskIndex(u, v, chunk.bounds.Width())] = GreedyMaskCell{true, block_coord, block->type};
        }
    }
}

void BuildHorizontalFacePlane(
    const VoxelWorld& world,
    const ChunkInfo& chunk,
    FaceDirection direction,
    int fixed_level,
    std::vector<GreedyMaskCell>& mask)
{
    const FaceNeighborOffset* selected_offset = nullptr;
    for (const FaceNeighborOffset& offset : kNeighborOffsets) {
        if (offset.direction == direction) {
            selected_offset = &offset;
            break;
        }
    }
    if (selected_offset == nullptr) {
        return;
    }

    const int width = chunk.bounds.Width();
    const int height = chunk.bounds.Height();
    std::fill(mask.begin(), mask.end(), GreedyMaskCell{});

    for (int v = 0; v < height; ++v) {
        const int y = chunk.bounds.min_y + v;
        for (int u = 0; u < width; ++u) {
            const int x = chunk.bounds.min_x + u;
            const BlockCoord block_coord{x, y, fixed_level};
            const std::optional<VoxelBlock> block = VisibleBlockFace(world, block_coord, *selected_offset);
            if (!block.has_value()) {
                continue;
            }
            mask[MaskIndex(u, v, width)] = GreedyMaskCell{true, block_coord, block->type};
        }
    }
}

void BuildGreedyChunkMesh(
    const VoxelWorld& world,
    const ChunkInfo& chunk,
    ChunkMeshData& mesh,
    Diagnostics& diagnostics)
{
    if (!chunk.bounds.IsValid() || !chunk.levels.has_value()) {
        return;
    }

    const int level_min = chunk.levels->min;
    const int level_max = chunk.levels->max;
    const int level_count = level_max - level_min + 1;

    std::vector<GreedyMaskCell> vertical_mask(static_cast<std::size_t>(chunk.bounds.Height()) * static_cast<std::size_t>(level_count));
    for (int x = chunk.bounds.min_x; x < chunk.bounds.max_x; ++x) {
        BuildVerticalFacePlane(world, chunk, FaceDirection::kWest, x, vertical_mask);
        EmitGreedyMaskRects(mesh, vertical_mask, chunk.bounds.Height(), level_count, FaceDirection::kWest, diagnostics);
        BuildVerticalFacePlane(world, chunk, FaceDirection::kEast, x, vertical_mask);
        EmitGreedyMaskRects(mesh, vertical_mask, chunk.bounds.Height(), level_count, FaceDirection::kEast, diagnostics);
    }

    std::vector<GreedyMaskCell> depth_mask(static_cast<std::size_t>(chunk.bounds.Width()) * static_cast<std::size_t>(level_count));
    for (int y = chunk.bounds.min_y; y < chunk.bounds.max_y; ++y) {
        BuildDepthFacePlane(world, chunk, FaceDirection::kNorth, y, depth_mask);
        EmitGreedyMaskRects(mesh, depth_mask, chunk.bounds.Width(), level_count, FaceDirection::kNorth, diagnostics);
        BuildDepthFacePlane(world, chunk, FaceDirection::kSouth, y, depth_mask);
        EmitGreedyMaskRects(mesh, depth_mask, chunk.bounds.Width(), level_count, FaceDirection::kSouth, diagnostics);
    }

    std::vector<GreedyMaskCell> horizontal_mask(static_cast<std::size_t>(chunk.bounds.Width()) * static_cast<std::size_t>(chunk.bounds.Height()));
    for (int z = level_min; z <= level_max; ++z) {
        BuildHorizontalFacePlane(world, chunk, FaceDirection::kDown, z, horizontal_mask);
        EmitGreedyMaskRects(mesh, horizontal_mask, chunk.bounds.Width(), chunk.bounds.Height(), FaceDirection::kDown, diagnostics);
        BuildHorizontalFacePlane(world, chunk, FaceDirection::kUp, z, horizontal_mask);
        EmitGreedyMaskRects(mesh, horizontal_mask, chunk.bounds.Width(), chunk.bounds.Height(), FaceDirection::kUp, diagnostics);
    }
}

void BuildChunkMesh(
    const VoxelWorld& world,
    const ChunkInfo& chunk,
    ChunkMeshBuildMode mode,
    ChunkMeshData& mesh,
    Diagnostics& diagnostics)
{
    mesh.coord = chunk.coord;
    mesh.bounds = chunk.bounds;
    ReserveChunkBuffers(chunk, mesh);

    switch (mode) {
        case ChunkMeshBuildMode::kSimpleFaces:
            BuildSimpleChunkMesh(world, chunk, mesh, diagnostics);
            break;
        case ChunkMeshBuildMode::kGreedyFaces:
            BuildGreedyChunkMesh(world, chunk, mesh, diagnostics);
            break;
    }
}

void CopyWorldShape(const VoxelWorld& world, ChunkMeshBuildMode mode, ChunkMeshBuildInfo& info)
{
    info.mode = mode;
    info.map_width = world.info.map_width;
    info.map_height = world.info.map_height;
    info.chunk_size_x = world.info.chunk_size_x;
    info.chunk_size_y = world.info.chunk_size_y;
    info.chunks_x = world.info.chunks_x;
    info.chunks_y = world.info.chunks_y;
    info.total_chunks = world.info.total_chunks;
    info.levels = world.info.levels;
    info.solid_blocks = world.info.solid_blocks;
}

void AccumulateMeshStats(const ChunkMeshData& mesh, ChunkMeshBuildInfo& info)
{
    if (!mesh.faces.empty()) {
        ++info.non_empty_chunks;
    }
    info.visible_faces += static_cast<std::uint64_t>(mesh.faces.size());
    info.vertices += static_cast<std::uint64_t>(mesh.vertices.size());
    info.indices += static_cast<std::uint64_t>(mesh.indices.size());
}

[[nodiscard]] double ReductionRatio(std::uint64_t before, std::uint64_t after)
{
    if (before == 0 || after >= before) {
        return 0.0;
    }
    return static_cast<double>(before - after) / static_cast<double>(before);
}

void AppendPercent(std::ostringstream& out, double ratio)
{
    out.setf(std::ios::fixed);
    out.precision(1);
    out << ratio * 100.0 << '%';
}

}  // namespace

std::string_view ToString(ChunkMeshBuildMode mode)
{
    switch (mode) {
        case ChunkMeshBuildMode::kSimpleFaces:
            return "simple";
        case ChunkMeshBuildMode::kGreedyFaces:
            return "greedy";
    }
    return "unknown";
}

bool ChunkMeshBuildChunkResult::IsValid() const
{
    return mesh.IsValid();
}

bool ChunkMeshData::IsValid() const
{
    return bounds.IsValid() && vertices.size() == faces.size() * 4ULL && indices.size() == faces.size() * 6ULL;
}

std::uint64_t ChunkMeshData::FaceCount() const
{
    return static_cast<std::uint64_t>(faces.size());
}

bool ChunkMeshBuildInfo::IsValid() const
{
    return map_width > 0 && map_height > 0 && chunks_x > 0 && chunks_y > 0
        && static_cast<std::uint64_t>(total_chunks) == ExpectedChunkCount(chunks_x, chunks_y) && levels.has_value()
        && levels->max >= levels->min && vertices == visible_faces * 4ULL && indices == visible_faces * 6ULL
        && non_empty_chunks <= static_cast<std::uint64_t>(total_chunks);
}

bool ChunkMeshBuildResult::IsValid() const
{
    return info.IsValid() && chunks.size() == static_cast<std::size_t>(info.total_chunks)
        && std::all_of(chunks.begin(), chunks.end(), [](const ChunkMeshData& mesh) {
               return mesh.IsValid();
           });
}

double MeshOptimizationStats::FaceCullingReductionRatio() const
{
    return ReductionRatio(naive_faces, simple_faces);
}

double MeshOptimizationStats::GreedyReductionRatio() const
{
    return ReductionRatio(simple_faces, greedy_faces);
}

double MeshOptimizationStats::ActiveReductionRatio() const
{
    return ReductionRatio(naive_faces, active_faces);
}

ChunkMeshBuildChunkResult BuildChunkMeshForChunk(
    const VoxelWorld& world,
    const ChunkInfo& chunk,
    ChunkMeshBuildMode mode)
{
    ChunkMeshBuildChunkResult result;

    if (!world.IsValid()) {
        result.diagnostics.AddWarning("cannot build chunk mesh from invalid voxel world");
        return result;
    }
    if (!chunk.IsValid()) {
        result.diagnostics.AddWarning("cannot build chunk mesh from invalid chunk");
        return result;
    }

    BuildChunkMesh(world, chunk, mode, result.mesh, result.diagnostics);
    if (!result.IsValid()) {
        result.diagnostics.AddWarning("single chunk mesh validation failed after build");
    }
    return result;
}

ChunkMeshBuildResult BuildChunkMeshes(const VoxelWorld& world, const ChunkGrid& chunks, ChunkMeshBuildMode mode)
{
    ChunkMeshBuildResult result;

    if (!world.IsValid()) {
        result.diagnostics.AddWarning("cannot build chunk meshes from invalid voxel world");
        return result;
    }
    if (!chunks.IsValid()) {
        result.diagnostics.AddWarning("cannot build chunk meshes from invalid chunk grid");
        return result;
    }
    if (world.info.total_chunks != chunks.info.total_chunks || world.info.chunks_x != chunks.info.chunks_x
        || world.info.chunks_y != chunks.info.chunks_y) {
        result.diagnostics.AddWarning("cannot build chunk meshes because voxel world and chunk grid dimensions differ");
        return result;
    }

    CopyWorldShape(world, mode, result.info);
    result.chunks.reserve(chunks.chunks.size());

    for (const ChunkInfo& chunk : chunks.chunks) {
        ChunkMeshBuildChunkResult chunk_result = BuildChunkMeshForChunk(world, chunk, mode);
        for (const auto& warning : chunk_result.diagnostics.warnings) {
            result.diagnostics.AddWarning(warning);
        }
        AccumulateMeshStats(chunk_result.mesh, result.info);
        result.chunks.push_back(std::move(chunk_result.mesh));
    }

    if (!result.IsValid()) {
        result.diagnostics.AddWarning("chunk mesh validation failed after build");
    }
    return result;
}

std::string ToLogString(const ChunkMeshBuildResult& result)
{
    std::ostringstream out;
    out << "status=" << (result.IsValid() ? "loaded" : "invalid");
    out << " mode=" << ToString(result.info.mode);
    if (result.info.map_width > 0 && result.info.map_height > 0) {
        out << " map=" << result.info.map_width << 'x' << result.info.map_height;
    }
    if (result.info.levels.has_value()) {
        out << " levels=" << result.info.levels->min << ".." << result.info.levels->max;
    }
    if (result.info.chunks_x > 0 && result.info.chunks_y > 0) {
        out << " chunks=" << result.info.chunks_x << 'x' << result.info.chunks_y << " total=" << result.info.total_chunks;
    }
    out << " mesh_chunks=" << result.chunks.size();
    out << " non_empty=" << result.info.non_empty_chunks;
    out << " faces=" << result.info.visible_faces;
    out << " vertices=" << result.info.vertices;
    out << " indices=" << result.info.indices;
    out << " solid=" << result.info.solid_blocks;
    if (!result.diagnostics.warnings.empty()) {
        out << " warnings=" << result.diagnostics.warnings.size();
    }
    return out.str();
}

std::string ToLogString(const MeshOptimizationStats& stats)
{
    std::ostringstream out;
    out << "mode=" << ToString(stats.active_mode);
    out << " solid=" << stats.solid_blocks;
    out << " naive_faces=" << stats.naive_faces;
    out << " culled_faces=" << stats.culled_faces;
    out << " simple_faces=" << stats.simple_faces;
    out << " greedy_faces=" << stats.greedy_faces;
    out << " active_faces=" << stats.active_faces;
    out << " vertices=" << stats.active_vertices;
    out << " indices=" << stats.active_indices;
    out << " mesh_chunks=" << stats.mesh_chunks;
    out << " draw_models=" << stats.draw_models;
    if (stats.skipped_chunks > 0) {
        out << " skipped_chunks=" << stats.skipped_chunks;
    }
    out << " culling_saved=";
    AppendPercent(out, stats.FaceCullingReductionRatio());
    out << " greedy_saved=";
    AppendPercent(out, stats.GreedyReductionRatio());
    out << " total_saved=";
    AppendPercent(out, stats.ActiveReductionRatio());
    return out.str();
}

}  // namespace vox3d
