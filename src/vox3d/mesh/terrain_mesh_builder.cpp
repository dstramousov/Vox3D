#include "vox3d/mesh/terrain_mesh_builder.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cctype>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace vox3d {
namespace {

struct TerrainEdge {
    FaceDirection direction = FaceDirection::kWest;
    int dx = 0;
    int dy = 0;
};

struct TopMaskCell {
    bool visible = false;
    int height = 0;
    BlockTypeId block_type = BlockTypeId::kEmpty;
    TerrainSurfaceKind surface_kind = TerrainSurfaceKind::kUnknown;
};

struct TopSpan {
    TileCoord tile;
    int width = 1;
    int height_tiles = 1;
    int level = 0;
    BlockTypeId block_type = BlockTypeId::kEmpty;
    TerrainSurfaceKind surface_kind = TerrainSurfaceKind::kUnknown;
};

struct WallCell {
    bool visible = false;
    int bottom_level = 0;
    int top_level = 0;
    BlockTypeId block_type = BlockTypeId::kEmpty;
    TerrainSurfaceKind surface_kind = TerrainSurfaceKind::kUnknown;
};

struct WallSpan {
    TileCoord tile;
    int length = 1;
    int bottom_level = 0;
    int top_level = 0;
    FaceDirection direction = FaceDirection::kWest;
    BlockTypeId block_type = BlockTypeId::kEmpty;
    TerrainSurfaceKind surface_kind = TerrainSurfaceKind::kUnknown;
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

[[nodiscard]] std::size_t MaskIndex(int x, int y, int width)
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

[[nodiscard]] std::string NormalizeToken(std::string_view raw_value)
{
    std::string normalized;
    normalized.reserve(raw_value.size());
    for (unsigned char c : raw_value) {
        if (std::isalnum(c) != 0) {
            normalized.push_back(static_cast<char>(std::tolower(c)));
        }
    }
    return normalized;
}

[[nodiscard]] bool Contains(std::string_view text, std::string_view needle)
{
    return text.find(needle) != std::string_view::npos;
}

[[nodiscard]] bool IsStartOrGoal(const std::optional<TileCoord>& point, TileCoord tile)
{
    return point.has_value() && point->x == tile.x && point->y == tile.y;
}

[[nodiscard]] TerrainSurfaceKind SurfaceKind(const RuntimeMap& map, TileCoord tile)
{
    if (IsStartOrGoal(map.info.start, tile)) {
        return TerrainSurfaceKind::kStart;
    }
    if (IsStartOrGoal(map.info.goal, tile)) {
        return TerrainSurfaceKind::kGoal;
    }

    const std::size_t index = GridIndex(tile.x, tile.y, map.info.width);
    const bool blocked = map.collision.cells[index] != 0U;
    const std::string terrain = map.terrain.IsValid() ? NormalizeToken(map.terrain.cells[index]) : std::string{};

    if (Contains(terrain, "start") || Contains(terrain, "spawn")) {
        return TerrainSurfaceKind::kStart;
    }
    if (Contains(terrain, "goal") || Contains(terrain, "exit")) {
        return TerrainSurfaceKind::kGoal;
    }
    if (Contains(terrain, "water") || Contains(terrain, "wet") || Contains(terrain, "river") || Contains(terrain, "lake")) {
        return TerrainSurfaceKind::kWaterWetTerrain;
    }
    if (Contains(terrain, "tree") || Contains(terrain, "forest") || Contains(terrain, "woods")) {
        return TerrainSurfaceKind::kTreeBlocker;
    }
    if (Contains(terrain, "structure") || Contains(terrain, "depth")) {
        return TerrainSurfaceKind::kStructuralDepth;
    }
    if (Contains(terrain, "bush") || Contains(terrain, "slow") || Contains(terrain, "swamp") || Contains(terrain, "marsh")) {
        return TerrainSurfaceKind::kWalkableSlow;
    }
    if (blocked) {
        return TerrainSurfaceKind::kBlockedTerrain;
    }
    return TerrainSurfaceKind::kWalkableGround;
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

[[nodiscard]] int StructureHeight(const RuntimeMap& map, TileCoord tile)
{
    if (!map.structure_height.IsValid() || !map.structure_height.Contains(tile)) {
        return 0;
    }
    return static_cast<int>(map.structure_height.cells[GridIndex(tile.x, tile.y, map.info.width)]);
}

[[nodiscard]] int TileSolidTopLevel(const RuntimeMap& map, TileCoord tile)
{
    return TileHeight(map, tile) + StructureHeight(map, tile);
}

[[nodiscard]] bool SameTopMaskType(const TopMaskCell& a, const TopMaskCell& b)
{
    return a.visible && b.visible && a.height == b.height && a.block_type == b.block_type
        && a.surface_kind == b.surface_kind;
}

[[nodiscard]] bool SameWallMaskType(const WallCell& a, const WallCell& b)
{
    return a.visible && b.visible && a.bottom_level == b.bottom_level && a.top_level == b.top_level
        && a.block_type == b.block_type && a.surface_kind == b.surface_kind;
}

void EmitQuad(
    ChunkMeshData& mesh,
    BlockCoord block_coord,
    BlockTypeId block_type,
    FaceDirection direction,
    TerrainRenderPass terrain_pass,
    TerrainSurfaceKind surface_kind,
    const std::array<MeshPosition, 4>& corners)
{
    const auto first_vertex = static_cast<std::uint32_t>(mesh.vertices.size());
    const auto first_index = static_cast<std::uint32_t>(mesh.indices.size());

    MeshFace face;
    face.block = block_coord;
    face.direction = direction;
    face.block_type = block_type;
    face.terrain_pass = terrain_pass;
    face.surface_kind = surface_kind;
    face.first_vertex = first_vertex;
    face.first_index = first_index;
    mesh.faces.push_back(face);

    for (const MeshPosition& position : corners) {
        mesh.vertices.push_back(MeshVertex{position, block_type, direction, terrain_pass, surface_kind, block_coord.z});
    }

    mesh.indices.push_back(first_vertex + 0U);
    mesh.indices.push_back(first_vertex + 1U);
    mesh.indices.push_back(first_vertex + 2U);
    mesh.indices.push_back(first_vertex + 0U);
    mesh.indices.push_back(first_vertex + 2U);
    mesh.indices.push_back(first_vertex + 3U);
}

[[nodiscard]] std::array<MeshPosition, 4> TopSpanCorners(const TopSpan& span)
{
    const float x0 = static_cast<float>(span.tile.x);
    const float x1 = static_cast<float>(span.tile.x + span.width);
    const float y0 = static_cast<float>(span.tile.y);
    const float y1 = static_cast<float>(span.tile.y + span.height_tiles);
    const float z = static_cast<float>(span.level + 1);
    return {{{x0, y0, z}, {x1, y0, z}, {x1, y1, z}, {x0, y1, z}}};
}

[[nodiscard]] std::array<MeshPosition, 4> WallSpanCorners(const WallSpan& span)
{
    const float x0 = static_cast<float>(span.tile.x);
    const float x1 = static_cast<float>(span.tile.x + 1);
    const float y0 = static_cast<float>(span.tile.y);
    const float y1 = static_cast<float>(span.tile.y + 1);
    const float bottom_z = static_cast<float>(span.bottom_level + 1);
    const float top_z = static_cast<float>(span.top_level + 1);

    switch (span.direction) {
        case FaceDirection::kWest: {
            const float y_end = y0 + static_cast<float>(span.length);
            return {{{x0, y0, bottom_z}, {x0, y0, top_z}, {x0, y_end, top_z}, {x0, y_end, bottom_z}}};
        }
        case FaceDirection::kEast: {
            const float y_end = y0 + static_cast<float>(span.length);
            return {{{x1, y_end, bottom_z}, {x1, y_end, top_z}, {x1, y0, top_z}, {x1, y0, bottom_z}}};
        }
        case FaceDirection::kNorth: {
            const float x_end = x0 + static_cast<float>(span.length);
            return {{{x_end, y0, bottom_z}, {x_end, y0, top_z}, {x0, y0, top_z}, {x0, y0, bottom_z}}};
        }
        case FaceDirection::kSouth: {
            const float x_end = x0 + static_cast<float>(span.length);
            return {{{x0, y1, bottom_z}, {x0, y1, top_z}, {x_end, y1, top_z}, {x_end, y1, bottom_z}}};
        }
        case FaceDirection::kDown:
        case FaceDirection::kUp:
            break;
    }
    return {{{x0, y0, bottom_z}, {x0, y0, top_z}, {x0, y1, top_z}, {x0, y1, bottom_z}}};
}

void EmitTopSpan(ChunkMeshData& mesh, const TopSpan& span, ChunkMeshBuildInfo& info, Diagnostics& diagnostics)
{
    if (!CanAppendQuad(mesh)) {
        diagnostics.AddWarning("terrain mesh skipped top spans because uint32 index space is exhausted");
        return;
    }

    EmitQuad(
        mesh,
        BlockCoord{span.tile.x, span.tile.y, span.level},
        span.block_type,
        FaceDirection::kUp,
        TerrainRenderPass::kTops,
        span.surface_kind,
        TopSpanCorners(span));
    ++info.terrain_top_faces;
}

void EmitWallSpan(ChunkMeshData& mesh, const WallSpan& span, ChunkMeshBuildInfo& info, Diagnostics& diagnostics)
{
    if (!CanAppendQuad(mesh)) {
        diagnostics.AddWarning("terrain mesh skipped wall spans because uint32 index space is exhausted");
        return;
    }

    const TerrainRenderPass terrain_pass = span.top_level - span.bottom_level > 1
        ? TerrainRenderPass::kCliffs
        : TerrainRenderPass::kWalls;
    EmitQuad(
        mesh,
        BlockCoord{span.tile.x, span.tile.y, span.bottom_level},
        span.block_type,
        span.direction,
        terrain_pass,
        span.surface_kind,
        WallSpanCorners(span));
    if (terrain_pass == TerrainRenderPass::kCliffs) {
        ++info.terrain_cliff_faces;
    } else {
        ++info.terrain_wall_faces;
    }
}

void ReserveTerrainChunkBuffers(const ChunkInfo& chunk, ChunkMeshData& mesh)
{
    if (!chunk.bounds.IsValid()) {
        return;
    }

    constexpr std::size_t kEstimatedFacesPerTile = 3;
    const std::size_t expected_faces = chunk.TileCount() * kEstimatedFacesPerTile;
    mesh.faces.reserve(expected_faces);
    mesh.vertices.reserve(expected_faces * 4ULL);
    mesh.indices.reserve(expected_faces * 6ULL);
}

void EmitStructureTop(
    ChunkMeshData& mesh,
    TileCoord tile,
    int top_level,
    ChunkMeshBuildInfo& info,
    Diagnostics& diagnostics)
{
    if (!CanAppendQuad(mesh)) {
        diagnostics.AddWarning("terrain mesh skipped ruin structure tops because uint32 index space is exhausted");
        return;
    }

    const TopSpan span{
        tile,
        1,
        1,
        top_level,
        BlockTypeId::kRuinStructure,
        TerrainSurfaceKind::kUnknown,
    };
    EmitQuad(
        mesh,
        BlockCoord{tile.x, tile.y, top_level},
        BlockTypeId::kRuinStructure,
        FaceDirection::kUp,
        TerrainRenderPass::kTops,
        TerrainSurfaceKind::kUnknown,
        TopSpanCorners(span));
    ++info.structure_top_faces;
}

void EmitStructureWall(
    ChunkMeshData& mesh,
    TileCoord tile,
    FaceDirection direction,
    int bottom_level,
    int top_level,
    ChunkMeshBuildInfo& info,
    Diagnostics& diagnostics)
{
    if (bottom_level >= top_level) {
        return;
    }
    if (!CanAppendQuad(mesh)) {
        diagnostics.AddWarning("terrain mesh skipped ruin structure walls because uint32 index space is exhausted");
        return;
    }

    const WallSpan span{
        tile,
        1,
        bottom_level,
        top_level,
        direction,
        BlockTypeId::kRuinStructure,
        TerrainSurfaceKind::kUnknown,
    };
    EmitQuad(
        mesh,
        BlockCoord{tile.x, tile.y, bottom_level},
        BlockTypeId::kRuinStructure,
        direction,
        TerrainRenderPass::kWalls,
        TerrainSurfaceKind::kUnknown,
        WallSpanCorners(span));
    ++info.structure_wall_faces;
}

void BuildStructureChunkMesh(
    const RuntimeMap& map,
    const ChunkInfo& chunk,
    ChunkMeshData& mesh,
    ChunkMeshBuildInfo& info,
    Diagnostics& diagnostics)
{
    if (!map.structure_height.IsValid()) {
        return;
    }

    for (int y = chunk.bounds.min_y; y < chunk.bounds.max_y; ++y) {
        for (int x = chunk.bounds.min_x; x < chunk.bounds.max_x; ++x) {
            const TileCoord tile{x, y};
            const int structure_height = StructureHeight(map, tile);
            if (structure_height <= 0) {
                continue;
            }

            const int ground_level = TileHeight(map, tile);
            const int top_level = ground_level + structure_height;
            EmitStructureTop(mesh, tile, top_level, info, diagnostics);

            for (const TerrainEdge& edge : kTerrainEdges) {
                const TileCoord neighbor{tile.x + edge.dx, tile.y + edge.dy};
                const int neighbor_top_level = ContainsTile(map, neighbor)
                    ? TileSolidTopLevel(map, neighbor)
                    : ground_level;
                const int visible_bottom_level = std::max(ground_level, neighbor_top_level);
                EmitStructureWall(
                    mesh,
                    tile,
                    edge.direction,
                    visible_bottom_level,
                    top_level,
                    info,
                    diagnostics);
            }
        }
    }
}

void BuildTopMask(const RuntimeMap& map, const ChunkInfo& chunk, std::vector<TopMaskCell>& mask, ChunkMeshBuildInfo& info)
{
    const int width = chunk.bounds.Width();
    const int height = chunk.bounds.Height();
    mask.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), TopMaskCell{});

