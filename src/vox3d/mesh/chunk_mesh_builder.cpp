#include "vox3d/mesh/chunk_mesh_builder.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <sstream>

namespace vox3d {
namespace {

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

void EmitFace(ChunkMeshData& mesh, BlockCoord block_coord, const VoxelBlock& block, FaceDirection direction)
{
    const auto corners = FaceCorners(block_coord, direction);
    const auto first_vertex = static_cast<std::uint32_t>(mesh.vertices.size());
    const auto first_index = static_cast<std::uint32_t>(mesh.indices.size());

    MeshFace face;
    face.block = block_coord;
    face.direction = direction;
    face.block_type = block.type;
    face.first_vertex = first_vertex;
    face.first_index = first_index;
    mesh.faces.push_back(face);

    for (const MeshPosition& position : corners) {
        mesh.vertices.push_back(MeshVertex{position, block.type, direction});
    }

    mesh.indices.push_back(first_vertex + 0U);
    mesh.indices.push_back(first_vertex + 1U);
    mesh.indices.push_back(first_vertex + 2U);
    mesh.indices.push_back(first_vertex + 0U);
    mesh.indices.push_back(first_vertex + 2U);
    mesh.indices.push_back(first_vertex + 3U);
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

void BuildChunkMesh(const VoxelWorld& world, const ChunkInfo& chunk, ChunkMeshData& mesh, Diagnostics& diagnostics)
{
    mesh.coord = chunk.coord;
    mesh.bounds = chunk.bounds;
    ReserveChunkBuffers(chunk, mesh);

    for (int y = chunk.bounds.min_y; y < chunk.bounds.max_y; ++y) {
        for (int x = chunk.bounds.min_x; x < chunk.bounds.max_x; ++x) {
            const VoxelColumn* column = world.FindColumn(TileCoord{x, y});
            if (column == nullptr) {
                continue;
            }

            for (int level = column->base_level; level <= column->surface_level; ++level) {
                const BlockCoord block_coord{x, y, level};
                const VoxelBlock block = world.GetBlock(block_coord);
                if (!block.IsSolid()) {
                    continue;
                }

                for (const FaceNeighborOffset& offset : kNeighborOffsets) {
                    const BlockCoord neighbor_coord{
                        block_coord.x + offset.dx,
                        block_coord.y + offset.dy,
                        block_coord.z + offset.dz,
                    };
                    if (world.GetBlock(neighbor_coord).IsSolid()) {
                        continue;
                    }
                    if (!CanAppendQuad(mesh)) {
                        diagnostics.AddWarning("chunk mesh skipped faces because uint32 index space is exhausted");
                        return;
                    }
                    EmitFace(mesh, block_coord, block, offset.direction);
                }
            }
        }
    }
}

void CopyWorldShape(const VoxelWorld& world, ChunkMeshBuildInfo& info)
{
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

}  // namespace

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

ChunkMeshBuildResult BuildChunkMeshes(const VoxelWorld& world, const ChunkGrid& chunks)
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

    CopyWorldShape(world, result.info);
    result.chunks.reserve(chunks.chunks.size());

    for (const ChunkInfo& chunk : chunks.chunks) {
        ChunkMeshData mesh;
        BuildChunkMesh(world, chunk, mesh, result.diagnostics);
        AccumulateMeshStats(mesh, result.info);
        result.chunks.push_back(std::move(mesh));
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

}  // namespace vox3d
