#include "vox3d/mesh/structure_micro_mesh_builder.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace vox3d {
namespace {

constexpr int kMicroDivision = 4;
constexpr float kMicroSize = 1.0F / static_cast<float>(kMicroDivision);

struct MicroColumn {
    bool solid = false;
    int bottom_level = 0;
    int top_level = 0;
    RuntimeStructureType structure_type = RuntimeStructureType::kNone;
};

struct TopMaskCell {
    bool visible = false;
    int top_level = 0;
    RuntimeStructureType structure_type = RuntimeStructureType::kNone;
};

struct VisibleWallSegment {
    int bottom_level = 0;
    int top_level = 0;
    RuntimeStructureType structure_type = RuntimeStructureType::kNone;
    bool used = false;
};

struct WallRunCell {
    std::array<VisibleWallSegment, 2> segments;
    int segment_count = 0;
};

[[nodiscard]] std::size_t GridIndex(int x, int y, int width)
{
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
        + static_cast<std::size_t>(x);
}

[[nodiscard]] bool CanAppendQuad(const ChunkMeshData& mesh)
{
    constexpr std::size_t kQuadVertices = 4;
    constexpr std::size_t kQuadIndices = 6;
    return mesh.vertices.size()
            <= static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())
                - kQuadVertices
        && mesh.indices.size()
            <= static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())
                - kQuadIndices;
}

[[nodiscard]] bool SameTopCell(const TopMaskCell& lhs, const TopMaskCell& rhs)
{
    return lhs.visible && rhs.visible && lhs.top_level == rhs.top_level
        && lhs.structure_type == rhs.structure_type;
}

[[nodiscard]] MicroColumn ColumnAt(const RuntimeMap& map, int micro_x, int micro_y)
{
    MicroColumn column;
    if (micro_x < 0 || micro_y < 0) {
        return column;
    }

    const int tile_x = micro_x / kMicroDivision;
    const int tile_y = micro_y / kMicroDivision;
    if (tile_x < 0 || tile_y < 0 || tile_x >= map.info.width
        || tile_y >= map.info.height) {
        return column;
    }

    const int sub_x = micro_x % kMicroDivision;
    const int sub_y = micro_y % kMicroDivision;
    const std::size_t tile_index = GridIndex(tile_x, tile_y, map.info.width);
    const std::uint16_t mask = map.structure_micro_mask.cells[tile_index];
    const int bit_index = sub_y * kMicroDivision + sub_x;
    if ((mask & static_cast<std::uint16_t>(1U << bit_index)) == 0U) {
        return column;
    }

    const int height = static_cast<int>(map.structure_height.cells[tile_index]);
    const auto structure_type = static_cast<RuntimeStructureType>(
        map.structure_type.cells[tile_index]);
    if (height <= 0 || !IsVerticalStructureType(structure_type)) {
        return column;
    }

    column.solid = true;
    column.bottom_level = map.height.cells[tile_index];
    column.top_level = column.bottom_level + height;
    column.structure_type = structure_type;
    return column;
}

void EmitQuad(
    ChunkMeshData& mesh,
    BlockCoord block,
    FaceDirection direction,
    TerrainRenderPass render_pass,
    RuntimeStructureType structure_type,
    const std::array<MeshPosition, 4>& corners,
    Diagnostics& diagnostics)
{
    if (!CanAppendQuad(mesh)) {
        diagnostics.AddWarning(
            "structure micro mesh skipped faces because uint32 index space is exhausted");
        return;
    }

    const auto first_vertex = static_cast<std::uint32_t>(mesh.vertices.size());
    const auto first_index = static_cast<std::uint32_t>(mesh.indices.size());
    const std::uint8_t structure_type_id = static_cast<std::uint8_t>(structure_type);

    MeshFace face;
    face.block = block;
    face.direction = direction;
    face.block_type = BlockTypeId::kRuinStructure;
    face.terrain_pass = render_pass;
    face.surface_kind = TerrainSurfaceKind::kUnknown;
    face.first_vertex = first_vertex;
    face.first_index = first_index;
    face.structure_type = structure_type_id;
    mesh.faces.push_back(face);

    for (const MeshPosition& position : corners) {
        MeshVertex vertex;
        vertex.position = position;
        vertex.block_type = BlockTypeId::kRuinStructure;
        vertex.face_direction = direction;
        vertex.terrain_pass = render_pass;
        vertex.surface_kind = TerrainSurfaceKind::kUnknown;
        vertex.level = block.z;
        vertex.structure_type = structure_type_id;
        mesh.vertices.push_back(vertex);
    }

    mesh.indices.push_back(first_vertex + 0U);
    mesh.indices.push_back(first_vertex + 1U);
    mesh.indices.push_back(first_vertex + 2U);
    mesh.indices.push_back(first_vertex + 0U);
    mesh.indices.push_back(first_vertex + 2U);
    mesh.indices.push_back(first_vertex + 3U);
}

