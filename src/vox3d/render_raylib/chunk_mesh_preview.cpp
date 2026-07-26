#include "vox3d/render_raylib/chunk_mesh_preview.hpp"

#include "vox3d/voxel/block.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
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
        case BlockTypeId::kRuinStructure:
            return RgbaColor{126, 106, 82, 255};
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

constexpr int kGeographicMinLevel = -5;
constexpr int kGeographicMaxLevel = 20;

// Keep one explicit color per supported elevation so adjacent terrain terraces remain distinguishable.
constexpr std::array<RgbaColor, kGeographicMaxLevel - kGeographicMinLevel + 1>
    kGeographicLevelColors{{
        RgbaColor{17, 45, 101, 255},   // -5
        RgbaColor{20, 63, 132, 255},   // -4
        RgbaColor{24, 82, 158, 255},   // -3
        RgbaColor{35, 105, 180, 255},  // -2
        RgbaColor{55, 134, 193, 255},  // -1
        RgbaColor{33, 104, 55, 255},   // 0
        RgbaColor{42, 120, 59, 255},   // 1
        RgbaColor{52, 137, 63, 255},   // 2
        RgbaColor{66, 151, 67, 255},   // 3
        RgbaColor{82, 164, 71, 255},   // 4
        RgbaColor{101, 174, 75, 255},  // 5
        RgbaColor{122, 181, 77, 255},  // 6
        RgbaColor{145, 185, 78, 255},  // 7
        RgbaColor{170, 186, 79, 255},  // 8
        RgbaColor{193, 181, 75, 255},  // 9
        RgbaColor{208, 170, 67, 255},  // 10
        RgbaColor{218, 155, 58, 255},  // 11
        RgbaColor{221, 138, 50, 255},  // 12
        RgbaColor{214, 119, 45, 255},  // 13
        RgbaColor{199, 101, 43, 255},  // 14
        RgbaColor{181, 87, 46, 255},   // 15
        RgbaColor{160, 77, 51, 255},   // 16
        RgbaColor{141, 70, 56, 255},   // 17
        RgbaColor{122, 65, 61, 255},   // 18
        RgbaColor{164, 151, 136, 255}, // 19
        RgbaColor{231, 231, 220, 255}, // 20
    }};