    for (int local_y = 0; local_y < height; ++local_y) {
        const int world_y = chunk.bounds.min_y + local_y;
        for (int local_x = 0; local_x < width; ++local_x) {
            const int world_x = chunk.bounds.min_x + local_x;
            const TileCoord tile{world_x, world_y};
            TopMaskCell& cell = mask[MaskIndex(local_x, local_y, width)];
            cell.visible = true;
            cell.height = TileHeight(map, tile);
            cell.block_type = SurfaceBlockType(map, tile);
            cell.surface_kind = SurfaceKind(map, tile);
            ++info.terrain_raw_top_faces;
        }
    }
}

void EmitMergedTopSpans(
    ChunkMeshData& mesh,
    const ChunkInfo& chunk,
    std::vector<TopMaskCell>& mask,
    ChunkMeshBuildInfo& info,
    Diagnostics& diagnostics)
{
    const int width = chunk.bounds.Width();
    const int height = chunk.bounds.Height();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            TopMaskCell& start = mask[MaskIndex(x, y, width)];
            if (!start.visible) {
                continue;
            }

            int span_width = 1;
            while (x + span_width < width && SameTopMaskType(start, mask[MaskIndex(x + span_width, y, width)])) {
                ++span_width;
            }

            int span_height = 1;
            bool can_extend = true;
            while (y + span_height < height && can_extend) {
                for (int test_x = 0; test_x < span_width; ++test_x) {
                    if (!SameTopMaskType(start, mask[MaskIndex(x + test_x, y + span_height, width)])) {
                        can_extend = false;
                        break;
                    }
                }
                if (can_extend) {
                    ++span_height;
                }
            }

            EmitTopSpan(
                mesh,
                TopSpan{
                    TileCoord{chunk.bounds.min_x + x, chunk.bounds.min_y + y},
                    span_width,
                    span_height,
                    start.height,
                    start.block_type,
                    start.surface_kind,
                },
                info,
                diagnostics);

            for (int clear_y = 0; clear_y < span_height; ++clear_y) {
                for (int clear_x = 0; clear_x < span_width; ++clear_x) {
                    mask[MaskIndex(x + clear_x, y + clear_y, width)].visible = false;
                }
            }
        }
    }
}

