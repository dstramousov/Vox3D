#include "vox3d/render_raylib/chunk_mesh_preview.hpp"

#include "vox3d/voxel/block.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <string_view>
#include <sstream>

namespace vox3d {

std::string_view ToString(RaylibChunkMeshColorMode mode)
{
    switch (mode) {
        case RaylibChunkMeshColorMode::kMaterial:
            return "material";
        case RaylibChunkMeshColorMode::kGeographic:
            return "geographic";
        case RaylibChunkMeshColorMode::kChunkId:
            return "chunk_id";
        case RaylibChunkMeshColorMode::kFaceType:
            return "face_type";
    }
    return "unknown";
}

std::string_view ToString(RaylibChunkVisibilityMode mode)
{
    switch (mode) {
        case RaylibChunkVisibilityMode::kAllChunks:
            return "all_chunks";
        case RaylibChunkVisibilityMode::kRadiusFade:
            return "radius_fade";
        case RaylibChunkVisibilityMode::kHardCull:
            return "hard_cull";
    }
    return "unknown";
}

namespace {

enum class ChunkVisibilityClass {
    kVisible,
    kFade,
    kHidden,
};

struct RgbaColor {
    unsigned char r = 255;
    unsigned char g = 255;
    unsigned char b = 255;
    unsigned char a = 255;
};

[[nodiscard]] RgbaColor BaseColor(BlockTypeId type)
{
    switch (type) {
        case BlockTypeId::kEmpty:
            return RgbaColor{0, 0, 0, 0};
        case BlockTypeId::kSubsurface:
            return RgbaColor{96, 92, 84, 255};
        case BlockTypeId::kTerrainSurface:
            return RgbaColor{86, 146, 82, 255};
        case BlockTypeId::kBlockedSurface:
            return RgbaColor{92, 88, 78, 255};
    }
    return RgbaColor{160, 160, 150, 255};
}

[[nodiscard]] float DirectionShade(FaceDirection direction)
{
    switch (direction) {
        case FaceDirection::kUp:
            return 1.12F;
        case FaceDirection::kDown:
            return 0.50F;
        case FaceDirection::kWest:
        case FaceDirection::kNorth:
            return 0.78F;
        case FaceDirection::kEast:
        case FaceDirection::kSouth:
            return 0.92F;
    }
    return 1.0F;
}

[[nodiscard]] RgbaColor ApplyShade(RgbaColor base, float shade)
{
    return RgbaColor{
        static_cast<unsigned char>(std::clamp(std::round(static_cast<float>(base.r) * shade), 0.0F, 255.0F)),
        static_cast<unsigned char>(std::clamp(std::round(static_cast<float>(base.g) * shade), 0.0F, 255.0F)),
        static_cast<unsigned char>(std::clamp(std::round(static_cast<float>(base.b) * shade), 0.0F, 255.0F)),
        base.a,
    };
}

[[nodiscard]] RgbaColor GeographicBaseColor(int level)
{
    if (level <= -3) {
        return RgbaColor{27, 73, 137, 255};
    }
    if (level <= -1) {
        return RgbaColor{56, 132, 190, 255};
    }
    if (level <= 2) {
        return RgbaColor{71, 143, 75, 255};
    }
    if (level <= 6) {
        return RgbaColor{119, 173, 83, 255};
    }
    if (level <= 10) {
        return RgbaColor{199, 190, 94, 255};
    }
    if (level <= 14) {
        return RgbaColor{205, 139, 52, 255};
    }
    if (level <= 18) {
        return RgbaColor{121, 84, 59, 255};
    }
    return RgbaColor{230, 229, 218, 255};
}

[[nodiscard]] RgbaColor ChunkBaseColor(ChunkCoord coord)
{
    const std::uint32_t hash = static_cast<std::uint32_t>(coord.x * 73856093)
        ^ static_cast<std::uint32_t>(coord.y * 19349663);
    return RgbaColor{
        static_cast<unsigned char>(92U + (hash & 0x5FU)),
        static_cast<unsigned char>(104U + ((hash >> 8U) & 0x5FU)),
        static_cast<unsigned char>(116U + ((hash >> 16U) & 0x5FU)),
        255,
    };
}

[[nodiscard]] RgbaColor FaceTypeBaseColor(FaceDirection direction)
{
    switch (direction) {
        case FaceDirection::kUp:
            return RgbaColor{84, 170, 90, 255};
        case FaceDirection::kDown:
            return RgbaColor{94, 82, 134, 255};
        case FaceDirection::kWest:
        case FaceDirection::kEast:
            return RgbaColor{177, 121, 64, 255};
        case FaceDirection::kNorth:
        case FaceDirection::kSouth:
            return RgbaColor{132, 104, 82, 255};
    }
    return RgbaColor{180, 180, 170, 255};
}

[[nodiscard]] RgbaColor VertexBaseColor(
    const MeshVertex& vertex,
    ChunkCoord chunk_coord,
    RaylibChunkMeshColorMode color_mode)
{
    switch (color_mode) {
        case RaylibChunkMeshColorMode::kMaterial:
            return BaseColor(vertex.block_type);
        case RaylibChunkMeshColorMode::kGeographic:
            return GeographicBaseColor(vertex.level);
        case RaylibChunkMeshColorMode::kChunkId:
            return ChunkBaseColor(chunk_coord);
        case RaylibChunkMeshColorMode::kFaceType:
            return FaceTypeBaseColor(vertex.face_direction);
    }
    return BaseColor(vertex.block_type);
}

[[nodiscard]] RgbaColor VertexColor(
    const MeshVertex& vertex,
    ChunkCoord chunk_coord,
    RaylibChunkMeshColorMode color_mode)
{
    return ApplyShade(VertexBaseColor(vertex, chunk_coord, color_mode), DirectionShade(vertex.face_direction));
}

[[nodiscard]] Vector3 WorldPosition(const MeshPosition& position, int map_width, int map_height)
{
    return Vector3{
        position.x - static_cast<float>(map_width) * 0.5F,
        position.z,
        static_cast<float>(map_height) * 0.5F - position.y,
    };
}

[[nodiscard]] int ClampInt(int value, int minimum, int maximum)
{
    return std::max(minimum, std::min(maximum, value));
}

[[nodiscard]] ChunkCoord CameraChunkCoord(
    const Camera3D& camera,
    const ChunkMeshBuildInfo& info)
{
    const int chunk_size_x = std::max(1, info.chunk_size_x);
    const int chunk_size_y = std::max(1, info.chunk_size_y);
    const float tile_x = camera.position.x + static_cast<float>(info.map_width) * 0.5F;
    const float tile_y = static_cast<float>(info.map_height) * 0.5F - camera.position.z;
    const int chunk_x = ClampInt(
        static_cast<int>(std::floor(tile_x / static_cast<float>(chunk_size_x))),
        0,
        std::max(0, info.chunks_x - 1));
    const int chunk_y = ClampInt(
        static_cast<int>(std::floor(tile_y / static_cast<float>(chunk_size_y))),
        0,
        std::max(0, info.chunks_y - 1));
    return ChunkCoord{chunk_x, chunk_y};
}

[[nodiscard]] ChunkVisibilityClass ClassifyChunkVisibility(
    ChunkCoord chunk,
    ChunkCoord camera_chunk,
    RaylibChunkVisibilityOptions visibility)
{
    if (visibility.mode == RaylibChunkVisibilityMode::kAllChunks) {
        return ChunkVisibilityClass::kVisible;
    }

    const int radius = std::max(0, visibility.radius_chunks);
    const int fade_ring = std::max(0, visibility.fade_ring_chunks);
    const int distance = std::max(
        std::abs(chunk.x - camera_chunk.x),
        std::abs(chunk.y - camera_chunk.y));
    if (distance <= radius) {
        return ChunkVisibilityClass::kVisible;
    }
    if (visibility.mode == RaylibChunkVisibilityMode::kRadiusFade
        && distance <= radius + fade_ring) {
        return ChunkVisibilityClass::kFade;
    }
    return ChunkVisibilityClass::kHidden;
}

[[nodiscard]] Color VisibilityTint(ChunkVisibilityClass visibility)
{
    switch (visibility) {
        case ChunkVisibilityClass::kVisible:
            return Color{255, 255, 255, 255};
        case ChunkVisibilityClass::kFade:
            return Color{86, 94, 94, 255};
        case ChunkVisibilityClass::kHidden:
            return Color{0, 0, 0, 0};
    }
    return Color{255, 255, 255, 255};
}

[[nodiscard]] Vector3 TileCornerWorld(float tile_x, float tile_y, float level, int map_width, int map_height);

[[nodiscard]] float HiddenBoundsLevel(const ChunkMeshBuildResult& build_result)
{
    if (build_result.info.levels.has_value()) {
        return static_cast<float>(build_result.info.levels->max + 2);
    }
    return 1.0F;
}

void DrawHiddenChunkBounds(
    const std::vector<RaylibUploadedChunkModel>& chunks,
    const ChunkMeshBuildResult& build_result,
    const Camera3D& camera,
    RaylibChunkVisibilityOptions visibility)
{
    if (visibility.mode == RaylibChunkVisibilityMode::kAllChunks || !visibility.show_hidden_bounds) {
        return;
    }

    const ChunkCoord camera_chunk = CameraChunkCoord(camera, build_result.info);
    const float level = HiddenBoundsLevel(build_result);
    constexpr Color kHiddenBounds{255, 105, 90, 150};
    for (const auto& chunk : chunks) {
        if (ClassifyChunkVisibility(chunk.coord, camera_chunk, visibility) != ChunkVisibilityClass::kHidden) {
            continue;
        }
        const Vector3 nw = TileCornerWorld(static_cast<float>(chunk.bounds.min_x), static_cast<float>(chunk.bounds.min_y), level, build_result.info.map_width, build_result.info.map_height);
        const Vector3 ne = TileCornerWorld(static_cast<float>(chunk.bounds.max_x), static_cast<float>(chunk.bounds.min_y), level, build_result.info.map_width, build_result.info.map_height);
        const Vector3 se = TileCornerWorld(static_cast<float>(chunk.bounds.max_x), static_cast<float>(chunk.bounds.max_y), level, build_result.info.map_width, build_result.info.map_height);
        const Vector3 sw = TileCornerWorld(static_cast<float>(chunk.bounds.min_x), static_cast<float>(chunk.bounds.max_y), level, build_result.info.map_width, build_result.info.map_height);
        DrawLine3D(nw, ne, kHiddenBounds);
        DrawLine3D(ne, se, kHiddenBounds);
        DrawLine3D(se, sw, kHiddenBounds);
        DrawLine3D(sw, nw, kHiddenBounds);
    }
}

[[nodiscard]] Vector3 FaceNormal(FaceDirection direction)
{
    switch (direction) {
        case FaceDirection::kWest:
            return Vector3{-1.0F, 0.0F, 0.0F};
        case FaceDirection::kEast:
            return Vector3{1.0F, 0.0F, 0.0F};
        case FaceDirection::kNorth:
            return Vector3{0.0F, 0.0F, 1.0F};
        case FaceDirection::kSouth:
            return Vector3{0.0F, 0.0F, -1.0F};
        case FaceDirection::kDown:
            return Vector3{0.0F, -1.0F, 0.0F};
        case FaceDirection::kUp:
            return Vector3{0.0F, 1.0F, 0.0F};
    }
    return Vector3{0.0F, 1.0F, 0.0F};
}

[[nodiscard]] bool CanUploadChunk(const ChunkMeshData& chunk)
{
    if (chunk.vertices.empty() || chunk.indices.empty() || !chunk.IsValid()) {
        return false;
    }
    if (chunk.vertices.size() > static_cast<std::size_t>(std::numeric_limits<unsigned short>::max())) {
        return false;
    }

    constexpr std::size_t kMaxRaylibAllocation = static_cast<std::size_t>(std::numeric_limits<unsigned int>::max());
    if (chunk.vertices.size() > kMaxRaylibAllocation / (3ULL * sizeof(float))
        || chunk.vertices.size() > kMaxRaylibAllocation / (4ULL * sizeof(unsigned char))
        || chunk.indices.size() > kMaxRaylibAllocation / sizeof(unsigned short)) {
        return false;
    }

    return std::all_of(chunk.indices.begin(), chunk.indices.end(), [](std::uint32_t index) {
        return index <= static_cast<std::uint32_t>(std::numeric_limits<unsigned short>::max());
    });
}

void CopyChunkVertices(
    const ChunkMeshData& chunk,
    Mesh& mesh,
    int map_width,
    int map_height,
    RaylibChunkMeshColorMode color_mode)
{
    for (std::size_t i = 0; i < chunk.vertices.size(); ++i) {
        const MeshVertex& source = chunk.vertices[i];
        const Vector3 position = WorldPosition(source.position, map_width, map_height);
        const Vector3 normal = FaceNormal(source.face_direction);
        const RgbaColor color = VertexColor(source, chunk.coord, color_mode);
        const std::size_t vertex_offset = i * 3ULL;
        const std::size_t color_offset = i * 4ULL;

        mesh.vertices[vertex_offset + 0ULL] = position.x;
        mesh.vertices[vertex_offset + 1ULL] = position.y;
        mesh.vertices[vertex_offset + 2ULL] = position.z;

        mesh.normals[vertex_offset + 0ULL] = normal.x;
        mesh.normals[vertex_offset + 1ULL] = normal.y;
        mesh.normals[vertex_offset + 2ULL] = normal.z;

        mesh.colors[color_offset + 0ULL] = color.r;
        mesh.colors[color_offset + 1ULL] = color.g;
        mesh.colors[color_offset + 2ULL] = color.b;
        mesh.colors[color_offset + 3ULL] = color.a;
    }
}

void CopyChunkIndices(const ChunkMeshData& chunk, Mesh& mesh)
{
    for (std::size_t i = 0; i < chunk.indices.size(); ++i) {
        mesh.indices[i] = static_cast<unsigned short>(chunk.indices[i]);
    }
}

[[nodiscard]] Model LoadChunkModel(
    const ChunkMeshData& chunk,
    int map_width,
    int map_height,
    RaylibChunkMeshColorMode color_mode)
{
    Mesh mesh{};
    mesh.vertexCount = static_cast<int>(chunk.vertices.size());
    mesh.triangleCount = static_cast<int>(chunk.indices.size() / 3ULL);
    mesh.vertices = static_cast<float*>(MemAlloc(static_cast<unsigned int>(chunk.vertices.size() * 3ULL * sizeof(float))));
    mesh.normals = static_cast<float*>(MemAlloc(static_cast<unsigned int>(chunk.vertices.size() * 3ULL * sizeof(float))));
    mesh.colors = static_cast<unsigned char*>(MemAlloc(static_cast<unsigned int>(chunk.vertices.size() * 4ULL * sizeof(unsigned char))));
    mesh.indices = static_cast<unsigned short*>(MemAlloc(static_cast<unsigned int>(chunk.indices.size() * sizeof(unsigned short))));

    if (mesh.vertices == nullptr || mesh.normals == nullptr || mesh.colors == nullptr || mesh.indices == nullptr) {
        if (mesh.vertices != nullptr) {
            MemFree(mesh.vertices);
        }
        if (mesh.normals != nullptr) {
            MemFree(mesh.normals);
        }
        if (mesh.colors != nullptr) {
            MemFree(mesh.colors);
        }
        if (mesh.indices != nullptr) {
            MemFree(mesh.indices);
        }
        return Model{};
    }

    CopyChunkVertices(chunk, mesh, map_width, map_height, color_mode);
    CopyChunkIndices(chunk, mesh);
    UploadMesh(&mesh, false);
    return LoadModelFromMesh(mesh);
}


[[nodiscard]] Vector3 TileCenterWorld(int tile_x, int tile_y, float level, int map_width, int map_height)
{
    return Vector3{
        static_cast<float>(tile_x) + 0.5F - static_cast<float>(map_width) * 0.5F,
        level,
        static_cast<float>(map_height) * 0.5F - static_cast<float>(tile_y) - 0.5F,
    };
}

[[nodiscard]] Vector3 TileCornerWorld(float tile_x, float tile_y, float level, int map_width, int map_height)
{
    return Vector3{
        tile_x - static_cast<float>(map_width) * 0.5F,
        level,
        static_cast<float>(map_height) * 0.5F - tile_y,
    };
}

[[nodiscard]] float OverlayBaseLevel(const ChunkMeshBuildResult& build_result)
{
    if (!build_result.info.levels.has_value()) {
        return 0.0F;
    }
    return static_cast<float>(build_result.info.levels->min);
}

void DrawChunkBoundsOverlay(const ChunkGrid& chunks, int map_width, int map_height)
{
    constexpr Color kChunkColor{255, 214, 92, 210};
    for (const ChunkInfo& chunk : chunks.chunks) {
        if (!chunk.bounds.IsValid()) {
            continue;
        }
        const float level = chunk.levels.has_value() ? static_cast<float>(chunk.levels->max + 1) : 0.08F;
        const Vector3 nw = TileCornerWorld(static_cast<float>(chunk.bounds.min_x), static_cast<float>(chunk.bounds.min_y), level, map_width, map_height);
        const Vector3 ne = TileCornerWorld(static_cast<float>(chunk.bounds.max_x), static_cast<float>(chunk.bounds.min_y), level, map_width, map_height);
        const Vector3 se = TileCornerWorld(static_cast<float>(chunk.bounds.max_x), static_cast<float>(chunk.bounds.max_y), level, map_width, map_height);
        const Vector3 sw = TileCornerWorld(static_cast<float>(chunk.bounds.min_x), static_cast<float>(chunk.bounds.max_y), level, map_width, map_height);
        DrawLine3D(nw, ne, kChunkColor);
        DrawLine3D(ne, se, kChunkColor);
        DrawLine3D(se, sw, kChunkColor);
        DrawLine3D(sw, nw, kChunkColor);
    }
}

void DrawWorldGridOverlay(const ChunkMeshBuildResult& build_result)
{
    const float map_width = static_cast<float>(std::max(1, build_result.info.map_width));
    const float map_height = static_cast<float>(std::max(1, build_result.info.map_height));
    const float level = OverlayBaseLevel(build_result) - 0.02F;
    const int step = std::max(4, build_result.info.chunk_size_x > 0 ? build_result.info.chunk_size_x : 16);
    constexpr Color kGridColor{190, 210, 220, 90};

    for (int x = 0; x <= build_result.info.map_width; x += step) {
        const Vector3 a = TileCornerWorld(static_cast<float>(x), 0.0F, level, build_result.info.map_width, build_result.info.map_height);
        const Vector3 b = TileCornerWorld(static_cast<float>(x), map_height, level, build_result.info.map_width, build_result.info.map_height);
        DrawLine3D(a, b, kGridColor);
    }
    for (int y = 0; y <= build_result.info.map_height; y += step) {
        const Vector3 a = TileCornerWorld(0.0F, static_cast<float>(y), level, build_result.info.map_width, build_result.info.map_height);
        const Vector3 b = TileCornerWorld(map_width, static_cast<float>(y), level, build_result.info.map_width, build_result.info.map_height);
        DrawLine3D(a, b, kGridColor);
    }
}

void DrawCollisionOverlay(const RuntimeMap& map)
{
    if (!map.collision.IsValid() || !map.height.IsValid()) {
        return;
    }

    constexpr Color kCollisionColor{240, 72, 72, 155};
    constexpr Vector3 kSize{0.78F, 0.08F, 0.78F};
    for (int y = 0; y < map.info.height; ++y) {
        for (int x = 0; x < map.info.width; ++x) {
            const auto index = static_cast<std::size_t>(y) * static_cast<std::size_t>(map.info.width) + static_cast<std::size_t>(x);
            if (map.collision.cells[index] == 0U) {
                continue;
            }
            const float level = static_cast<float>(map.height.cells[index] + 1) + 0.08F;
            DrawCubeV(TileCenterWorld(x, y, level, map.info.width, map.info.height), kSize, kCollisionColor);
        }
    }
}

[[nodiscard]] Color HeightColor(int level, LevelRange range)
{
    const int span = std::max(1, range.max - range.min);
    const float t = std::clamp(static_cast<float>(level - range.min) / static_cast<float>(span), 0.0F, 1.0F);
    return Color{
        static_cast<unsigned char>(80.0F + 175.0F * t),
        static_cast<unsigned char>(170.0F - 80.0F * t),
        static_cast<unsigned char>(255.0F - 175.0F * t),
        220,
    };
}

void DrawHeightOverlay(const RuntimeMap& map)
{
    if (!map.height.IsValid() || !map.info.levels.has_value()) {
        return;
    }

    const int sample_step = std::max(1, std::max(map.info.width, map.info.height) / 96);
    const LevelRange levels = *map.info.levels;
    for (int y = 0; y < map.info.height; y += sample_step) {
        for (int x = 0; x < map.info.width; x += sample_step) {
            const auto index = static_cast<std::size_t>(y) * static_cast<std::size_t>(map.info.width) + static_cast<std::size_t>(x);
            const int level = map.height.cells[index];
            const Vector3 a = TileCenterWorld(x, y, static_cast<float>(level + 1) + 0.05F, map.info.width, map.info.height);
            const Vector3 b = TileCenterWorld(x, y, static_cast<float>(level + 1) + 0.85F, map.info.width, map.info.height);
            DrawLine3D(a, b, HeightColor(level, levels));
        }
    }
}

void DrawDebugOverlays(
    const ChunkMeshBuildResult& build_result,
    const RuntimeMap* runtime_map,
    const ChunkGrid* chunk_grid,
    RaylibChunkMeshDebugOverlayOptions overlays)
{
    if (overlays.show_world_grid) {
        DrawWorldGridOverlay(build_result);
    }
    if (overlays.show_chunk_bounds && chunk_grid != nullptr && chunk_grid->IsValid()) {
        DrawChunkBoundsOverlay(*chunk_grid, build_result.info.map_width, build_result.info.map_height);
    }
    if (runtime_map == nullptr || !runtime_map->IsValid()) {
        return;
    }
    if (overlays.show_collision) {
        DrawCollisionOverlay(*runtime_map);
    }
    if (overlays.show_height) {
        DrawHeightOverlay(*runtime_map);
    }
}

}  // namespace

bool RaylibChunkVisibilityStats::IsValid() const
{
    return resident_chunks > 0 && resident_chunks == drawn_models + culled_models;
}

double RaylibChunkVisibilityStats::DrawSavedRatio() const
{
    return resident_chunks == 0 ? 0.0 : static_cast<double>(culled_models) / static_cast<double>(resident_chunks);
}

double RaylibChunkVisibilityStats::FaceSavedRatio() const
{
    return total_faces == 0 ? 0.0 : static_cast<double>(culled_faces) / static_cast<double>(total_faces);
}

bool RaylibChunkMeshPreviewStats::IsValid() const
{
    return uploaded && models > 0 && faces > 0 && vertices == faces * 4ULL && indices == faces * 6ULL;
}

RaylibChunkMeshPreview::~RaylibChunkMeshPreview()
{
    Unload();
}

bool RaylibChunkMeshPreview::Upload(const ChunkMeshBuildResult& build_result, RaylibChunkMeshColorMode color_mode)
{
    Unload();

    if (!build_result.IsValid()) {
        return false;
    }

    chunks_.reserve(build_result.chunks.size());
    for (const ChunkMeshData& chunk : build_result.chunks) {
        if (chunk.faces.empty()) {
            continue;
        }
        if (!CanUploadChunk(chunk)) {
            ++stats_.skipped_chunks;
            continue;
        }

        Model model = LoadChunkModel(chunk, build_result.info.map_width, build_result.info.map_height, color_mode);
        if (model.meshCount <= 0 || model.meshes == nullptr) {
            ++stats_.skipped_chunks;
            continue;
        }

        chunks_.push_back(RaylibUploadedChunkModel{model, chunk.coord, chunk.bounds, static_cast<std::uint64_t>(chunk.faces.size())});
        ++stats_.models;
        stats_.faces += static_cast<std::uint64_t>(chunk.faces.size());
        stats_.vertices += static_cast<std::uint64_t>(chunk.vertices.size());
        stats_.indices += static_cast<std::uint64_t>(chunk.indices.size());
    }

    stats_.uploaded = !chunks_.empty();
    return stats_.uploaded;
}

void RaylibChunkMeshPreview::Draw(
    Rectangle viewport,
    const ChunkMeshBuildResult& build_result,
    const Camera3D& camera,
    const RuntimeMap* runtime_map,
    const ChunkGrid* chunk_grid,
    RaylibChunkMeshDebugOverlayOptions overlays,
    RaylibChunkVisibilityOptions visibility) const
{
    if (!IsUploaded() || viewport.width <= 1.0F || viewport.height <= 1.0F) {
        return;
    }

    BeginScissorMode(
        static_cast<int>(viewport.x),
        static_cast<int>(viewport.y),
        static_cast<int>(viewport.width),
        static_cast<int>(viewport.height));
    BeginMode3D(camera);

    constexpr Vector3 kOrigin{0.0F, 0.0F, 0.0F};
    constexpr float kScale = 1.0F;
    const ChunkCoord camera_chunk = CameraChunkCoord(camera, build_result.info);
    for (const RaylibUploadedChunkModel& chunk : chunks_) {
        const ChunkVisibilityClass visibility_class = ClassifyChunkVisibility(chunk.coord, camera_chunk, visibility);
        if (visibility_class == ChunkVisibilityClass::kHidden) {
            continue;
        }
        DrawModel(chunk.model, kOrigin, kScale, VisibilityTint(visibility_class));
    }

    DrawHiddenChunkBounds(chunks_, build_result, camera, visibility);
    DrawDebugOverlays(build_result, runtime_map, chunk_grid, overlays);

    EndMode3D();
    EndScissorMode();
}

RaylibChunkVisibilityStats RaylibChunkMeshPreview::CalculateVisibilityStats(
    const ChunkMeshBuildResult& build_result,
    const Camera3D& camera,
    RaylibChunkVisibilityOptions visibility) const
{
    RaylibChunkVisibilityStats result;
    result.mode = visibility.mode;
    result.radius_chunks = std::max(0, visibility.radius_chunks);
    result.fade_ring_chunks = std::max(0, visibility.fade_ring_chunks);
    if (!IsUploaded() || !build_result.info.IsValid()) {
        return result;
    }

    const ChunkCoord camera_chunk = CameraChunkCoord(camera, build_result.info);
    result.resident_chunks = static_cast<std::uint64_t>(chunks_.size());
    for (const RaylibUploadedChunkModel& chunk : chunks_) {
        result.total_faces += chunk.faces;
        const ChunkVisibilityClass visibility_class = ClassifyChunkVisibility(chunk.coord, camera_chunk, visibility);
        if (visibility_class == ChunkVisibilityClass::kHidden) {
            ++result.hidden_chunks;
            ++result.culled_models;
            result.culled_faces += chunk.faces;
            continue;
        }

        ++result.drawn_models;
        result.drawn_faces += chunk.faces;
        if (visibility_class == ChunkVisibilityClass::kFade) {
            ++result.fade_chunks;
        } else {
            ++result.visible_chunks;
        }
    }
    return result;
}

void RaylibChunkMeshPreview::Unload()
{
    for (RaylibUploadedChunkModel& chunk : chunks_) {
        if (chunk.model.meshCount > 0 && chunk.model.meshes != nullptr) {
            UnloadModel(chunk.model);
        }
    }
    chunks_.clear();
    stats_ = RaylibChunkMeshPreviewStats{};
}

bool RaylibChunkMeshPreview::IsUploaded() const
{
    return stats_.uploaded && !chunks_.empty();
}

const RaylibChunkMeshPreviewStats& RaylibChunkMeshPreview::Stats() const
{
    return stats_;
}

std::string ToLogString(const RaylibChunkMeshPreviewStats& stats)
{
    std::ostringstream out;
    out << "status=" << (stats.IsValid() ? "loaded" : "unavailable");
    out << " models=" << stats.models;
    out << " faces=" << stats.faces;
    out << " vertices=" << stats.vertices;
    out << " indices=" << stats.indices;
    if (stats.skipped_chunks > 0) {
        out << " skipped_chunks=" << stats.skipped_chunks;
    }
    return out.str();
}

std::string ToLogString(const RaylibChunkVisibilityStats& stats)
{
    std::ostringstream out;
    out << "mode=" << ToString(stats.mode);
    out << " radius=" << stats.radius_chunks;
    out << " fade_ring=" << stats.fade_ring_chunks;
    out << " resident=" << stats.resident_chunks;
    out << " visible=" << stats.visible_chunks;
    out << " fade=" << stats.fade_chunks;
    out << " hidden=" << stats.hidden_chunks;
    out << " drawn_models=" << stats.drawn_models << '/' << stats.resident_chunks;
    out << " faces=" << stats.drawn_faces << '/' << stats.total_faces;
    out << " draw_saved=" << std::fixed << std::setprecision(1) << stats.DrawSavedRatio() * 100.0 << '%';
    out << " face_saved=" << std::fixed << std::setprecision(1) << stats.FaceSavedRatio() * 100.0 << '%';
    return out.str();
}

}  // namespace vox3d
