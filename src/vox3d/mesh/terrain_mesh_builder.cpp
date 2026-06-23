#include "vox3d/mesh/terrain_mesh_builder.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <sstream>

namespace vox3d {
namespace {

struct TerrainEdge {
    FaceDirection direction = FaceDirection::kWest;
    int dx = 0;
    int dy = 0;
};

constexpr std::array<TerrainEdge, 4> kTerrainEdges{{
    {FaceDirection::kWest, -1, 0},
    {FaceDirection::kEast, 1, 0},
    {FaceDirection::kNorth, 0, -1},
    {FaceDirection::kSouth, 0, 1},
}};

[[nodiscard]] std::uint64_t ExpectedChunkCount(int chunks_x, int chunks_y)
{
    if (chunks_x <= 0 || chunks_y <= 0) {
        return 0;
    }
    return static_cast<std::uint64_t>(chunks_x) * static_cast<std::uint64_t>(chunks_y);
}

[[nodiscard]] std::size_t GridIndex(int x, int y, int width)
{
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
}

[[nodiscard]] bool CanAppendQuad(const ChunkMeshData& mesh)
{
    constexpr std::size_t kQuadVertices = 4;
    constexpr std::size_t kQuadIndices = 6;
    return mesh.vertices.size() <= static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) - kQuadVertices
        && mesh.indices.size() <= static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) - kQuadIndices;
}

[[nodiscard]] bool ContainsTile(const RuntimeMap& map, TileCoord coord)
{
    return coord.x >= 0 && coord.y >= 0 && coord.x < map.info.width && coord.y < map.info.height;
}

[[nodiscard]] BlockTypeId SurfaceBlockType(const RuntimeMap& map, TileCoord tile)
{
    const std::size_t index = GridIndex(tile.x, tile.y, map.info.width);
    return map.collision.cells[index] == 0U ? BlockTypeId::kTerrainSurface : BlockTypeId::kBlockedSurface;
}