void EmitTopRect(
    ChunkMeshData& mesh,
    int micro_x,
    int micro_y,
    int width,
    int height,
    const TopMaskCell& cell,
    ChunkMeshBuildInfo* info,
    Diagnostics& diagnostics)
{
    const float x0 = static_cast<float>(micro_x) * kMicroSize;
    const float x1 = static_cast<float>(micro_x + width) * kMicroSize;
    const float y0 = static_cast<float>(micro_y) * kMicroSize;
    const float y1 = static_cast<float>(micro_y + height) * kMicroSize;
    const float z = static_cast<float>(cell.top_level + 1);

    const std::size_t before = mesh.faces.size();
    EmitQuad(
        mesh,
        BlockCoord{micro_x / kMicroDivision, micro_y / kMicroDivision, cell.top_level},
        FaceDirection::kUp,
        TerrainRenderPass::kTops,
        cell.structure_type,
        {{{x0, y0, z}, {x1, y0, z}, {x1, y1, z}, {x0, y1, z}}},
        diagnostics);
    if (info != nullptr && mesh.faces.size() != before) {
        ++info->structure_top_faces;
    }
}

void BuildTopFaces(
    const RuntimeMap& map,
    const ChunkInfo& chunk,
    ChunkMeshData& mesh,
    ChunkMeshBuildInfo* info,
    Diagnostics& diagnostics)
{
    const int micro_min_x = chunk.bounds.min_x * kMicroDivision;
    const int micro_min_y = chunk.bounds.min_y * kMicroDivision;
    const int width = chunk.bounds.Width() * kMicroDivision;
    const int height = chunk.bounds.Height() * kMicroDivision;
    std::vector<TopMaskCell> mask(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height));

    for (int local_y = 0; local_y < height; ++local_y) {
        for (int local_x = 0; local_x < width; ++local_x) {
            const MicroColumn column = ColumnAt(
                map,
                micro_min_x + local_x,
                micro_min_y + local_y);
            if (!column.solid) {
                continue;
            }
            mask[GridIndex(local_x, local_y, width)] = TopMaskCell{
                true,
                column.top_level,
                column.structure_type,
            };
        }
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            TopMaskCell& start = mask[GridIndex(x, y, width)];
            if (!start.visible) {
                continue;
            }

            int rect_width = 1;
            while (x + rect_width < width
                   && SameTopCell(start, mask[GridIndex(x + rect_width, y, width)])) {
                ++rect_width;
            }

            int rect_height = 1;
            bool can_extend = true;
            while (y + rect_height < height && can_extend) {
                for (int test_x = 0; test_x < rect_width; ++test_x) {
                    if (!SameTopCell(
                            start,
                            mask[GridIndex(x + test_x, y + rect_height, width)])) {
                        can_extend = false;
                        break;
                    }
                }
                if (can_extend) {
                    ++rect_height;
                }
            }

            EmitTopRect(
                mesh,
                micro_min_x + x,
                micro_min_y + y,
                rect_width,
                rect_height,
                start,
                info,
                diagnostics);

            for (int clear_y = 0; clear_y < rect_height; ++clear_y) {
                for (int clear_x = 0; clear_x < rect_width; ++clear_x) {
                    mask[GridIndex(x + clear_x, y + clear_y, width)].visible = false;
                }
            }
        }
    }
}