[[nodiscard]] RgbaColor GeographicBaseColor(int level)
{
    const int clamped_level = std::clamp(level, kGeographicMinLevel, kGeographicMaxLevel);
    return kGeographicLevelColors[static_cast<std::size_t>(clamped_level - kGeographicMinLevel)];
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
    if (vertex.block_type == BlockTypeId::kRuinStructure
        && (color_mode == RaylibChunkMeshColorMode::kTraversal
            || color_mode == RaylibChunkMeshColorMode::kGeographic)) {
        return BaseColor(BlockTypeId::kRuinStructure);
    }

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
    const int structure_height = map.structure_height.IsValid()
        ? static_cast<int>(map.structure_height.cells[index])
        : 0;
    return static_cast<float>(map.height.cells[index] + 1 + structure_height);
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

[[nodiscard]] Vector3 TileCenterWorld(int tile_x, int tile_y, float level, int map_width, int map_height);
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

struct VegetationMeshBuffers {
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<unsigned char> colors;
    std::vector<unsigned short> indices;
    std::uint64_t pillars = 0;
    std::uint64_t faces = 0;
    float max_top = -std::numeric_limits<float>::infinity();
};

[[nodiscard]] bool UsesStaticVegetationMesh(const RuntimeMap& map)
{
    return map.info.vegetation_type_loaded
        && map.info.vegetation_height_loaded
        && map.vegetation_type.IsValid()
        && map.vegetation_height.IsValid();
}

[[nodiscard]] bool MatchesVegetationKind(RuntimeVegetationType type, RuntimeObjectMarkerKind kind)
{
    switch (kind) {
        case RuntimeObjectMarkerKind::kTree:
            return type == RuntimeVegetationType::kTree;
        case RuntimeObjectMarkerKind::kBush:
            return type == RuntimeVegetationType::kBush;
        case RuntimeObjectMarkerKind::kReed:
            return type == RuntimeVegetationType::kShoreReed
                || type == RuntimeVegetationType::kPuddleReed;
        case RuntimeObjectMarkerKind::kRuin:
        case RuntimeObjectMarkerKind::kCover:
        case RuntimeObjectMarkerKind::kLoot:
        case RuntimeObjectMarkerKind::kStructure:
        case RuntimeObjectMarkerKind::kTrench:
        case RuntimeObjectMarkerKind::kUnknown:
            break;
    }
    return false;
}

[[nodiscard]] Color StaticVegetationColor(RuntimeObjectMarkerKind kind)
{
    switch (kind) {
        case RuntimeObjectMarkerKind::kTree:
            return Color{10, 66, 28, 255};
        case RuntimeObjectMarkerKind::kBush:
            return Color{92, 188, 82, 245};
        case RuntimeObjectMarkerKind::kReed:
            return Color{181, 196, 72, 220};
        case RuntimeObjectMarkerKind::kRuin:
        case RuntimeObjectMarkerKind::kCover:
        case RuntimeObjectMarkerKind::kLoot:
        case RuntimeObjectMarkerKind::kStructure:
        case RuntimeObjectMarkerKind::kTrench:
        case RuntimeObjectMarkerKind::kUnknown:
            break;
    }
    return Color{255, 255, 255, 255};
}

void AppendVegetationVertex(
    VegetationMeshBuffers& mesh,
    Vector3 position,
    Vector3 normal,
    Color color)
{
    mesh.vertices.push_back(position.x);
    mesh.vertices.push_back(position.y);
    mesh.vertices.push_back(position.z);
    mesh.normals.push_back(normal.x);
    mesh.normals.push_back(normal.y);
    mesh.normals.push_back(normal.z);
    mesh.colors.push_back(color.r);
    mesh.colors.push_back(color.g);
    mesh.colors.push_back(color.b);
    mesh.colors.push_back(color.a);
}

void AppendVegetationQuad(
    VegetationMeshBuffers& mesh,
    std::array<Vector3, 4> vertices,
    Vector3 normal,
    Color color)
{
    const std::size_t first_vertex = mesh.vertices.size() / 3ULL;
    if (first_vertex + 3ULL > static_cast<std::size_t>(std::numeric_limits<unsigned short>::max())) {
        return;
    }
    for (Vector3 vertex : vertices) {
        AppendVegetationVertex(mesh, vertex, normal, color);
    }
    const auto base = static_cast<unsigned short>(first_vertex);
    mesh.indices.push_back(static_cast<unsigned short>(base + 0U));
    mesh.indices.push_back(static_cast<unsigned short>(base + 1U));
    mesh.indices.push_back(static_cast<unsigned short>(base + 2U));
    mesh.indices.push_back(static_cast<unsigned short>(base + 0U));
    mesh.indices.push_back(static_cast<unsigned short>(base + 2U));
    mesh.indices.push_back(static_cast<unsigned short>(base + 3U));
    ++mesh.faces;
}

void AppendVegetationPillar(
    VegetationMeshBuffers& mesh,
    Vector3 center,
    float width,
    float height,
    Color color)
{
    const float half_width = width * 0.5F;
    const float x0 = center.x - half_width;
    const float x1 = center.x + half_width;
    const float z0 = center.z - half_width;
    const float z1 = center.z + half_width;
    const float y0 = center.y;
    const float y1 = center.y + height;

    AppendVegetationQuad(
        mesh,
        {Vector3{x0, y1, z0}, Vector3{x0, y1, z1}, Vector3{x1, y1, z1}, Vector3{x1, y1, z0}},
        Vector3{0.0F, 1.0F, 0.0F},
        color);
    AppendVegetationQuad(
        mesh,
        {Vector3{x0, y0, z0}, Vector3{x0, y0, z1}, Vector3{x0, y1, z1}, Vector3{x0, y1, z0}},
        Vector3{-1.0F, 0.0F, 0.0F},
        color);
    AppendVegetationQuad(
        mesh,
        {Vector3{x1, y0, z1}, Vector3{x1, y0, z0}, Vector3{x1, y1, z0}, Vector3{x1, y1, z1}},
        Vector3{1.0F, 0.0F, 0.0F},
        color);
    AppendVegetationQuad(
        mesh,
        {Vector3{x0, y0, z1}, Vector3{x1, y0, z1}, Vector3{x1, y1, z1}, Vector3{x0, y1, z1}},
        Vector3{0.0F, 0.0F, 1.0F},
        color);
    AppendVegetationQuad(
        mesh,
        {Vector3{x1, y0, z0}, Vector3{x0, y0, z0}, Vector3{x0, y1, z0}, Vector3{x1, y1, z0}},
        Vector3{0.0F, 0.0F, -1.0F},
        color);
    ++mesh.pillars;
}

[[nodiscard]] VegetationMeshBuffers BuildVegetationMeshBuffers(
    const RuntimeMap& map,
    const ChunkMeshData& chunk,
    RuntimeObjectMarkerKind kind,
    bool skip_trees)
{
    VegetationMeshBuffers mesh;
    if (!UsesStaticVegetationMesh(map) || !chunk.bounds.IsValid()) {
        return mesh;
    }

    const std::size_t tile_capacity = static_cast<std::size_t>(chunk.bounds.Width())
        * static_cast<std::size_t>(chunk.bounds.Height());
    mesh.vertices.reserve(tile_capacity * 5ULL * 4ULL * 3ULL);
    mesh.normals.reserve(tile_capacity * 5ULL * 4ULL * 3ULL);
    mesh.colors.reserve(tile_capacity * 5ULL * 4ULL * 4ULL);
    mesh.indices.reserve(tile_capacity * 5ULL * 6ULL);

    const Color color = StaticVegetationColor(kind);
    if (skip_trees && kind == RuntimeObjectMarkerKind::kTree) {
        return mesh;
    }
    for (int y = chunk.bounds.min_y; y < chunk.bounds.max_y; ++y) {
        for (int x = chunk.bounds.min_x; x < chunk.bounds.max_x; ++x) {
            const auto index = static_cast<std::size_t>(y) * static_cast<std::size_t>(map.info.width)
                + static_cast<std::size_t>(x);
            if (index >= map.vegetation_type.cells.size()
                || index >= map.vegetation_height.cells.size()) {
                continue;
            }
            const auto type = static_cast<RuntimeVegetationType>(map.vegetation_type.cells[index]);
            if (!MatchesVegetationKind(type, kind)) {
                continue;
            }

            const int source_height = static_cast<int>(map.vegetation_height.cells[index]);
            if (source_height <= 0) {
                continue;
            }
            const TileCoord tile{x, y};
            Vector3 base = TileCenterWorld(
                x,
                y,
                TerrainTopLevel(map, tile),
                map.info.width,
                map.info.height);
            float width = 0.50F;
            float height = static_cast<float>(source_height);
            if (kind == RuntimeObjectMarkerKind::kReed) {
                width = 0.34F;
                height = 0.34F;
                base.y += 0.20F;
            }
            AppendVegetationPillar(mesh, base, width, height, color);
            mesh.max_top = std::max(mesh.max_top, base.y + height);
        }
    }
    return mesh;
}

constexpr std::uint64_t kGpuVertexBytes = 3ULL * sizeof(float)
    + 3ULL * sizeof(float) + 4ULL * sizeof(unsigned char);
constexpr std::uint64_t kGpuIndexBytes = sizeof(unsigned short);

[[nodiscard]] std::uint64_t GpuBufferBytes(std::uint64_t vertices, std::uint64_t indices)
{
    return vertices * kGpuVertexBytes + indices * kGpuIndexBytes;
}

[[nodiscard]] Model LoadVegetationModel(const VegetationMeshBuffers& source)
{
    if (source.vertices.empty() || source.indices.empty()
        || source.vertices.size() / 3ULL > static_cast<std::size_t>(std::numeric_limits<unsigned short>::max())) {
        return Model{};
    }

    Mesh mesh{};
    mesh.vertexCount = static_cast<int>(source.vertices.size() / 3ULL);
    mesh.triangleCount = static_cast<int>(source.indices.size() / 3ULL);
    mesh.vertices = static_cast<float*>(MemAlloc(static_cast<unsigned int>(source.vertices.size() * sizeof(float))));
    mesh.normals = static_cast<float*>(MemAlloc(static_cast<unsigned int>(source.normals.size() * sizeof(float))));
    mesh.colors = static_cast<unsigned char*>(MemAlloc(static_cast<unsigned int>(source.colors.size() * sizeof(unsigned char))));
    mesh.indices = static_cast<unsigned short*>(MemAlloc(static_cast<unsigned int>(source.indices.size() * sizeof(unsigned short))));

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

    std::copy(source.vertices.begin(), source.vertices.end(), mesh.vertices);
    std::copy(source.normals.begin(), source.normals.end(), mesh.normals);
    std::copy(source.colors.begin(), source.colors.end(), mesh.colors);
    std::copy(source.indices.begin(), source.indices.end(), mesh.indices);
    UploadMesh(&mesh, false);
    return LoadModelFromMesh(mesh);
}

void AccumulateVegetationStats(
    RuntimeObjectMarkerKind kind,
    const VegetationMeshBuffers& mesh,
    RaylibVegetationMeshStats& stats)
{
    ++stats.models;
    stats.pillars += mesh.pillars;
    stats.faces += mesh.faces;
    stats.vertices += mesh.faces * 4ULL;
    stats.indices += mesh.faces * 6ULL;
    if (kind == RuntimeObjectMarkerKind::kTree) {
        ++stats.tree_models;
        stats.tree_pillars += mesh.pillars;
    } else if (kind == RuntimeObjectMarkerKind::kBush) {
        ++stats.bush_models;
        stats.bush_pillars += mesh.pillars;
    } else if (kind == RuntimeObjectMarkerKind::kReed) {
        ++stats.reed_models;
        stats.reed_pillars += mesh.pillars;
    }
    stats.uploaded = stats.models > 0;
}

[[nodiscard]] float UploadVegetationChunkModels(
    const RuntimeMap& map,
    const ChunkMeshData& chunk,
    std::vector<RaylibUploadedVegetationModel>& models,
    RaylibVegetationMeshStats& stats,
    bool skip_trees)
{
    float max_top = -std::numeric_limits<float>::infinity();
    if (!UsesStaticVegetationMesh(map)) {
        return max_top;
    }
    for (RuntimeObjectMarkerKind kind : {
             RuntimeObjectMarkerKind::kTree,
             RuntimeObjectMarkerKind::kBush,
             RuntimeObjectMarkerKind::kReed}) {
        VegetationMeshBuffers mesh = BuildVegetationMeshBuffers(
            map,
            chunk,
            kind,
            skip_trees);
        if (mesh.pillars == 0 || mesh.faces == 0) {
            continue;
        }
        Model model = LoadVegetationModel(mesh);
        if (model.meshCount <= 0 || model.meshes == nullptr) {
            continue;
        }
        const std::uint64_t vertices = mesh.faces * 4ULL;
        const std::uint64_t indices = mesh.faces * 6ULL;
        models.push_back(RaylibUploadedVegetationModel{
            model,
            chunk.coord,
            kind,
            mesh.pillars,
            mesh.faces,
            vertices,
            indices,
            GpuBufferBytes(vertices, indices),
        });
        AccumulateVegetationStats(kind, mesh, stats);
        max_top = std::max(max_top, mesh.max_top);
    }
    return max_top;
}

[[nodiscard]] std::uint32_t TreePlacementHash(int x, int y)
{
    std::uint32_t value = static_cast<std::uint32_t>(x) * 0x9E3779B1U;
    value ^= static_cast<std::uint32_t>(y) * 0x85EBCA77U;
    value ^= value >> 16U;
    value *= 0x7FEB352DU;
    value ^= value >> 15U;
    return value;
}

[[nodiscard]] std::size_t SelectExperimentalTreeModel(int x, int y, std::uint32_t hash)
{
    // Model indices match ConfigureExperimentalTrees(). Common models occupy
    // [0, 11], while [12, 13] are intentionally rare special silhouettes.
    constexpr std::array<std::size_t, 6> kBroadleafModels{0U, 1U, 2U, 3U, 4U, 5U};
    constexpr std::array<std::size_t, 6> kConiferModels{6U, 7U, 8U, 9U, 10U, 11U};
    constexpr std::size_t kAncientBroadleaf = 12U;
    constexpr std::size_t kDeadConifer = 13U;
    constexpr int kForestPatchSize = 12;

    const std::uint32_t patch_hash = TreePlacementHash(
        x / kForestPatchSize,
        y / kForestPatchSize);
    const std::uint32_t profile = patch_hash % 3U;
    const std::uint32_t roll = (hash >> 3U) % 100U;
    const std::uint32_t variant_roll = (hash >> 11U) % 100U;

    // Keep special trees genuinely uncommon: about two instances per hundred.
    if (roll == 0U) {
        return profile == 1U ? kDeadConifer : kAncientBroadleaf;
    }
    if (roll == 1U) {
        return profile == 0U ? kAncientBroadleaf : kDeadConifer;
    }

    const auto select_weighted = [variant_roll](
                                     const std::array<std::size_t, 6>& models) {
        // Base and detailed silhouettes are common; young, crooked, sparse,
        // tall, and wide variants appear often enough to break repetition.
        constexpr std::array<std::uint32_t, 6> kUpperBounds{26U, 46U, 61U, 76U, 89U, 100U};
        for (std::size_t index = 0; index < kUpperBounds.size(); ++index) {
            if (variant_roll < kUpperBounds[index]) {
                return models[index];
            }
        }
        return models.back();
    };

    switch (profile) {
    case 0U:  // Broadleaf forest.
        return select_weighted(kBroadleafModels);
    case 1U:  // Conifer forest.
        return select_weighted(kConiferModels);
    default:  // Mixed forest.
        return roll < 51U
            ? select_weighted(kBroadleafModels)
            : select_weighted(kConiferModels);
    }
}

void AppendExperimentalTreeInstances(
    const RuntimeMap& map,
    const ChunkMeshData& chunk,
    int tree_limit,
    std::vector<RaylibExperimentalTreeInstance>& instances)
{
    const bool has_limit = tree_limit > 0;
    if (!UsesStaticVegetationMesh(map) || !chunk.bounds.IsValid()
        || (has_limit && instances.size() >= static_cast<std::size_t>(tree_limit))) {
        return;
    }

    for (int y = chunk.bounds.min_y; y < chunk.bounds.max_y; ++y) {
        for (int x = chunk.bounds.min_x; x < chunk.bounds.max_x; ++x) {
            if (has_limit && instances.size() >= static_cast<std::size_t>(tree_limit)) {
                return;
            }
            const auto index = static_cast<std::size_t>(y)
                * static_cast<std::size_t>(map.info.width)
                + static_cast<std::size_t>(x);
            if (index >= map.vegetation_type.cells.size()
                || static_cast<RuntimeVegetationType>(map.vegetation_type.cells[index])
                    != RuntimeVegetationType::kTree) {
                continue;
            }

            const std::uint32_t hash = TreePlacementHash(x, y);
            const float offset_x = (static_cast<float>((hash >> 8U) & 0xFFU) / 255.0F - 0.5F) * 0.34F;
            const float offset_z = (static_cast<float>((hash >> 16U) & 0xFFU) / 255.0F - 0.5F) * 0.34F;
            const float scale = 1.45F + static_cast<float>((hash >> 24U) & 0xFFU) / 255.0F * 0.45F;
            Vector3 position = TileCenterWorld(
                x,
                y,
                TerrainTopLevel(map, TileCoord{x, y}),
                map.info.width,
                map.info.height);
            position.x += offset_x;
            position.z += offset_z;
            instances.push_back(RaylibExperimentalTreeInstance{
                chunk.coord,
                position,
                static_cast<float>(hash % 360U),
                scale,
                SelectExperimentalTreeModel(x, y, hash),
            });
        }
    }
}

[[nodiscard]] std::vector<ChunkVisibilityClass> BuildChunkVisibilityClassMap(
    const ChunkMeshBuildInfo& info,
    const ChunkVisibilityReport& report);

void DrawExperimentalTreeInstances(
    const std::vector<Model>& models,
    const std::vector<RaylibExperimentalTreeInstance>& instances,
    const ChunkMeshBuildInfo& info,
    const ChunkVisibilityReport& visibility_report,
    RaylibChunkMeshDebugOverlayOptions overlays,
    RaylibVegetationMeshStats& stats)
{
    stats.last_experimental_tree_draw_calls = 0;
    if (!overlays.show_object_trees || models.empty() || instances.empty()) {
        return;
    }

    const std::vector<ChunkVisibilityClass> classes = BuildChunkVisibilityClassMap(
        info,
        visibility_report);
    const int chunks_x = std::max(1, info.chunks_x);
    constexpr Vector3 kAxis{0.0F, 1.0F, 0.0F};
    for (const RaylibExperimentalTreeInstance& instance : instances) {
        if (instance.coord.x < 0 || instance.coord.y < 0
            || instance.coord.x >= info.chunks_x || instance.coord.y >= info.chunks_y) {
            continue;
        }
        const auto chunk_index = static_cast<std::size_t>(instance.coord.y)
            * static_cast<std::size_t>(chunks_x)
            + static_cast<std::size_t>(instance.coord.x);
        if (chunk_index >= classes.size()
            || classes[chunk_index] == ChunkVisibilityClass::kHidden
            || instance.model_index >= models.size()) {
            continue;
        }
        const Vector3 scale{instance.scale, instance.scale, instance.scale};
        DrawModelEx(
            models[instance.model_index],
            instance.position,
            kAxis,
            instance.rotation_degrees,
            scale,
            VisibilityTint(classes[chunk_index]));
        ++stats.last_experimental_tree_draw_calls;
    }
}

[[nodiscard]] std::vector<ChunkVisibilityClass> BuildChunkVisibilityClassMap(
    const ChunkMeshBuildInfo& info,
    const ChunkVisibilityReport& report)
{
    const int chunks_x = std::max(1, info.chunks_x);
    const int chunks_y = std::max(1, info.chunks_y);
    std::vector<ChunkVisibilityClass> classes(
        static_cast<std::size_t>(chunks_x) * static_cast<std::size_t>(chunks_y),
        ChunkVisibilityClass::kHidden);
    for (const ChunkVisibilityEntry& entry : report.entries) {
        if (entry.coord.x < 0 || entry.coord.y < 0 || entry.coord.x >= chunks_x || entry.coord.y >= chunks_y) {
            continue;
        }
        const auto index = static_cast<std::size_t>(entry.coord.y) * static_cast<std::size_t>(chunks_x)
            + static_cast<std::size_t>(entry.coord.x);
        classes[index] = entry.visibility_class;
    }
    return classes;
}

void DrawVegetationChunkModels(
    const std::vector<RaylibUploadedVegetationModel>& models,
    const ChunkMeshBuildInfo& info,
    const ChunkVisibilityReport& visibility_report,
    RaylibChunkMeshDebugOverlayOptions overlays,
    RaylibVegetationMeshStats& stats)
{
    stats.last_visible_chunks = 0;
    stats.last_draw_calls = 0;
    stats.last_drawn_pillars = 0;
    if (models.empty()
        || (!overlays.show_object_trees && !overlays.show_object_bushes && !overlays.show_object_reeds)) {
        return;
    }

    const std::vector<ChunkVisibilityClass> classes = BuildChunkVisibilityClassMap(info, visibility_report);
    std::vector<std::uint8_t> drawn_chunks(classes.size(), 0U);
    const int chunks_x = std::max(1, info.chunks_x);
    constexpr Vector3 kOrigin{0.0F, 0.0F, 0.0F};
    constexpr float kScale = 1.0F;
    for (const RaylibUploadedVegetationModel& vegetation : models) {
        const bool enabled = (vegetation.kind == RuntimeObjectMarkerKind::kTree && overlays.show_object_trees)
            || (vegetation.kind == RuntimeObjectMarkerKind::kBush && overlays.show_object_bushes)
            || (vegetation.kind == RuntimeObjectMarkerKind::kReed && overlays.show_object_reeds);
        if (!enabled || vegetation.coord.x < 0 || vegetation.coord.y < 0
            || vegetation.coord.x >= info.chunks_x || vegetation.coord.y >= info.chunks_y) {
            continue;
        }
        const auto index = static_cast<std::size_t>(vegetation.coord.y) * static_cast<std::size_t>(chunks_x)
            + static_cast<std::size_t>(vegetation.coord.x);
        if (index >= classes.size() || classes[index] == ChunkVisibilityClass::kHidden) {
            continue;
        }
        DrawModel(vegetation.model, kOrigin, kScale, VisibilityTint(classes[index]));
        if (drawn_chunks[index] == 0U) {
            drawn_chunks[index] = 1U;
            ++stats.last_visible_chunks;
        }
        ++stats.last_draw_calls;
        stats.last_drawn_pillars += vegetation.pillars;
    }
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

[[nodiscard]] std::uint64_t RuinFaceCount(const ChunkMeshData& mesh)
{
    return static_cast<std::uint64_t>(std::count_if(
        mesh.faces.begin(),
        mesh.faces.end(),
        [](const MeshFace& face) { return face.block_type == BlockTypeId::kRuinStructure; }));
}

void AccumulateUploadStats(const ChunkMeshData& mesh, RaylibChunkMeshPreviewStats& stats)
{
    const std::uint64_t faces = static_cast<std::uint64_t>(mesh.faces.size());
    const std::uint64_t ruin_faces = RuinFaceCount(mesh);
    ++stats.models;
    stats.faces += faces;
    stats.ruin_faces += ruin_faces;
    stats.terrain_faces += faces - ruin_faces;
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
            const float level = TerrainTopLevel(map, TileCoord{x, y}) + 0.08F;
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

[[nodiscard]] bool UsesVegetationPillar(const RuntimeObjectMarker& marker)
{
    return marker.visual_only
        && marker.role == "vegetation"
        && (marker.kind == RuntimeObjectMarkerKind::kTree
            || marker.kind == RuntimeObjectMarkerKind::kBush);
}

[[nodiscard]] Color ObjectMarkerColor(const RuntimeObjectMarker& marker)
{
    if (UsesVegetationPillar(marker)) {
        if (marker.kind == RuntimeObjectMarkerKind::kTree) {
            return Color{10, 66, 28, 255};
        }
        return Color{92, 188, 82, 245};
    }

    switch (marker.kind) {
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

[[nodiscard]] Vector3 ObjectMarkerSize(const RuntimeObjectMarker& marker)
{
    if (UsesVegetationPillar(marker)) {
        constexpr float kVegetationPillarWidth = 0.50F;
        const float height = static_cast<float>(std::max(1, marker.height));
        return Vector3{kVegetationPillarWidth, height, kVegetationPillarWidth};
    }

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

void DrawObjectMarker(
    const RuntimeObjectMarker& marker,
    const RuntimeMap& map,
    const ChunkMeshBuildResult& build_result,
    const std::vector<std::uint8_t>& visible_chunks,
    RaylibChunkMeshDebugOverlayOptions overlays)
{
    if (!IsObjectMarkerKindEnabled(marker.kind, overlays)
        || !map.height.Contains(marker.tile)
        || !IsMarkerChunkVisible(marker, build_result.info, visible_chunks)) {
        return;
    }

    const Color color = ObjectMarkerColor(marker);
    const Vector3 size = ObjectMarkerSize(marker);
    const float terrain_level = TerrainTopLevel(map, marker.tile);
    const float base_offset = UsesVegetationPillar(marker) ? 0.0F : 0.20F;
    const Vector3 base = TileCenterWorld(
        marker.tile.x,
        marker.tile.y,
        terrain_level + base_offset,
        build_result.info.map_width,
        build_result.info.map_height);
    const Vector3 center{base.x, base.y + size.y * 0.5F, base.z};
    DrawCubeV(center, size, color);
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
    const std::size_t vegetation_count = std::min(
        map.object_markers.size(),
        static_cast<std::size_t>(std::max(0, map.info.vegetation_markers)));
    const std::size_t vegetation_start = map.object_markers.size() - vegetation_count;

    for (std::size_t index = 0; index < vegetation_start; ++index) {
        DrawObjectMarker(map.object_markers[index], map, build_result, visible_chunks, overlays);
    }

    if (UsesStaticVegetationMesh(map)) {
        return;
    }
    for (std::size_t index = vegetation_start; index < map.object_markers.size(); ++index) {
        DrawObjectMarker(map.object_markers[index], map, build_result, visible_chunks, overlays);
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

bool RaylibVegetationMeshStats::IsValid() const
{
    return uploaded && models > 0 && pillars > 0
        && models == tree_models + bush_models + reed_models
        && pillars == tree_pillars + bush_pillars + reed_pillars
        && faces == pillars * 5ULL
        && vertices == faces * 4ULL && indices == faces * 6ULL;
}

RaylibChunkMeshPreview::~RaylibChunkMeshPreview()
{
    Unload();
    UnloadExperimentalTreeAssets();
}

bool RaylibChunkMeshPreview::ConfigureExperimentalTrees(
    const RaylibExperimentalTreeOptions& options)
{
    UnloadExperimentalTreeAssets();
    experimental_tree_options_ = options;
    if (!options.enabled) {
        return true;
    }

    constexpr std::array<std::string_view, 14> kModelNames{
        "forest-deciduous-clustered.glb",
        "forest-deciduous-single-crown.glb",
        "forest-deciduous-tall.glb",
        "forest-deciduous-wide.glb",
        "forest-deciduous-young.glb",
        "forest-deciduous-crooked.glb",
        "forest-conifer-simple.glb",
        "forest-conifer-detailed.glb",
        "forest-conifer-young.glb",
        "forest-conifer-tall.glb",
        "forest-conifer-wide.glb",
        "forest-conifer-sparse.glb",
        "forest-rare-ancient-deciduous.glb",
        "forest-rare-dead-conifer.glb",
    };
    experimental_tree_models_.reserve(kModelNames.size());
    for (const std::string_view name : kModelNames) {
        const std::filesystem::path path = options.asset_directory / name;
        const std::string path_text = path.string();
        Model model = LoadModel(path_text.c_str());
        if (model.meshCount <= 0 || model.meshes == nullptr) {
            TraceLog(LOG_ERROR, "VOX3D: failed to load experimental tree model: %s",
                path_text.c_str());
            if (model.meshCount > 0 || model.meshes != nullptr) {
                UnloadModel(model);
            }
            UnloadExperimentalTreeAssets();
            experimental_tree_options_.enabled = false;
            return false;
        }
        TraceLog(LOG_INFO, "VOX3D: loaded experimental tree model: %s meshes=%d",
            path_text.c_str(), model.meshCount);
        experimental_tree_models_.push_back(model);
    }
    vegetation_stats_.experimental_tree_assets = experimental_tree_models_.size();
    return true;
}

bool RaylibChunkMeshPreview::Upload(
    const ChunkMeshBuildResult& build_result,
    RaylibChunkMeshColorMode color_mode,
    const RuntimeMap* runtime_map)
{
    Unload();
    return UploadAdditional(build_result, color_mode, runtime_map);
}

bool RaylibChunkMeshPreview::UploadAdditional(
    const ChunkMeshBuildResult& build_result,
    RaylibChunkMeshColorMode color_mode,
    const RuntimeMap* runtime_map)
{
    if (!build_result.IsValid()) {
        return false;
    }

    const auto upload_started = std::chrono::steady_clock::now();
    const std::uint64_t bytes_before = gpu_resource_stats_.current_bytes;
    const std::uint64_t models_before = stats_.models + vegetation_stats_.models;

    const bool split_terrain_passes = build_result.info.mode == ChunkMeshBuildMode::kTerrainSurface;
    chunks_.reserve(chunks_.size() + build_result.chunks.size() * (split_terrain_passes ? 3ULL : 1ULL));
    visibility_items_.reserve(visibility_items_.size() + build_result.chunks.size());
    if (runtime_map != nullptr && UsesStaticVegetationMesh(*runtime_map)) {
        vegetation_models_.reserve(vegetation_models_.size() + build_result.chunks.size() * 3ULL);
    }
    const std::uint64_t before_models = stats_.models;
    for (const ChunkMeshData& chunk : build_result.chunks) {
        if (chunk.faces.empty()) {
            continue;
        }

        Aabb3f world_bounds = ComputeWorldBounds(chunk, build_result.info.map_width, build_result.info.map_height);
        if (runtime_map != nullptr) {
            const float vegetation_top = UploadVegetationChunkModels(
                *runtime_map,
                chunk,
                vegetation_models_,
                vegetation_stats_,
                experimental_tree_options_.enabled);
            if (experimental_tree_options_.enabled) {
                AppendExperimentalTreeInstances(
                    *runtime_map,
                    chunk,
                    experimental_tree_options_.tree_limit,
                    experimental_tree_instances_);
                vegetation_stats_.experimental_tree_instances =
                    experimental_tree_instances_.size();
            }
            if (std::isfinite(vegetation_top)) {
                world_bounds.max.y = std::max(world_bounds.max.y, vegetation_top);
            }
        }
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

                const std::uint64_t faces = pass_mesh.FaceCount();
                const std::uint64_t ruin_faces = RuinFaceCount(pass_mesh);
                const std::uint64_t vertices = static_cast<std::uint64_t>(pass_mesh.vertices.size());
                const std::uint64_t indices = static_cast<std::uint64_t>(pass_mesh.indices.size());
                chunks_.push_back(RaylibUploadedChunkModel{
                    model,
                    chunk.coord,
                    chunk.bounds,
                    world_bounds,
                    pass,
                    visibility_item_index,
                    faces,
                    faces - ruin_faces,
                    ruin_faces,
                    vertices,
                    indices,
                    GpuBufferBytes(vertices, indices),
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

        const std::uint64_t faces = chunk.FaceCount();
        const std::uint64_t ruin_faces = RuinFaceCount(chunk);
        const std::uint64_t vertices = static_cast<std::uint64_t>(chunk.vertices.size());
        const std::uint64_t indices = static_cast<std::uint64_t>(chunk.indices.size());
        chunks_.push_back(RaylibUploadedChunkModel{
            model,
            chunk.coord,
            chunk.bounds,
            world_bounds,
            TerrainRenderPass::kBody,
            visibility_item_index,
            faces,
            faces - ruin_faces,
            ruin_faces,
            vertices,
            indices,
            GpuBufferBytes(vertices, indices),
        });
        AccumulateUploadStats(chunk, stats_);
    }

    stats_.uploaded = !chunks_.empty();
    RebuildGpuResourceStats();
    const std::uint64_t uploaded_bytes = gpu_resource_stats_.current_bytes >= bytes_before
        ? gpu_resource_stats_.current_bytes - bytes_before
        : 0ULL;
    const std::uint64_t uploaded_models = stats_.models + vegetation_stats_.models - models_before;
    const double upload_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - upload_started).count();
    gpu_streaming_stats_.uploaded_bytes_this_frame += uploaded_bytes;
    gpu_streaming_stats_.upload_time_this_frame_ms += upload_ms;
    gpu_streaming_stats_.uploaded_models_this_frame += uploaded_models;
    gpu_streaming_stats_.total_uploaded_bytes += uploaded_bytes;
    gpu_streaming_stats_.total_uploaded_models += uploaded_models;
    return stats_.models > before_models;
}


std::size_t RaylibChunkMeshPreview::UnloadChunks(const std::vector<ChunkCoord>& coords)
{
    if (coords.empty() || (chunks_.empty() && vegetation_models_.empty())) {
        return 0;
    }

    auto should_remove = [&](ChunkCoord coord) {
        return std::any_of(coords.begin(), coords.end(), [&](ChunkCoord removed) {
            return removed.x == coord.x && removed.y == coord.y;
        });
    };

    std::vector<RaylibUploadedChunkModel> kept;
    kept.reserve(chunks_.size());
    std::vector<ChunkCoord> removed_unique;
    std::uint64_t removed_models = 0;
    for (RaylibUploadedChunkModel& chunk : chunks_) {
        if (!should_remove(chunk.coord)) {
            kept.push_back(std::move(chunk));
            continue;
        }
        if (chunk.model.meshCount > 0 && chunk.model.meshes != nullptr) {
            UnloadModel(chunk.model);
        }
        ++removed_models;
        const bool known = std::any_of(removed_unique.begin(), removed_unique.end(), [&](ChunkCoord removed) {
            return removed.x == chunk.coord.x && removed.y == chunk.coord.y;
        });
        if (!known) {
            removed_unique.push_back(chunk.coord);
        }
    }

    chunks_ = std::move(kept);

    std::vector<RaylibUploadedVegetationModel> kept_vegetation;
    kept_vegetation.reserve(vegetation_models_.size());
    for (RaylibUploadedVegetationModel& vegetation : vegetation_models_) {
        if (!should_remove(vegetation.coord)) {
            kept_vegetation.push_back(std::move(vegetation));
            continue;
        }
        if (vegetation.model.meshCount > 0 && vegetation.model.meshes != nullptr) {
            UnloadModel(vegetation.model);
        }
        ++removed_models;
    }
    vegetation_models_ = std::move(kept_vegetation);
    std::erase_if(
        experimental_tree_instances_,
        [&](const RaylibExperimentalTreeInstance& instance) {
            return should_remove(instance.coord);
        });
    vegetation_stats_ = RaylibVegetationMeshStats{};
    vegetation_stats_.experimental_tree_assets = experimental_tree_models_.size();
    vegetation_stats_.experimental_tree_instances = experimental_tree_instances_.size();
    for (const RaylibUploadedVegetationModel& vegetation : vegetation_models_) {
        ++vegetation_stats_.models;
        vegetation_stats_.pillars += vegetation.pillars;
        vegetation_stats_.faces += vegetation.faces;
        vegetation_stats_.vertices += vegetation.vertices;
        vegetation_stats_.indices += vegetation.indices;
        if (vegetation.kind == RuntimeObjectMarkerKind::kTree) {
            ++vegetation_stats_.tree_models;
            vegetation_stats_.tree_pillars += vegetation.pillars;
        } else if (vegetation.kind == RuntimeObjectMarkerKind::kBush) {
            ++vegetation_stats_.bush_models;
            vegetation_stats_.bush_pillars += vegetation.pillars;
        } else if (vegetation.kind == RuntimeObjectMarkerKind::kReed) {
            ++vegetation_stats_.reed_models;
            vegetation_stats_.reed_pillars += vegetation.pillars;
        }
    }
    vegetation_stats_.uploaded = vegetation_stats_.models > 0;

    visibility_items_.clear();
    stats_ = RaylibChunkMeshPreviewStats{};
    for (RaylibUploadedChunkModel& chunk : chunks_) {
        auto existing = std::find_if(
            visibility_items_.begin(),
            visibility_items_.end(),
            [&](const ChunkVisibilityItem& item) {
                return item.coord.x == chunk.coord.x && item.coord.y == chunk.coord.y;
            });
        if (existing == visibility_items_.end()) {
            chunk.visibility_item_index = visibility_items_.size();
            visibility_items_.push_back(ChunkVisibilityItem{chunk.coord, chunk.world_bounds, chunk.faces});
        } else {
            chunk.visibility_item_index = static_cast<std::size_t>(std::distance(visibility_items_.begin(), existing));
        }
        ++stats_.models;
        stats_.faces += chunk.faces;
        stats_.terrain_faces += chunk.terrain_faces;
        stats_.ruin_faces += chunk.ruin_faces;
        stats_.vertices += chunk.vertices;
        stats_.indices += chunk.indices;
    }
    stats_.uploaded = !chunks_.empty();
    gpu_streaming_stats_.unloaded_models_this_frame += removed_models;
    gpu_streaming_stats_.total_unloaded_models += removed_models;
    RebuildGpuResourceStats();
    return removed_unique.size();
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
    render_frame_stats_ = RaylibRenderFrameStats{};
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
            ++render_frame_stats_.models_skipped;
            continue;
        }
        const ChunkVisibilityClass visibility_class = visibility_report.entries[chunk.visibility_item_index].visibility_class;
        if (visibility_class == ChunkVisibilityClass::kHidden) {
            ++render_frame_stats_.models_skipped;
            continue;
        }
        DrawModel(chunk.model, kOrigin, kScale, VisibilityTint(visibility_class));
        ++render_frame_stats_.model_draw_calls;
        ++render_frame_stats_.models_drawn;
        render_frame_stats_.vertices_submitted += chunk.vertices;
        render_frame_stats_.triangles_submitted += chunk.indices / 3ULL;
    }
    DrawVegetationChunkModels(
        vegetation_models_,
        build_result.info,
        visibility_report,
        overlays,
        vegetation_stats_);
    DrawExperimentalTreeInstances(
        experimental_tree_models_,
        experimental_tree_instances_,
        build_result.info,
        visibility_report,
        overlays,
        vegetation_stats_);
    render_frame_stats_.model_draw_calls += vegetation_stats_.last_draw_calls;
    render_frame_stats_.models_drawn += vegetation_stats_.last_draw_calls;
    render_frame_stats_.models_skipped += vegetation_models_.size() >= vegetation_stats_.last_draw_calls
        ? static_cast<std::uint64_t>(vegetation_models_.size()) - vegetation_stats_.last_draw_calls
        : 0ULL;
    render_frame_stats_.vertices_submitted += vegetation_stats_.last_drawn_pillars * 20ULL;
    render_frame_stats_.triangles_submitted += vegetation_stats_.last_drawn_pillars * 10ULL;
    render_frame_stats_.model_draw_calls += vegetation_stats_.last_experimental_tree_draw_calls;
    render_frame_stats_.models_drawn += vegetation_stats_.last_experimental_tree_draw_calls;

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
    const std::uint64_t unloaded_models = static_cast<std::uint64_t>(chunks_.size() + vegetation_models_.size());
    for (RaylibUploadedChunkModel& chunk : chunks_) {
        if (chunk.model.meshCount > 0 && chunk.model.meshes != nullptr) {
            UnloadModel(chunk.model);
        }
    }
    chunks_.clear();
    for (RaylibUploadedVegetationModel& vegetation : vegetation_models_) {
        if (vegetation.model.meshCount > 0 && vegetation.model.meshes != nullptr) {
            UnloadModel(vegetation.model);
        }
    }
    vegetation_models_.clear();
    experimental_tree_instances_.clear();
    visibility_items_.clear();
    stats_ = RaylibChunkMeshPreviewStats{};
    vegetation_stats_ = RaylibVegetationMeshStats{};
    vegetation_stats_.experimental_tree_assets = experimental_tree_models_.size();
    render_frame_stats_ = RaylibRenderFrameStats{};
    gpu_streaming_stats_.unloaded_models_this_frame += unloaded_models;
    gpu_streaming_stats_.total_unloaded_models += unloaded_models;
    RebuildGpuResourceStats();
}

void RaylibChunkMeshPreview::UnloadExperimentalTreeAssets()
{
    for (Model& model : experimental_tree_models_) {
        if (model.meshCount > 0 && model.meshes != nullptr) {
            UnloadModel(model);
        }
    }
    experimental_tree_models_.clear();
    experimental_tree_instances_.clear();
    vegetation_stats_.experimental_tree_assets = 0;
    vegetation_stats_.experimental_tree_instances = 0;
    vegetation_stats_.last_experimental_tree_draw_calls = 0;
}

bool RaylibChunkMeshPreview::IsUploaded() const
{
    return stats_.uploaded && !chunks_.empty();
}

const RaylibChunkMeshPreviewStats& RaylibChunkMeshPreview::Stats() const
{
    return stats_;
}

const RaylibVegetationMeshStats& RaylibChunkMeshPreview::VegetationStats() const
{
    return vegetation_stats_;
}

void RaylibChunkMeshPreview::BeginFrameDiagnostics()
{
    gpu_streaming_stats_.uploaded_bytes_this_frame = 0;
    gpu_streaming_stats_.upload_time_this_frame_ms = 0.0;
    gpu_streaming_stats_.uploaded_models_this_frame = 0;
    gpu_streaming_stats_.unloaded_models_this_frame = 0;
}

const RaylibGpuResourceStats& RaylibChunkMeshPreview::GpuResourceStats() const
{
    return gpu_resource_stats_;
}

const RaylibGpuStreamingStats& RaylibChunkMeshPreview::GpuStreamingStats() const
{
    return gpu_streaming_stats_;
}

const RaylibRenderFrameStats& RaylibChunkMeshPreview::RenderFrameStats() const
{
    return render_frame_stats_;
}

void RaylibChunkMeshPreview::RebuildGpuResourceStats()
{
    RaylibGpuResourceStats rebuilt;
    for (const RaylibUploadedChunkModel& chunk : chunks_) {
        rebuilt.vertex_buffer_bytes += chunk.vertices * kGpuVertexBytes;
        rebuilt.index_buffer_bytes += chunk.indices * kGpuIndexBytes;
        rebuilt.terrain_bytes += GpuBufferBytes(chunk.terrain_faces * 4ULL, chunk.terrain_faces * 6ULL);
        rebuilt.ruin_bytes += GpuBufferBytes(chunk.ruin_faces * 4ULL, chunk.ruin_faces * 6ULL);
        ++rebuilt.mesh_count;
    }
    for (const RaylibUploadedVegetationModel& vegetation : vegetation_models_) {
        rebuilt.vertex_buffer_bytes += vegetation.vertices * kGpuVertexBytes;
        rebuilt.index_buffer_bytes += vegetation.indices * kGpuIndexBytes;
        if (vegetation.kind == RuntimeObjectMarkerKind::kTree) {
            rebuilt.tree_bytes += vegetation.gpu_bytes;
        } else if (vegetation.kind == RuntimeObjectMarkerKind::kBush) {
            rebuilt.bush_bytes += vegetation.gpu_bytes;
        } else if (vegetation.kind == RuntimeObjectMarkerKind::kReed) {
            rebuilt.reed_bytes += vegetation.gpu_bytes;
        }
        ++rebuilt.mesh_count;
    }
    rebuilt.current_bytes = rebuilt.vertex_buffer_bytes + rebuilt.index_buffer_bytes;
    rebuilt.peak_bytes = std::max(gpu_resource_stats_.peak_bytes, rebuilt.current_bytes);
    gpu_resource_stats_ = rebuilt;
}

std::string ToLogString(const RaylibChunkMeshPreviewStats& stats)
{
    std::ostringstream out;
    out << "status=" << (stats.IsValid() ? "loaded" : "unavailable");
    out << " models=" << stats.models;
    out << " faces=" << stats.faces;
    out << " terrain_faces=" << stats.terrain_faces;
    out << " ruin_faces=" << stats.ruin_faces;
    out << " vertices=" << stats.vertices;
    out << " indices=" << stats.indices;
    if (stats.skipped_chunks > 0) {
        out << " skipped_chunks=" << stats.skipped_chunks;
    }
    return out.str();
}

std::string ToLogString(const RaylibVegetationMeshStats& stats)
{
    std::ostringstream out;
    out << "status=" << (stats.IsValid() ? "loaded" : "unavailable");
    out << " models=" << stats.models;
    out << " tree_models=" << stats.tree_models;
    out << " bush_models=" << stats.bush_models;
    out << " reed_models=" << stats.reed_models;
    out << " pillars=" << stats.pillars;
    out << " trees=" << stats.tree_pillars;
    out << " bushes=" << stats.bush_pillars;
    out << " reeds=" << stats.reed_pillars;
    out << " faces=" << stats.faces;
    out << " vertices=" << stats.vertices;
    out << " indices=" << stats.indices;
    out << " visible_chunks=" << stats.last_visible_chunks;
    out << " draw_calls=" << stats.last_draw_calls;
    out << " drawn_pillars=" << stats.last_drawn_pillars;
    out << " glb_assets=" << stats.experimental_tree_assets;
    out << " glb_instances=" << stats.experimental_tree_instances;
    out << " glb_draw_calls=" << stats.last_experimental_tree_draw_calls;
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