[[nodiscard]] int TileHeight(const RuntimeMap& map, TileCoord tile)
{
    return map.height.cells[GridIndex(tile.x, tile.y, map.info.width)];
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

[[nodiscard]] std::array<MeshPosition, 4> TopCorners(int x, int y, float top_z)
{
    const float x0 = static_cast<float>(x);
    const float x1 = static_cast<float>(x + 1);
    const float y0 = static_cast<float>(y);
    const float y1 = static_cast<float>(y + 1);
    return {{{x0, y0, top_z}, {x1, y0, top_z}, {x1, y1, top_z}, {x0, y1, top_z}}};
}

[[nodiscard]] std::array<MeshPosition, 4> WallCorners(
    int x,
    int y,
    float bottom_z,
    float top_z,
    FaceDirection direction)
{
    const float x0 = static_cast<float>(x);
    const float x1 = static_cast<float>(x + 1);
    const float y0 = static_cast<float>(y);
    const float y1 = static_cast<float>(y + 1);

    switch (direction) {
        case FaceDirection::kWest:
            return {{{x0, y0, bottom_z}, {x0, y0, top_z}, {x0, y1, top_z}, {x0, y1, bottom_z}}};
        case FaceDirection::kEast:
            return {{{x1, y1, bottom_z}, {x1, y1, top_z}, {x1, y0, top_z}, {x1, y0, bottom_z}}};
        case FaceDirection::kNorth:
            return {{{x1, y0, bottom_z}, {x1, y0, top_z}, {x0, y0, top_z}, {x0, y0, bottom_z}}};
        case FaceDirection::kSouth:
            return {{{x0, y1, bottom_z}, {x0, y1, top_z}, {x1, y1, top_z}, {x1, y1, bottom_z}}};
        case FaceDirection::kDown:
        case FaceDirection::kUp:
            break;
    }
    return {{{x0, y0, bottom_z}, {x0, y0, top_z}, {x0, y1, top_z}, {x0, y1, bottom_z}}};
}

void EmitTopSurface(
    const RuntimeMap& map,
    TileCoord tile,
    ChunkMeshData& mesh,
    ChunkMeshBuildInfo& info,
    Diagnostics& diagnostics)
{
    if (!CanAppendQuad(mesh)) {
        diagnostics.AddWarning("terrain mesh skipped top faces because uint32 index space is exhausted");
        return;
    }

    const int height = TileHeight(map, tile);
    const float top_z = static_cast<float>(height + 1);
    const BlockTypeId block_type = SurfaceBlockType(map, tile);
    EmitQuad(mesh, BlockCoord{tile.x, tile.y, height}, block_type, FaceDirection::kUp, TopCorners(tile.x, tile.y, top_z));
    ++info.terrain_top_faces;
}

void EmitWallIfNeeded(
    const RuntimeMap& map,
    TileCoord tile,
    const TerrainEdge& edge,
    ChunkMeshData& mesh,
    ChunkMeshBuildInfo& info,
    Diagnostics& diagnostics)
{
    const int height = TileHeight(map, tile);
    const TileCoord neighbor{tile.x + edge.dx, tile.y + edge.dy};

    const int neighbor_height = ContainsTile(map, neighbor)
        ? TileHeight(map, neighbor)
        : (map.info.levels.has_value() ? map.info.levels->min - 1 : height - 1);
    if (neighbor_height >= height) {
        return;
    }
    if (!CanAppendQuad(mesh)) {
        diagnostics.AddWarning("terrain mesh skipped wall faces because uint32 index space is exhausted");
        return;
    }

    const float bottom_z = static_cast<float>(neighbor_height + 1);
    const float top_z = static_cast<float>(height + 1);
    const BlockTypeId block_type = SurfaceBlockType(map, tile);
    EmitQuad(
        mesh,
        BlockCoord{tile.x, tile.y, neighbor_height},
        block_type,
        edge.direction,
        WallCorners(tile.x, tile.y, bottom_z, top_z, edge.direction));
    ++info.terrain_wall_faces;
}

void ReserveTerrainChunkBuffers(const ChunkInfo& chunk, ChunkMeshData& mesh)
{
    if (!chunk.bounds.IsValid()) {
        return;
    }

    constexpr std::size_t kExpectedFacesPerTile = 3;
    const std::size_t expected_faces = chunk.TileCount() * kExpectedFacesPerTile;
    mesh.faces.reserve(expected_faces);
    mesh.vertices.reserve(expected_faces * 4ULL);
    mesh.indices.reserve(expected_faces * 6ULL);
}

void BuildTerrainChunkMesh(
    const RuntimeMap& map,
    const ChunkInfo& chunk,
    ChunkMeshData& mesh,
    ChunkMeshBuildInfo& info,
    Diagnostics& diagnostics)
{
    mesh.coord = chunk.coord;
    mesh.bounds = chunk.bounds;
    ReserveTerrainChunkBuffers(chunk, mesh);

    for (int y = chunk.bounds.min_y; y < chunk.bounds.max_y; ++y) {
        for (int x = chunk.bounds.min_x; x < chunk.bounds.max_x; ++x) {
            const TileCoord tile{x, y};
            EmitTopSurface(map, tile, mesh, info, diagnostics);
            for (const TerrainEdge& edge : kTerrainEdges) {
                EmitWallIfNeeded(map, tile, edge, mesh, info, diagnostics);
            }
        }
    }
}

void CopyMapShape(const RuntimeMap& map, const ChunkGrid& chunks, ChunkMeshBuildInfo& info)
{
    info.mode = ChunkMeshBuildMode::kTerrainSurface;
    info.map_width = map.info.width;
    info.map_height = map.info.height;
    info.chunk_size_x = chunks.info.chunk_size_x;
    info.chunk_size_y = chunks.info.chunk_size_y;
    info.chunks_x = chunks.info.chunks_x;
    info.chunks_y = chunks.info.chunks_y;
    info.total_chunks = chunks.info.total_chunks;
    info.levels = map.info.levels;
    info.solid_blocks = static_cast<std::uint64_t>(map.info.width) * static_cast<std::uint64_t>(map.info.height);
}

void AccumulateChunkStats(const ChunkMeshData& mesh, ChunkMeshBuildInfo& info)
{
    if (!mesh.faces.empty()) {
        ++info.non_empty_chunks;
    }
    info.visible_faces += static_cast<std::uint64_t>(mesh.faces.size());
    info.vertices += static_cast<std::uint64_t>(mesh.vertices.size());
    info.indices += static_cast<std::uint64_t>(mesh.indices.size());
}

}  // namespace

ChunkMeshBuildResult BuildTerrainChunkMeshes(const RuntimeMap& map, const ChunkGrid& chunks)
{
    ChunkMeshBuildResult result;

    if (!map.IsValid()) {
        result.diagnostics.AddWarning("cannot build terrain mesh from invalid runtime map");
        return result;
    }
    if (!map.HasCoreGrids()) {
        result.diagnostics.AddWarning("cannot build terrain mesh without runtime map core grids");
        return result;
    }
    if (!chunks.IsValid()) {
        result.diagnostics.AddWarning("cannot build terrain mesh from invalid chunk grid");
        return result;
    }
    if (map.info.width != chunks.info.map_width || map.info.height != chunks.info.map_height) {
        result.diagnostics.AddWarning("cannot build terrain mesh because runtime map and chunk grid dimensions differ");
        return result;
    }
    if (chunks.info.total_chunks <= 0 || ExpectedChunkCount(chunks.info.chunks_x, chunks.info.chunks_y) != static_cast<std::uint64_t>(chunks.info.total_chunks)) {
        result.diagnostics.AddWarning("cannot build terrain mesh because chunk grid shape is inconsistent");
        return result;
    }

    CopyMapShape(map, chunks, result.info);
    result.chunks.reserve(chunks.chunks.size());

    for (const ChunkInfo& chunk : chunks.chunks) {
        ChunkMeshData mesh;
        BuildTerrainChunkMesh(map, chunk, mesh, result.info, result.diagnostics);
        AccumulateChunkStats(mesh, result.info);
        result.chunks.push_back(std::move(mesh));
    }

    if (!result.IsValid()) {
        result.diagnostics.AddWarning("terrain mesh validation failed after build");
    }
    return result;
}

}  // namespace vox3d