[[nodiscard]] std::array<MeshPosition, 4> WallCorners(
    FaceDirection direction,
    int fixed_micro,
    int run_micro,
    int run_length,
    int bottom_level,
    int level_height)
{
    const float fixed = static_cast<float>(fixed_micro) * kMicroSize;
    const float run0 = static_cast<float>(run_micro) * kMicroSize;
    const float run1 = static_cast<float>(run_micro + run_length) * kMicroSize;
    const float z0 = static_cast<float>(bottom_level + 1);
    const float z1 = static_cast<float>(bottom_level + level_height + 1);

    switch (direction) {
        case FaceDirection::kWest:
            return {{{fixed, run0, z0}, {fixed, run0, z1}, {fixed, run1, z1}, {fixed, run1, z0}}};
        case FaceDirection::kEast:
            return {{{fixed, run1, z0}, {fixed, run1, z1}, {fixed, run0, z1}, {fixed, run0, z0}}};
        case FaceDirection::kNorth:
            return {{{run1, fixed, z0}, {run1, fixed, z1}, {run0, fixed, z1}, {run0, fixed, z0}}};
        case FaceDirection::kSouth:
            return {{{run0, fixed, z0}, {run0, fixed, z1}, {run1, fixed, z1}, {run1, fixed, z0}}};
        case FaceDirection::kDown:
        case FaceDirection::kUp:
            break;
    }
    return {};
}

[[nodiscard]] bool SameSegment(
    const VisibleWallSegment& lhs,
    const VisibleWallSegment& rhs)
{
    return lhs.bottom_level == rhs.bottom_level
        && lhs.top_level == rhs.top_level
        && lhs.structure_type == rhs.structure_type;
}

void AddVisibleSegment(
    WallRunCell& cell,
    int bottom_level,
    int top_level,
    RuntimeStructureType structure_type)
{
    if (bottom_level >= top_level || cell.segment_count >= 2) {
        return;
    }
    cell.segments[static_cast<std::size_t>(cell.segment_count)] = VisibleWallSegment{
        bottom_level,
        top_level,
        structure_type,
        false,
    };
    ++cell.segment_count;
}

[[nodiscard]] WallRunCell BuildVisibleSegments(
    const MicroColumn& current,
    const MicroColumn& neighbor)
{
    WallRunCell cell;
    if (!current.solid) {
        return cell;
    }
    if (!neighbor.solid) {
        AddVisibleSegment(
            cell,
            current.bottom_level,
            current.top_level,
            current.structure_type);
        return cell;
    }

    AddVisibleSegment(
        cell,
        current.bottom_level,
        std::min(current.top_level, neighbor.bottom_level),
        current.structure_type);
    AddVisibleSegment(
        cell,
        std::max(current.bottom_level, neighbor.top_level),
        current.top_level,
        current.structure_type);
    return cell;
}

[[nodiscard]] int FindMatchingSegment(
    WallRunCell& cell,
    const VisibleWallSegment& expected)
{
    for (int index = 0; index < cell.segment_count; ++index) {
        VisibleWallSegment& candidate = cell.segments[static_cast<std::size_t>(index)];
        if (!candidate.used && SameSegment(candidate, expected)) {
            return index;
        }
    }
    return -1;
}