[[nodiscard]] WallCell BuildWallCell(const RuntimeMap& map, TileCoord tile, const TerrainEdge& edge)
{
    WallCell cell;
    const int height = TileHeight(map, tile);
    const TileCoord neighbor{tile.x + edge.dx, tile.y + edge.dy};
    const int neighbor_height = ContainsTile(map, neighbor)
        ? TileHeight(map, neighbor)
        : (map.info.levels.has_value() ? map.info.levels->min - 1 : height - 1);
    if (neighbor_height >= height) {
        return cell;
    }

    cell.visible = true;
    cell.bottom_level = neighbor_height;
    cell.top_level = height;
    cell.block_type = SurfaceBlockType(map, tile);
    cell.surface_kind = SurfaceKind(map, tile);
    return cell;
}

void EmitMergedYWallSpans(
    const RuntimeMap& map,
    const ChunkInfo& chunk,
    const TerrainEdge& edge,
    ChunkMeshData& mesh,
    ChunkMeshBuildInfo& info,
    Diagnostics& diagnostics)
{
    for (int x = chunk.bounds.min_x; x < chunk.bounds.max_x; ++x) {
        int y = chunk.bounds.min_y;
        while (y < chunk.bounds.max_y) {
            const TileCoord tile{x, y};
            const WallCell start = BuildWallCell(map, tile, edge);
            if (!start.visible) {
                ++y;
                continue;
            }

            ++info.terrain_raw_wall_faces;
            int length = 1;
            while (y + length < chunk.bounds.max_y) {
                const WallCell next = BuildWallCell(map, TileCoord{x, y + length}, edge);
                if (!SameWallMaskType(start, next)) {
                    break;
                }
                ++info.terrain_raw_wall_faces;
                ++length;
            }

            EmitWallSpan(
                mesh,
                WallSpan{tile, length, start.bottom_level, start.top_level, edge.direction, start.block_type, start.surface_kind},
                info,
                diagnostics);
            y += length;
        }
    }
}

