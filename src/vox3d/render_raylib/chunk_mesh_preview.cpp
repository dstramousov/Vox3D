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
#include <optional>
#include <string_view>
#include <sstream>

namespace vox3d {

std::string_view ToString(RaylibChunkMeshColorMode mode)
{
    switch (mode) {
        case RaylibChunkMeshColorMode::kTraversal:
            return "traversal";
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
        case RaylibChunkVisibilityMode::kFrustumCull:
            return "frustum_cull";
    }
    return "unknown";
}

namespace {

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


[[nodiscard]] RgbaColor TraversalBaseColor(TerrainSurfaceKind kind, BlockTypeId fallback_type)
{
    switch (kind) {
        case TerrainSurfaceKind::kWalkableGround:
            return RgbaColor{73, 151, 73, 255};
        case TerrainSurfaceKind::kWalkableSlow:
            return RgbaColor{180, 148, 76, 255};
        case TerrainSurfaceKind::kBlockedTerrain:
            return RgbaColor{111, 70, 58, 255};
        case TerrainSurfaceKind::kWaterWetTerrain:
            return RgbaColor{40, 101, 155, 255};
        case TerrainSurfaceKind::kStructuralDepth:
            return RgbaColor{119, 72, 176, 255};
        case TerrainSurfaceKind::kTreeBlocker:
            return RgbaColor{31, 95, 45, 255};
        case TerrainSurfaceKind::kStart:
            return RgbaColor{239, 235, 214, 255};
        case TerrainSurfaceKind::kGoal:
            return RgbaColor{242, 208, 73, 255};
        case TerrainSurfaceKind::kUnknown:
            break;
    }
    return BaseColor(fallback_type);
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
        case RaylibChunkMeshColorMode::kTraversal:
            return TraversalBaseColor(vertex.surface_kind, vertex.block_type);
        case RaylibChunkMeshColorMode::kGeographic:
            return GeographicBaseColor(vertex.level);
        case RaylibChunkMeshColorMode::kChunkId:
            return ChunkBaseColor(chunk_coord);
        case RaylibChunkMeshColorMode::kFaceType:
            return FaceTypeBaseColor(vertex.face_direction);
    }
    return TraversalBaseColor(vertex.surface_kind, vertex.block_type);
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


[[nodiscard]] ChunkVisibilityMode ToCoreVisibilityMode(RaylibChunkVisibilityMode mode)
{
    switch (mode) {
        case RaylibChunkVisibilityMode::kAllChunks:
            return ChunkVisibilityMode::kAllChunks;
        case RaylibChunkVisibilityMode::kRadiusFade:
            return ChunkVisibilityMode::kRadiusFade;
        case RaylibChunkVisibilityMode::kHardCull:
            return ChunkVisibilityMode::kHardCull;
        case RaylibChunkVisibilityMode::kFrustumCull:
            return ChunkVisibilityMode::kFrustumCull;
    }
    return ChunkVisibilityMode::kAllChunks;
}

[[nodiscard]] Vector3 Add(Vector3 lhs, Vector3 rhs)
{
    return Vector3{lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

[[nodiscard]] Vector3 Subtract(Vector3 lhs, Vector3 rhs)
{
    return Vector3{lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

[[nodiscard]] Vector3 Scale(Vector3 value, float scale)
{
    return Vector3{value.x * scale, value.y * scale, value.z * scale};
}

[[nodiscard]] float Dot(Vector3 lhs, Vector3 rhs)
{
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

[[nodiscard]] Vector3 Cross(Vector3 lhs, Vector3 rhs)
{
    return Vector3{
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    };
}

[[nodiscard]] float Length(Vector3 value)
{
    return std::sqrt(Dot(value, value));
}

[[nodiscard]] Vector3 Normalize(Vector3 value, Vector3 fallback)
{
    const float length = Length(value);
    if (length <= 0.0001F) {
        return fallback;
    }
    return Scale(value, 1.0F / length);
}

[[nodiscard]] Vec3f ToCoreVector(Vector3 value)
{
    return Vec3f{value.x, value.y, value.z};
}

[[nodiscard]] Plane3f PlaneFromPoints(Vector3 a, Vector3 b, Vector3 c, Vector3 inside_point)
{
    Vector3 normal = Normalize(Cross(Subtract(b, a), Subtract(c, a)), Vector3{0.0F, 1.0F, 0.0F});
    float distance = -Dot(normal, a);
    if (Dot(normal, inside_point) + distance < 0.0F) {
        normal = Scale(normal, -1.0F);
        distance = -distance;
    }
    return Plane3f{ToCoreVector(normal), distance};
}

[[nodiscard]] float FrustumFarDistance(const Camera3D& camera, const ChunkMeshBuildInfo& info)
{
    const float map_width = static_cast<float>(std::max(1, info.map_width));
    const float map_height = static_cast<float>(std::max(1, info.map_height));
    float level_span = 16.0F;
    if (info.levels.has_value()) {
        level_span = static_cast<float>(std::max(1, info.levels->max - info.levels->min + 1));
    }

    const float map_diagonal = std::sqrt(map_width * map_width + map_height * map_height + level_span * level_span);
    const float camera_distance = Length(Subtract(camera.target, camera.position));
    return std::max(128.0F, map_diagonal * 3.0F + camera_distance);
}

[[nodiscard]] Frustum3f BuildCameraFrustum(
    const Camera3D& camera,
    const ChunkMeshBuildInfo& info,
    float viewport_aspect_ratio)
{
    constexpr float kNearDistance = 0.1F;
    constexpr float kDegToRad = 3.14159265358979323846F / 180.0F;

    const float aspect = std::max(0.1F, viewport_aspect_ratio);
    const float far_distance = FrustumFarDistance(camera, info);
    const Vector3 forward = Normalize(Subtract(camera.target, camera.position), Vector3{0.0F, 0.0F, -1.0F});
    const Vector3 right = Normalize(Cross(forward, camera.up), Vector3{1.0F, 0.0F, 0.0F});
    const Vector3 up = Normalize(Cross(right, forward), Vector3{0.0F, 1.0F, 0.0F});

    const float half_fov = camera.fovy * 0.5F * kDegToRad;
    const float near_half_height = std::tan(half_fov) * kNearDistance;
    const float near_half_width = near_half_height * aspect;
    const float far_half_height = std::tan(half_fov) * far_distance;
    const float far_half_width = far_half_height * aspect;

    const Vector3 near_center = Add(camera.position, Scale(forward, kNearDistance));
    const Vector3 far_center = Add(camera.position, Scale(forward, far_distance));
    const Vector3 inside = Add(camera.position, Scale(forward, (kNearDistance + far_distance) * 0.5F));

    const Vector3 ntl = Add(Add(near_center, Scale(up, near_half_height)), Scale(right, -near_half_width));
    const Vector3 ntr = Add(Add(near_center, Scale(up, near_half_height)), Scale(right, near_half_width));
    const Vector3 nbl = Add(Add(near_center, Scale(up, -near_half_height)), Scale(right, -near_half_width));
    const Vector3 nbr = Add(Add(near_center, Scale(up, -near_half_height)), Scale(right, near_half_width));

    const Vector3 ftl = Add(Add(far_center, Scale(up, far_half_height)), Scale(right, -far_half_width));
    const Vector3 ftr = Add(Add(far_center, Scale(up, far_half_height)), Scale(right, far_half_width));
    const Vector3 fbl = Add(Add(far_center, Scale(up, -far_half_height)), Scale(right, -far_half_width));
    const Vector3 fbr = Add(Add(far_center, Scale(up, -far_half_height)), Scale(right, far_half_width));

    Frustum3f frustum;
    frustum.valid = true;
    frustum.planes = std::array<Plane3f, 6>{
        PlaneFromPoints(ntl, ntr, nbr, inside),
        PlaneFromPoints(ftr, ftl, fbl, inside),
        PlaneFromPoints(ntl, nbl, fbl, inside),
        PlaneFromPoints(nbr, ntr, fbr, inside),
        PlaneFromPoints(ntr, ntl, ftl, inside),
        PlaneFromPoints(nbl, nbr, fbr, inside),
    };
    return frustum;
}

[[nodiscard]] ChunkVisibilityOptions BuildCoreVisibilityOptions(
    const ChunkMeshBuildResult& build_result,
    const Camera3D& camera,
    RaylibChunkVisibilityOptions visibility)
{
    ChunkVisibilityOptions options;
    options.mode = ToCoreVisibilityMode(visibility.mode);
    options.camera_chunk = CameraChunkCoord(camera, build_result.info);
    options.radius_chunks = visibility.radius_chunks;
    options.fade_ring_chunks = visibility.fade_ring_chunks;
    if (options.mode == ChunkVisibilityMode::kFrustumCull) {
        options.frustum = BuildCameraFrustum(camera, build_result.info, visibility.viewport_aspect_ratio);
    }
    return options;
}

struct Ray3f {
    Vector3 origin{};
    Vector3 direction{};
};

[[nodiscard]] float CurrentRenderAspectRatio()
{
    const int width = std::max(1, GetScreenWidth());
    const int height = std::max(1, GetScreenHeight());
    return std::max(0.1F, static_cast<float>(width) / static_cast<float>(height));
}

[[nodiscard]] Ray3f BuildScreenRay(Vector2 screen_position, const Camera3D& camera)
{
    constexpr float kDegToRad = 3.14159265358979323846F / 180.0F;

    // BeginMode3D builds its projection from the full render target size, even
    // when drawing is clipped by BeginScissorMode. Picking must mirror that
    // projection; otherwise the ray is shifted relative to the visible image.
    const float screen_width = static_cast<float>(std::max(1, GetScreenWidth()));
    const float screen_height = static_cast<float>(std::max(1, GetScreenHeight()));
    const float ndc_x = (screen_position.x / screen_width) * 2.0F - 1.0F;
    const float ndc_y = 1.0F - (screen_position.y / screen_height) * 2.0F;
    const float aspect = CurrentRenderAspectRatio();
    const float half_vertical = std::tan(camera.fovy * 0.5F * kDegToRad);

    const Vector3 forward = Normalize(Subtract(camera.target, camera.position), Vector3{0.0F, 0.0F, -1.0F});
    const Vector3 right = Normalize(Cross(forward, camera.up), Vector3{1.0F, 0.0F, 0.0F});
    const Vector3 up = Normalize(Cross(right, forward), Vector3{0.0F, 1.0F, 0.0F});
    const Vector3 direction = Normalize(
        Add(Add(forward, Scale(right, ndc_x * half_vertical * aspect)), Scale(up, ndc_y * half_vertical)),
        forward);
    return Ray3f{camera.position, direction};
}

[[nodiscard]] bool TileFromWorldPoint(Vector3 point, const RuntimeMap& map, TileCoord& tile)
{
    const int x = static_cast<int>(std::floor(point.x + static_cast<float>(map.info.width) * 0.5F));
    const int y = static_cast<int>(std::floor(static_cast<float>(map.info.height) * 0.5F - point.z));
    const TileCoord candidate{x, y};
    if (!map.IsValid() || candidate.x < 0 || candidate.y < 0
        || candidate.x >= map.info.width || candidate.y >= map.info.height) {
        return false;
    }
    tile = candidate;
    return true;
}

[[nodiscard]] float TerrainTopLevel(const RuntimeMap& map, TileCoord tile)
{
    if (!map.height.IsValid() || !map.height.Contains(tile)) {
        return 0.0F;
    }
    const auto index = static_cast<std::size_t>(tile.y) * static_cast<std::size_t>(map.info.width)
        + static_cast<std::size_t>(tile.x);
    return static_cast<float>(map.height.cells[index] + 1);
}

[[nodiscard]] float PickMaxDistance(const RuntimeMap& map, const Camera3D& camera)
{
    const float width = static_cast<float>(std::max(1, map.info.width));
    const float height = static_cast<float>(std::max(1, map.info.height));
    const float level_span = map.info.levels.has_value()
        ? static_cast<float>(std::max(1, map.info.levels->max - map.info.levels->min + 1))
        : 16.0F;
    const float map_diagonal = std::sqrt(width * width + height * height + level_span * level_span);
    return std::max(128.0F, map_diagonal * 3.0F + Length(Subtract(camera.target, camera.position)));
}

[[nodiscard]] std::optional<TileCoord> PickHeightfieldTile(const Ray3f& ray, const RuntimeMap& map, const Camera3D& camera)
{
    if (!map.height.IsValid()) {
        return std::nullopt;
    }

    constexpr float kStep = 0.25F;
    constexpr float kHitEpsilon = 0.06F;
    const float max_distance = PickMaxDistance(map, camera);
    for (float distance = 0.0F; distance <= max_distance; distance += kStep) {
        const Vector3 point = Add(ray.origin, Scale(ray.direction, distance));
        TileCoord tile;
        if (!TileFromWorldPoint(point, map, tile)) {
            continue;
        }
        if (point.y <= TerrainTopLevel(map, tile) + kHitEpsilon) {
            return tile;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<TileCoord> PickPlaneTile(const Ray3f& ray, const RuntimeMap& map)
{
    if (std::abs(ray.direction.y) <= 0.0001F) {
        return std::nullopt;
    }
    const float t = -ray.origin.y / ray.direction.y;
    if (t < 0.0F) {
        return std::nullopt;
    }

    TileCoord tile;
    if (!TileFromWorldPoint(Add(ray.origin, Scale(ray.direction, t)), map, tile)) {
        return std::nullopt;
    }
    return tile;
}

[[nodiscard]] Aabb3f ComputeWorldBounds(const ChunkMeshData& chunk, int map_width, int map_height)
{
    Aabb3f bounds{
        Vec3f{
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
        },
        Vec3f{
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
        },
    };

    for (const MeshVertex& vertex : chunk.vertices) {
        const Vector3 position = WorldPosition(vertex.position, map_width, map_height);
        bounds.min.x = std::min(bounds.min.x, position.x);
        bounds.min.y = std::min(bounds.min.y, position.y);
        bounds.min.z = std::min(bounds.min.z, position.z);
        bounds.max.x = std::max(bounds.max.x, position.x);
        bounds.max.y = std::max(bounds.max.y, position.y);
        bounds.max.z = std::max(bounds.max.z, position.z);
    }
    return bounds;
}

[[nodiscard]] RaylibChunkVisibilityStats ToRaylibVisibilityStats(
    const ChunkVisibilityReport& report,
    RaylibChunkVisibilityMode mode)
{
    RaylibChunkVisibilityStats result;
    result.mode = mode;
    result.radius_chunks = report.radius_chunks;
    result.fade_ring_chunks = report.fade_ring_chunks;
    result.resident_chunks = report.resident_chunks;
    result.resident_models = report.drawn_models + report.culled_models;
    result.visible_chunks = report.visible_chunks;
    result.fade_chunks = report.fade_chunks;
    result.hidden_chunks = report.hidden_chunks;
    result.drawn_models = report.drawn_models;
    result.culled_models = report.culled_models;
    result.total_faces = report.total_faces;
    result.drawn_faces = report.drawn_faces;
    result.culled_faces = report.culled_faces;
    return result;
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
    const ChunkVisibilityReport& visibility_report,
    bool show_hidden_bounds)
{
    if (visibility_report.mode == ChunkVisibilityMode::kAllChunks || !show_hidden_bounds) {
        return;
    }

    const float level = HiddenBoundsLevel(build_result);
    constexpr Color kHiddenBounds{255, 105, 90, 150};
    const std::size_t count = std::min(chunks.size(), visibility_report.entries.size());
    for (std::size_t index = 0; index < count; ++index) {
        if (visibility_report.entries[index].visibility_class != ChunkVisibilityClass::kHidden) {
            continue;
        }
        const RaylibUploadedChunkModel& chunk = chunks[index];
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


[[nodiscard]] bool IsTerrainPassEnabled(TerrainRenderPass pass, RaylibTerrainPassOptions options)
{
    switch (pass) {
        case TerrainRenderPass::kBody:
            return true;
        case TerrainRenderPass::kTops:
            return options.show_tops;
        case TerrainRenderPass::kWalls:
            return options.show_walls;
        case TerrainRenderPass::kCliffs:
            return options.show_cliffs;
    }
    return true;
}

[[nodiscard]] std::array<TerrainRenderPass, 3> TerrainDrawPasses()
{
    return {TerrainRenderPass::kTops, TerrainRenderPass::kWalls, TerrainRenderPass::kCliffs};
}

[[nodiscard]] ChunkMeshData ExtractTerrainPassMesh(const ChunkMeshData& chunk, TerrainRenderPass pass)
{
    ChunkMeshData result;
    result.coord = chunk.coord;
    result.bounds = chunk.bounds;

    result.faces.reserve(chunk.faces.size());
    result.vertices.reserve(chunk.vertices.size());
    result.indices.reserve(chunk.indices.size());

    for (const MeshFace& face : chunk.faces) {
        if (face.terrain_pass != pass || face.first_vertex + 3U >= chunk.vertices.size()) {
            continue;
        }

        const auto first_vertex = static_cast<std::uint32_t>(result.vertices.size());
        const auto first_index = static_cast<std::uint32_t>(result.indices.size());
        MeshFace copied = face;
        copied.first_vertex = first_vertex;
        copied.first_index = first_index;
        result.faces.push_back(copied);

        for (std::uint32_t offset = 0; offset < 4U; ++offset) {
            result.vertices.push_back(chunk.vertices[face.first_vertex + offset]);
        }

        result.indices.push_back(first_vertex + 0U);
        result.indices.push_back(first_vertex + 1U);
        result.indices.push_back(first_vertex + 2U);
        result.indices.push_back(first_vertex + 0U);
        result.indices.push_back(first_vertex + 2U);
        result.indices.push_back(first_vertex + 3U);
    }

    return result;
}

void AccumulateUploadStats(const ChunkMeshData& mesh, RaylibChunkMeshPreviewStats& stats)
{
    ++stats.models;
    stats.faces += static_cast<std::uint64_t>(mesh.faces.size());
    stats.vertices += static_cast<std::uint64_t>(mesh.vertices.size());
    stats.indices += static_cast<std::uint64_t>(mesh.indices.size());
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

[[nodiscard]] Color ObjectMarkerColor(RuntimeObjectMarkerKind kind)
{
    switch (kind) {
        case RuntimeObjectMarkerKind::kTree:
            return Color{24, 118, 54, 235};
        case RuntimeObjectMarkerKind::kBush:
            return Color{78, 168, 72, 225};
        case RuntimeObjectMarkerKind::kReed:
            return Color{181, 196, 72, 220};
        case RuntimeObjectMarkerKind::kRuin:
            return Color{155, 150, 142, 235};
        case RuntimeObjectMarkerKind::kCover:
            return Color{172, 128, 78, 235};
        case RuntimeObjectMarkerKind::kLoot:
            return Color{86, 196, 236, 240};
        case RuntimeObjectMarkerKind::kStructure:
            return Color{132, 132, 122, 240};
        case RuntimeObjectMarkerKind::kTrench:
            return Color{122, 88, 48, 230};
        case RuntimeObjectMarkerKind::kUnknown:
            break;
    }
    return Color{220, 94, 184, 220};
}

[[nodiscard]] Vector3 ObjectMarkerSize()
{
    constexpr float kMarkerCubeSize = 0.34F;
    return Vector3{kMarkerCubeSize, kMarkerCubeSize, kMarkerCubeSize};
}

[[nodiscard]] std::vector<std::uint8_t> BuildVisibleChunkMask(
    const ChunkMeshBuildInfo& info,
    const ChunkVisibilityReport& report)
{
    const int chunks_x = std::max(1, info.chunks_x);
    const int chunks_y = std::max(1, info.chunks_y);
    std::vector<std::uint8_t> visible(static_cast<std::size_t>(chunks_x) * static_cast<std::size_t>(chunks_y), 0U);
    for (const ChunkVisibilityEntry& entry : report.entries) {
        if (entry.visibility_class == ChunkVisibilityClass::kHidden) {
            continue;
        }
        if (entry.coord.x < 0 || entry.coord.y < 0 || entry.coord.x >= chunks_x || entry.coord.y >= chunks_y) {
            continue;
        }
        const auto index = static_cast<std::size_t>(entry.coord.y) * static_cast<std::size_t>(chunks_x)
            + static_cast<std::size_t>(entry.coord.x);
        visible[index] = 1U;
    }
    return visible;
}

[[nodiscard]] bool IsMarkerChunkVisible(
    const RuntimeObjectMarker& marker,
    const ChunkMeshBuildInfo& info,
    const std::vector<std::uint8_t>& visible_chunks)
{
    const int chunk_size_x = std::max(1, info.chunk_size_x);
    const int chunk_size_y = std::max(1, info.chunk_size_y);
    const int chunks_x = std::max(1, info.chunks_x);
    const int chunks_y = std::max(1, info.chunks_y);
    const int chunk_x = ClampInt(marker.tile.x / chunk_size_x, 0, chunks_x - 1);
    const int chunk_y = ClampInt(marker.tile.y / chunk_size_y, 0, chunks_y - 1);
    const auto index = static_cast<std::size_t>(chunk_y) * static_cast<std::size_t>(chunks_x)
        + static_cast<std::size_t>(chunk_x);
    return index < visible_chunks.size() && visible_chunks[index] != 0U;
}

[[nodiscard]] bool IsObjectMarkerKindEnabled(RuntimeObjectMarkerKind kind, RaylibChunkMeshDebugOverlayOptions overlays)
{
    switch (kind) {
        case RuntimeObjectMarkerKind::kTree:
            return overlays.show_object_trees;
        case RuntimeObjectMarkerKind::kBush:
            return overlays.show_object_bushes;
        case RuntimeObjectMarkerKind::kReed:
            return overlays.show_object_reeds;
        case RuntimeObjectMarkerKind::kRuin:
            return overlays.show_object_ruins;
        case RuntimeObjectMarkerKind::kCover:
            return overlays.show_object_cover;
        case RuntimeObjectMarkerKind::kLoot:
            return overlays.show_object_loot;
        case RuntimeObjectMarkerKind::kStructure:
            return overlays.show_object_structures;
        case RuntimeObjectMarkerKind::kTrench:
            return overlays.show_object_trenches;
        case RuntimeObjectMarkerKind::kUnknown:
            return overlays.show_object_unknown;
    }
    return false;
}

[[nodiscard]] bool HasAnyObjectMarkerFilter(RaylibChunkMeshDebugOverlayOptions overlays)
{
    return overlays.show_object_trees
        || overlays.show_object_bushes
        || overlays.show_object_reeds
        || overlays.show_object_ruins
        || overlays.show_object_cover
        || overlays.show_object_loot
        || overlays.show_object_structures
        || overlays.show_object_trenches
        || overlays.show_object_unknown;
}

void DrawObjectMarkersOverlay(
    const RuntimeMap& map,
    const ChunkMeshBuildResult& build_result,
    const ChunkVisibilityReport& visibility_report,
    RaylibChunkMeshDebugOverlayOptions overlays)
{
    if (!map.info.object_markers_loaded || !map.height.IsValid() || !build_result.info.IsValid()
        || !HasAnyObjectMarkerFilter(overlays)) {
        return;
    }

    const std::vector<std::uint8_t> visible_chunks = BuildVisibleChunkMask(build_result.info, visibility_report);
    for (const RuntimeObjectMarker& marker : map.object_markers) {
        if (!IsObjectMarkerKindEnabled(marker.kind, overlays)
            || !map.height.Contains(marker.tile)
            || !IsMarkerChunkVisible(marker, build_result.info, visible_chunks)) {
            continue;
        }

        const Color color = ObjectMarkerColor(marker.kind);
        const Vector3 size = ObjectMarkerSize();
        const float terrain_level = TerrainTopLevel(map, marker.tile);
        const Vector3 base = TileCenterWorld(
            marker.tile.x,
            marker.tile.y,
            terrain_level + 0.20F,
            build_result.info.map_width,
            build_result.info.map_height);
        const Vector3 center{base.x, base.y + size.y * 0.5F, base.z};
        DrawCubeV(center, size, color);
    }
}

[[nodiscard]] bool IsTransitionKindEnabled(TransitionFeatureKind kind, RaylibTransitionOverlayOptions options)
{
    switch (kind) {
        case TransitionFeatureKind::kRamp:
            return options.show_ramps;
        case TransitionFeatureKind::kStairs:
            return options.show_stairs;
        case TransitionFeatureKind::kBridge:
            return options.show_bridges;
        case TransitionFeatureKind::kDrop:
            return options.show_drops;
    }
    return false;
}

[[nodiscard]] Color TransitionFeatureColor(TransitionFeatureKind kind, bool passable)
{
    if (!passable && kind != TransitionFeatureKind::kDrop) {
        return Color{170, 170, 170, 205};
    }
    switch (kind) {
        case TransitionFeatureKind::kRamp:
            return Color{70, 230, 110, 230};
        case TransitionFeatureKind::kStairs:
            return Color{248, 214, 74, 230};
        case TransitionFeatureKind::kBridge:
            return Color{86, 210, 235, 230};
        case TransitionFeatureKind::kDrop:
            return Color{244, 92, 74, 230};
    }
    return Color{220, 220, 220, 220};
}

void DrawTransitionFeatureOverlay(
    const TransitionFeatureSet& features,
    const ChunkMeshBuildResult& build_result,
    RaylibTransitionOverlayOptions options)
{
    if (!options.show || !features.IsValid()) {
        return;
    }

    constexpr float kLevelOffset = 1.28F;
    constexpr float kMarkerSize = 0.18F;
    for (const TransitionFeature& feature : features.features) {
        if (!IsTransitionKindEnabled(feature.kind, options)) {
            continue;
        }

        const float from_level = static_cast<float>(feature.from_level + 1) + kLevelOffset;
        const float to_level = static_cast<float>(feature.to_level + 1) + kLevelOffset;
        const Vector3 from = TileCenterWorld(
            feature.from_tile.x,
            feature.from_tile.y,
            from_level,
            build_result.info.map_width,
            build_result.info.map_height);
        const Vector3 to = TileCenterWorld(
            feature.to_tile.x,
            feature.to_tile.y,
            to_level,
            build_result.info.map_width,
            build_result.info.map_height);
        const Color color = TransitionFeatureColor(feature.kind, feature.passable);
        const Vector3 middle{
            (from.x + to.x) * 0.5F,
            (from.y + to.y) * 0.5F,
            (from.z + to.z) * 0.5F,
        };

        DrawLine3D(from, to, color);
        DrawSphere(middle, kMarkerSize, color);
    }
}

void DrawSelectedTileOverlay(
    const RuntimeMap& map,
    const ChunkMeshBuildResult& build_result,
    RaylibTileSelectionOverlayOptions selected_tile)
{
    if (!selected_tile.show || !map.height.Contains(selected_tile.tile)) {
        return;
    }

    const float level = TerrainTopLevel(map, selected_tile.tile) + 0.10F;
    const float min_x = static_cast<float>(selected_tile.tile.x);
    const float min_y = static_cast<float>(selected_tile.tile.y);
    const float max_x = min_x + 1.0F;
    const float max_y = min_y + 1.0F;
    const Vector3 nw = TileCornerWorld(min_x, min_y, level, build_result.info.map_width, build_result.info.map_height);
    const Vector3 ne = TileCornerWorld(max_x, min_y, level, build_result.info.map_width, build_result.info.map_height);
    const Vector3 se = TileCornerWorld(max_x, max_y, level, build_result.info.map_width, build_result.info.map_height);
    const Vector3 sw = TileCornerWorld(min_x, max_y, level, build_result.info.map_width, build_result.info.map_height);

    constexpr Color kSelectionOuter{255, 245, 110, 255};
    constexpr Color kSelectionInner{20, 28, 32, 240};
    DrawLine3D(nw, ne, kSelectionOuter);
    DrawLine3D(ne, se, kSelectionOuter);
    DrawLine3D(se, sw, kSelectionOuter);
    DrawLine3D(sw, nw, kSelectionOuter);

    const Vector3 marker_bottom = TileCenterWorld(
        selected_tile.tile.x,
        selected_tile.tile.y,
        level,
        build_result.info.map_width,
        build_result.info.map_height);
    const Vector3 marker_top{marker_bottom.x, marker_bottom.y + 1.2F, marker_bottom.z};
    DrawLine3D(marker_bottom, marker_top, kSelectionOuter);
    DrawSphere(marker_top, 0.14F, kSelectionOuter);
    DrawSphere(marker_bottom, 0.08F, kSelectionInner);
}

[[nodiscard]] Color MovementProbeColor(const MovementProbeStep& step)
{
    if (step.passable) {
        return Color{92, 232, 118, 235};
    }
    if (step.block_reason == MovementBlockReason::kOutOfBounds) {
        return Color{130, 130, 140, 210};
    }
    if (step.block_reason == MovementBlockReason::kDrop) {
        return Color{246, 116, 56, 235};
    }
    return Color{236, 74, 74, 235};
}

void DrawMovementProbeOverlay(
    const RuntimeMap& map,
    const ChunkMeshBuildResult& build_result,
    const MovementProbeResult& probe,
    RaylibMovementProbeOverlayOptions options)
{
    if (!options.show || !probe.IsValid() || !map.height.Contains(probe.source_tile)) {
        return;
    }

    constexpr float kLevelOffset = 0.55F;
    constexpr float kSourceSize = 0.16F;
    constexpr float kTargetSize = 0.13F;
    const Vector3 source = TileCenterWorld(
        probe.source_tile.x,
        probe.source_tile.y,
        static_cast<float>(probe.source_elevation + 1) + kLevelOffset,
        build_result.info.map_width,
        build_result.info.map_height);
    DrawSphere(source, kSourceSize, Color{255, 245, 110, 245});

    for (int index = 0; index < probe.step_count; ++index) {
        const MovementProbeStep& step = probe.steps[static_cast<std::size_t>(index)];
        if (!step.target_in_bounds || !map.height.Contains(step.to_tile)) {
            continue;
        }

        const Color color = MovementProbeColor(step);
        const Vector3 target = TileCenterWorld(
            step.to_tile.x,
            step.to_tile.y,
            static_cast<float>(step.to_elevation + 1) + kLevelOffset,
            build_result.info.map_width,
            build_result.info.map_height);
        const Vector3 middle{
            (source.x + target.x) * 0.5F,
            (source.y + target.y) * 0.5F + 0.08F,
            (source.z + target.z) * 0.5F,
        };

        DrawLine3D(source, middle, color);
        DrawLine3D(middle, target, color);
        DrawSphere(target, kTargetSize, color);
        if (!step.passable) {
            const Vector3 mark_top{target.x, target.y + 0.35F, target.z};
            DrawLine3D(target, mark_top, color);
        }
    }
}

[[nodiscard]] bool IsPassabilityIssueEnabled(
    PassabilityIssueKind kind,
    RaylibPassabilityValidationOverlayOptions options)
{
    switch (kind) {
        case PassabilityIssueKind::kInvalidTransition:
            return options.show_invalid_transitions;
        case PassabilityIssueKind::kBlockedRamp:
        case PassabilityIssueKind::kBlockedStairs:
            return options.show_blocked_transitions;
        case PassabilityIssueKind::kSuspiciousDrop:
            return options.show_suspicious_drops;
        case PassabilityIssueKind::kIsolatedTile:
            return options.show_isolated_tiles;
    }
    return false;
}

[[nodiscard]] Color PassabilityIssueColor(PassabilityIssueKind kind)
{
    switch (kind) {
        case PassabilityIssueKind::kInvalidTransition:
            return Color{224, 64, 232, 240};
        case PassabilityIssueKind::kBlockedRamp:
        case PassabilityIssueKind::kBlockedStairs:
            return Color{255, 196, 54, 240};
        case PassabilityIssueKind::kSuspiciousDrop:
            return Color{255, 98, 44, 240};
        case PassabilityIssueKind::kIsolatedTile:
            return Color{160, 100, 255, 240};
    }
    return Color{255, 255, 255, 220};
}


void DrawPathProbeOverlay(
    const RuntimeMap& map,
    const ChunkMeshBuildResult& build_result,
    const PathProbeResult& path,
    RaylibPathProbeOverlayOptions options)
{
    if (!path.IsValid()) {
        return;
    }

    constexpr float kRouteOffset = 2.05F;
    constexpr float kVisitedOffset = 1.82F;
    constexpr float kRouteSize = 0.09F;
    constexpr Color kStartColor{76, 235, 116, 245};
    constexpr Color kGoalColor{86, 170, 255, 245};
    constexpr Color kRouteColor{255, 222, 72, 245};
    constexpr Color kVisitedColor{90, 210, 245, 100};
    constexpr Color kMissingColor{245, 82, 72, 235};

    if (options.show_visited) {
        for (TileCoord tile : path.visited_tiles) {
            if (!map.height.Contains(tile)) {
                continue;
            }
            const Vector3 marker = TileCenterWorld(
                tile.x,
                tile.y,
                TerrainTopLevel(map, tile) + kVisitedOffset,
                build_result.info.map_width,
                build_result.info.map_height);
            DrawSphere(marker, 0.035F, kVisitedColor);
        }
    }

    if (options.show_path && path.HasPath()) {
        Vector3 previous{};
        bool has_previous = false;
        for (const PathStep& step : path.path) {
            if (!map.height.Contains(step.tile)) {
                continue;
            }
            const Vector3 current = TileCenterWorld(
                step.tile.x,
                step.tile.y,
                TerrainTopLevel(map, step.tile) + kRouteOffset,
                build_result.info.map_width,
                build_result.info.map_height);
            if (has_previous) {
                DrawLine3D(previous, current, kRouteColor);
            }
            DrawSphere(current, kRouteSize, kRouteColor);
            previous = current;
            has_previous = true;
        }
    }

    if (map.height.Contains(path.start)) {
        const Vector3 start = TileCenterWorld(
            path.start.x,
            path.start.y,
            TerrainTopLevel(map, path.start) + kRouteOffset + 0.20F,
            build_result.info.map_width,
            build_result.info.map_height);
        DrawSphere(start, 0.18F, kStartColor);
    }
    if (map.height.Contains(path.goal)) {
        const Vector3 goal = TileCenterWorld(
            path.goal.x,
            path.goal.y,
            TerrainTopLevel(map, path.goal) + kRouteOffset + 0.20F,
            build_result.info.map_width,
            build_result.info.map_height);
        DrawSphere(goal, 0.18F, path.HasPath() ? kGoalColor : kMissingColor);
    }
}

void DrawPassabilityValidationOverlay(
    const RuntimeMap& map,
    const ChunkMeshBuildResult& build_result,
    const PassabilityValidationReport& report,
    RaylibPassabilityValidationOverlayOptions options)
{
    if (!options.show || !report.IsValid()) {
        return;
    }

    constexpr float kLevelOffset = 1.65F;
    constexpr float kIssueSize = 0.16F;
    for (const PassabilityIssue& issue : report.issues) {
        if (!IsPassabilityIssueEnabled(issue.kind, options) || !map.height.Contains(issue.from_tile)) {
            continue;
        }

        const Color color = PassabilityIssueColor(issue.kind);
        const Vector3 from = TileCenterWorld(
            issue.from_tile.x,
            issue.from_tile.y,
            static_cast<float>(issue.from_elevation + 1) + kLevelOffset,
            build_result.info.map_width,
            build_result.info.map_height);

        if (issue.kind == PassabilityIssueKind::kIsolatedTile || !map.height.Contains(issue.to_tile)
            || (issue.from_tile.x == issue.to_tile.x && issue.from_tile.y == issue.to_tile.y)) {
            DrawSphere(from, kIssueSize * 1.25F, color);
            const Vector3 top{from.x, from.y + 0.45F, from.z};
            DrawLine3D(from, top, color);
            continue;
        }

        const Vector3 to = TileCenterWorld(
            issue.to_tile.x,
            issue.to_tile.y,
            static_cast<float>(issue.to_elevation + 1) + kLevelOffset,
            build_result.info.map_width,
            build_result.info.map_height);
        const Vector3 middle{
            (from.x + to.x) * 0.5F,
            (from.y + to.y) * 0.5F + 0.14F,
            (from.z + to.z) * 0.5F,
        };

        DrawLine3D(from, middle, color);
        DrawLine3D(middle, to, color);
        DrawSphere(middle, kIssueSize, color);
    }
}

void DrawDebugOverlays(
    const ChunkMeshBuildResult& build_result,
    const RuntimeMap* runtime_map,
    const ChunkGrid* chunk_grid,
    const ChunkVisibilityReport& visibility_report,
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
    DrawObjectMarkersOverlay(*runtime_map, build_result, visibility_report, overlays);
}

}  // namespace

bool RaylibChunkVisibilityStats::IsValid() const
{
    return resident_chunks > 0 && resident_models > 0 && resident_models == drawn_models + culled_models;
}

double RaylibChunkVisibilityStats::DrawSavedRatio() const
{
    return resident_models == 0 ? 0.0 : static_cast<double>(culled_models) / static_cast<double>(resident_models);
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
    return UploadAdditional(build_result, color_mode);
}

bool RaylibChunkMeshPreview::UploadAdditional(
    const ChunkMeshBuildResult& build_result,
    RaylibChunkMeshColorMode color_mode)
{
    if (!build_result.IsValid()) {
        return false;
    }

    const bool split_terrain_passes = build_result.info.mode == ChunkMeshBuildMode::kTerrainSurface;
    chunks_.reserve(chunks_.size() + build_result.chunks.size() * (split_terrain_passes ? 3ULL : 1ULL));
    visibility_items_.reserve(visibility_items_.size() + build_result.chunks.size());
    const std::uint64_t before_models = stats_.models;
    for (const ChunkMeshData& chunk : build_result.chunks) {
        if (chunk.faces.empty()) {
            continue;
        }

        const Aabb3f world_bounds = ComputeWorldBounds(chunk, build_result.info.map_width, build_result.info.map_height);
        const auto visibility_item_index = visibility_items_.size();
        visibility_items_.push_back(ChunkVisibilityItem{chunk.coord, world_bounds, chunk.FaceCount()});

        if (split_terrain_passes) {
            for (TerrainRenderPass pass : TerrainDrawPasses()) {
                ChunkMeshData pass_mesh = ExtractTerrainPassMesh(chunk, pass);
                if (pass_mesh.faces.empty()) {
                    continue;
                }
                if (!CanUploadChunk(pass_mesh)) {
                    ++stats_.skipped_chunks;
                    continue;
                }

                Model model = LoadChunkModel(pass_mesh, build_result.info.map_width, build_result.info.map_height, color_mode);
                if (model.meshCount <= 0 || model.meshes == nullptr) {
                    ++stats_.skipped_chunks;
                    continue;
                }

                chunks_.push_back(RaylibUploadedChunkModel{
                    model,
                    chunk.coord,
                    chunk.bounds,
                    world_bounds,
                    pass,
                    visibility_item_index,
                    pass_mesh.FaceCount(),
                });
                AccumulateUploadStats(pass_mesh, stats_);
            }
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

        chunks_.push_back(RaylibUploadedChunkModel{
            model,
            chunk.coord,
            chunk.bounds,
            world_bounds,
            TerrainRenderPass::kBody,
            visibility_item_index,
            chunk.FaceCount(),
        });
        AccumulateUploadStats(chunk, stats_);
    }

    stats_.uploaded = !chunks_.empty();
    return stats_.models > before_models;
}

void RaylibChunkMeshPreview::Draw(
    Rectangle viewport,
    const ChunkMeshBuildResult& build_result,
    const Camera3D& camera,
    const RuntimeMap* runtime_map,
    const ChunkGrid* chunk_grid,
    RaylibChunkMeshDebugOverlayOptions overlays,
    RaylibChunkVisibilityOptions visibility,
    RaylibTerrainPassOptions terrain_passes,
    const TransitionFeatureSet* transition_features,
    RaylibTransitionOverlayOptions transitions,
    RaylibTileSelectionOverlayOptions selected_tile,
    const MovementProbeResult* movement_probe,
    RaylibMovementProbeOverlayOptions movement,
    const PathProbeResult* path_probe,
    RaylibPathProbeOverlayOptions path_overlay,
    const PassabilityValidationReport* passability,
    RaylibPassabilityValidationOverlayOptions passability_overlay) const
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

    visibility.viewport_aspect_ratio = CurrentRenderAspectRatio();
    const ChunkVisibilityReport visibility_report = BuildChunkVisibility(
        visibility_items_,
        BuildCoreVisibilityOptions(build_result, camera, visibility));

    constexpr Vector3 kOrigin{0.0F, 0.0F, 0.0F};
    constexpr float kScale = 1.0F;
    for (const RaylibUploadedChunkModel& chunk : chunks_) {
        if (!IsTerrainPassEnabled(chunk.terrain_pass, terrain_passes)
            || chunk.visibility_item_index >= visibility_report.entries.size()) {
            continue;
        }
        const ChunkVisibilityClass visibility_class = visibility_report.entries[chunk.visibility_item_index].visibility_class;
        if (visibility_class == ChunkVisibilityClass::kHidden) {
            continue;
        }
        DrawModel(chunk.model, kOrigin, kScale, VisibilityTint(visibility_class));
    }

    DrawHiddenChunkBounds(chunks_, build_result, visibility_report, visibility.show_hidden_bounds);
    if (transition_features != nullptr) {
        DrawTransitionFeatureOverlay(*transition_features, build_result, transitions);
    }
    DrawDebugOverlays(build_result, runtime_map, chunk_grid, visibility_report, overlays);
    if (runtime_map != nullptr) {
        DrawSelectedTileOverlay(*runtime_map, build_result, selected_tile);
        if (movement_probe != nullptr) {
            DrawMovementProbeOverlay(*runtime_map, build_result, *movement_probe, movement);
        }
        if (path_probe != nullptr) {
            DrawPathProbeOverlay(*runtime_map, build_result, *path_probe, path_overlay);
        }
        if (passability != nullptr) {
            DrawPassabilityValidationOverlay(*runtime_map, build_result, *passability, passability_overlay);
        }
    }

    EndMode3D();
    EndScissorMode();
}

std::optional<TileCoord> RaylibChunkMeshPreview::PickTile(
    Vector2 screen_position,
    Rectangle viewport,
    const RuntimeMap& runtime_map,
    const Camera3D& camera) const
{
    if (!runtime_map.IsValid() || viewport.width <= 1.0F || viewport.height <= 1.0F
        || !CheckCollisionPointRec(screen_position, viewport)) {
        return std::nullopt;
    }

    const Ray3f ray = BuildScreenRay(screen_position, camera);
    if (std::optional<TileCoord> tile = PickHeightfieldTile(ray, runtime_map, camera); tile.has_value()) {
        return tile;
    }
    return PickPlaneTile(ray, runtime_map);
}

RaylibChunkVisibilityStats RaylibChunkMeshPreview::CalculateVisibilityStats(
    const ChunkMeshBuildResult& build_result,
    const Camera3D& camera,
    RaylibChunkVisibilityOptions visibility,
    RaylibTerrainPassOptions terrain_passes) const
{
    RaylibChunkVisibilityStats result;
    result.mode = visibility.mode;
    result.radius_chunks = std::max(0, visibility.radius_chunks);
    result.fade_ring_chunks = std::max(0, visibility.fade_ring_chunks);
    if (!IsUploaded() || !build_result.info.IsValid()) {
        return result;
    }

    const ChunkVisibilityReport report = BuildChunkVisibility(
        visibility_items_,
        BuildCoreVisibilityOptions(build_result, camera, visibility));
    result = ToRaylibVisibilityStats(report, visibility.mode);
    result.resident_models = 0;
    result.drawn_models = 0;
    result.culled_models = 0;
    result.total_faces = 0;
    result.drawn_faces = 0;
    result.culled_faces = 0;

    for (const RaylibUploadedChunkModel& chunk : chunks_) {
        if (!IsTerrainPassEnabled(chunk.terrain_pass, terrain_passes)) {
            continue;
        }
        ++result.resident_models;
        result.total_faces += chunk.faces;
        if (chunk.visibility_item_index >= report.entries.size()
            || report.entries[chunk.visibility_item_index].visibility_class == ChunkVisibilityClass::kHidden) {
            ++result.culled_models;
            result.culled_faces += chunk.faces;
            continue;
        }
        ++result.drawn_models;
        result.drawn_faces += chunk.faces;
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
    visibility_items_.clear();
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
    out << " resident_chunks=" << stats.resident_chunks;
    out << " resident_models=" << stats.resident_models;
    out << " visible=" << stats.visible_chunks;
    out << " fade=" << stats.fade_chunks;
    out << " hidden=" << stats.hidden_chunks;
    out << " drawn_models=" << stats.drawn_models << '/' << stats.resident_models;
    out << " faces=" << stats.drawn_faces << '/' << stats.total_faces;
    out << " draw_saved=" << std::fixed << std::setprecision(1) << stats.DrawSavedRatio() * 100.0 << '%';
    out << " face_saved=" << std::fixed << std::setprecision(1) << stats.FaceSavedRatio() * 100.0 << '%';
    return out.str();
}

}  // namespace vox3d