void BuildWallPlane(
    const RuntimeMap& map,
    const ChunkInfo& chunk,
    FaceDirection direction,
    int fixed_local,
    ChunkMeshData& mesh,
    ChunkMeshBuildInfo* info,
    Diagnostics& diagnostics)
{
    const bool x_plane = direction == FaceDirection::kWest
        || direction == FaceDirection::kEast;
    const int micro_min_x = chunk.bounds.min_x * kMicroDivision;
    const int micro_min_y = chunk.bounds.min_y * kMicroDivision;
    const int micro_width = chunk.bounds.Width() * kMicroDivision;
    const int micro_height = chunk.bounds.Height() * kMicroDivision;
    const int run_size = x_plane ? micro_height : micro_width;
    const int run_start = x_plane ? micro_min_y : micro_min_x;
    const int cell_fixed = x_plane ? micro_min_x + fixed_local
                                   : micro_min_y + fixed_local;
    const int fixed_boundary = (direction == FaceDirection::kEast
                                || direction == FaceDirection::kSouth)
        ? cell_fixed + 1
        : cell_fixed;
    std::vector<WallRunCell> cells(static_cast<std::size_t>(run_size));

    for (int run = 0; run < run_size; ++run) {
        const int cell_run = run_start + run;
        const int current_x = x_plane ? cell_fixed : cell_run;
        const int current_y = x_plane ? cell_run : cell_fixed;
        int neighbor_x = current_x;
        int neighbor_y = current_y;
        switch (direction) {
            case FaceDirection::kWest:
                --neighbor_x;
                break;
            case FaceDirection::kEast:
                ++neighbor_x;
                break;
            case FaceDirection::kNorth:
                --neighbor_y;
                break;
            case FaceDirection::kSouth:
                ++neighbor_y;
                break;
            case FaceDirection::kDown:
            case FaceDirection::kUp:
                break;
        }
        cells[static_cast<std::size_t>(run)] = BuildVisibleSegments(
            ColumnAt(map, current_x, current_y),
            ColumnAt(map, neighbor_x, neighbor_y));
    }

    for (int run = 0; run < run_size; ++run) {
        WallRunCell& start_cell = cells[static_cast<std::size_t>(run)];
        for (int segment_index = 0;
             segment_index < start_cell.segment_count;
             ++segment_index) {
            VisibleWallSegment& start_segment =
                start_cell.segments[static_cast<std::size_t>(segment_index)];
            if (start_segment.used) {
                continue;
            }
            start_segment.used = true;

            int run_length = 1;
            while (run + run_length < run_size) {
                WallRunCell& next_cell =
                    cells[static_cast<std::size_t>(run + run_length)];
                const int matching_index = FindMatchingSegment(
                    next_cell,
                    start_segment);
                if (matching_index < 0) {
                    break;
                }
                next_cell.segments[static_cast<std::size_t>(matching_index)].used = true;
                ++run_length;
            }

            const int world_run = run_start + run;
            const std::size_t before = mesh.faces.size();
            EmitQuad(
                mesh,
                x_plane
                    ? BlockCoord{cell_fixed / kMicroDivision,
                                 world_run / kMicroDivision,
                                 start_segment.bottom_level}
                    : BlockCoord{world_run / kMicroDivision,
                                 cell_fixed / kMicroDivision,
                                 start_segment.bottom_level},
                direction,
                TerrainRenderPass::kWalls,
                start_segment.structure_type,
                WallCorners(
                    direction,
                    fixed_boundary,
                    world_run,
                    run_length,
                    start_segment.bottom_level,
                    start_segment.top_level - start_segment.bottom_level),
                diagnostics);
            if (info != nullptr && mesh.faces.size() != before) {
                ++info->structure_wall_faces;
            }
        }
    }
}

void BuildWallFaces(
    const RuntimeMap& map,
    const ChunkInfo& chunk,
    ChunkMeshData& mesh,
    ChunkMeshBuildInfo* info,
    Diagnostics& diagnostics)
{
    const int micro_width = chunk.bounds.Width() * kMicroDivision;
    const int micro_height = chunk.bounds.Height() * kMicroDivision;
    for (int local_x = 0; local_x < micro_width; ++local_x) {
        BuildWallPlane(
            map,
            chunk,
            FaceDirection::kWest,
            local_x,
            mesh,
            info,
            diagnostics);
        BuildWallPlane(
            map,
            chunk,
            FaceDirection::kEast,
            local_x,
            mesh,
            info,
            diagnostics);
    }
    for (int local_y = 0; local_y < micro_height; ++local_y) {
        BuildWallPlane(
            map,
            chunk,
            FaceDirection::kNorth,
            local_y,
            mesh,
            info,
            diagnostics);
        BuildWallPlane(
            map,
            chunk,
            FaceDirection::kSouth,
            local_y,
            mesh,
            info,
            diagnostics);
    }
}

}  // namespace

bool HasStructureMicroGeometry(const RuntimeMap& map)
{
    return map.info.structure_type_loaded
        && map.info.structure_micro_geometry_loaded
        && map.info.structure_micro_division == kMicroDivision
        && map.structure_height.IsValid()
        && map.structure_type.IsValid()
        && map.structure_micro_mask.IsValid()
        && map.structure_height.width == map.info.width
        && map.structure_height.height == map.info.height
        && map.structure_type.width == map.info.width
        && map.structure_type.height == map.info.height
        && map.structure_micro_mask.width == map.info.width
        && map.structure_micro_mask.height == map.info.height;
}

void AppendStructureMicroChunkMesh(
    const RuntimeMap& map,
    const ChunkInfo& chunk,
    ChunkMeshData& mesh,
    ChunkMeshBuildInfo* info,
    Diagnostics& diagnostics)
{
    if (!HasStructureMicroGeometry(map) || !chunk.bounds.IsValid()) {
        return;
    }

    BuildTopFaces(map, chunk, mesh, info, diagnostics);
    BuildWallFaces(map, chunk, mesh, info, diagnostics);
}

}  // namespace vox3d