void EmitMergedXWallSpans(
    const RuntimeMap& map,
    const ChunkInfo& chunk,
    const TerrainEdge& edge,
    ChunkMeshData& mesh,
    ChunkMeshBuildInfo& info,
    Diagnostics& diagnostics)
{
    for (int y = chunk.bounds.min_y; y < chunk.bounds.max_y; ++y) {
        int x = chunk.bounds.min_x;
        while (x < chunk.bounds.max_x) {
            const TileCoord tile{x, y};
            const WallCell start = BuildWallCell(map, tile, edge);
            if (!start.visible) {
                ++x;
                continue;
            }

            ++info.terrain_raw_wall_faces;
            int length = 1;
            while (x + length < chunk.bounds.max_x) {
                const WallCell next = BuildWallCell(map, TileCoord{x + length, y}, edge);
                if (!SameWallMaskType(start, next)) {
                    break;
                }
                ++info.terrain_raw_wall_faces;
                ++length;
            }

            EmitWallSpan(
                mesh,
                WallSpan{tile, length, start.bottom_level, start.top_level, edge.direction, start.block_type, start.surface_kind},
                info,
                diagnostics);
            x += length;
        }
    }
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

    std::vector<TopMaskCell> top_mask;
    BuildTopMask(map, chunk, top_mask, info);
    EmitMergedTopSpans(mesh, chunk, top_mask, info, diagnostics);

    for (const TerrainEdge& edge : kTerrainEdges) {
        switch (edge.direction) {
            case FaceDirection::kWest:
            case FaceDirection::kEast:
                EmitMergedYWallSpans(map, chunk, edge, mesh, info, diagnostics);
                break;
            case FaceDirection::kNorth:
            case FaceDirection::kSouth:
                EmitMergedXWallSpans(map, chunk, edge, mesh, info, diagnostics);
                break;
            case FaceDirection::kDown:
            case FaceDirection::kUp:
                break;
        }
    }

    BuildStructureChunkMesh(map, chunk, mesh, info, diagnostics);
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
    if (info.levels.has_value() && map.structure_height.IsValid()) {
        for (int y = 0; y < map.info.height; ++y) {
            for (int x = 0; x < map.info.width; ++x) {
                info.levels->max = std::max(
                    info.levels->max,
                    TileSolidTopLevel(map, TileCoord{x, y}));
            }
        }
    }
    info.solid_blocks = static_cast<std::uint64_t>(map.info.width) * static_cast<std::uint64_t>(map.info.height);
    if (map.structure_height.IsValid()) {
        for (const std::uint8_t height : map.structure_height.cells) {
            info.solid_blocks += height;
        }
    }
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

ChunkMeshBuildResult BuildTerrainChunkMeshesForSelectedChunks(
    const RuntimeMap& map,
    const ChunkGrid& chunks,
    const std::vector<std::uint8_t>& selected_chunks)
{
    ChunkMeshBuildResult result;

    if (!map.IsValid()) {
        result.diagnostics.AddWarning("cannot build selected terrain mesh from invalid runtime map");
        return result;
    }
    if (!map.HasCoreGrids()) {
        result.diagnostics.AddWarning("cannot build selected terrain mesh without runtime map core grids");
        return result;
    }
    if (!chunks.IsValid()) {
        result.diagnostics.AddWarning("cannot build selected terrain mesh from invalid chunk grid");
        return result;
    }
    if (map.info.width != chunks.info.map_width || map.info.height != chunks.info.map_height) {
        result.diagnostics.AddWarning("cannot build selected terrain mesh because runtime map and chunk grid dimensions differ");
        return result;
    }
    if (chunks.info.total_chunks <= 0 || ExpectedChunkCount(chunks.info.chunks_x, chunks.info.chunks_y) != static_cast<std::uint64_t>(chunks.info.total_chunks)) {
        result.diagnostics.AddWarning("cannot build selected terrain mesh because chunk grid shape is inconsistent");
        return result;
    }
    if (selected_chunks.size() != chunks.chunks.size()) {
        result.diagnostics.AddWarning("cannot build selected terrain mesh because selection size differs from chunk grid");
        return result;
    }

    CopyMapShape(map, chunks, result.info);
    result.chunks.reserve(chunks.chunks.size());

    for (std::size_t index = 0; index < chunks.chunks.size(); ++index) {
        const ChunkInfo& chunk = chunks.chunks[index];
        ChunkMeshData mesh;
        mesh.coord = chunk.coord;
        mesh.bounds = chunk.bounds;

        if (selected_chunks[index] != 0U) {
            BuildTerrainChunkMesh(map, chunk, mesh, result.info, result.diagnostics);
        }
        AccumulateChunkStats(mesh, result.info);
        result.chunks.push_back(std::move(mesh));
    }

    if (!result.IsValid()) {
        result.diagnostics.AddWarning("selected terrain mesh validation failed after build");
    }
    return result;
}

}  // namespace vox3d
