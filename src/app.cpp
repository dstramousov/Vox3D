#include "app.hpp"

#include "ui_draw.hpp"
#include "vox3d/chunk/chunk_grid.hpp"
#include "vox3d/map/map_package.hpp"
#include "vox3d/map/runtime_map.hpp"
#include "vox3d/mesh/chunk_mesh_builder.hpp"
#include "vox3d/mesh/chunk_mesh_cache.hpp"
#include "vox3d/mesh/face_visibility.hpp"
#include "vox3d/mesh/terrain_mesh_builder.hpp"
#include "vox3d/path/path_probe.hpp"
#include "vox3d/transition/transition_feature.hpp"
#include "vox3d/validation/passability_validator.hpp"
#include "vox3d/voxel/voxel_world.hpp"

#include <raylib.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace vox3d {
namespace {

constexpr int kChunkSize16 = 16;
constexpr int kChunkSize32 = 32;

[[nodiscard]] std::vector<int> BuildFontCodepoints()
{
    std::vector<int> codepoints;
    codepoints.reserve(256);
    for (int c = 32; c <= 126; ++c) {
        codepoints.push_back(c);
    }
    for (int c = 0x0400; c <= 0x04FF; ++c) {
        codepoints.push_back(c);
    }
    codepoints.push_back(0x2116);
    return codepoints;
}

[[nodiscard]] std::string BuildMode()
{
#ifdef NDEBUG
    return "release";
#else
    return "debug";
#endif
}

[[nodiscard]] std::string CurrentWorkingDirectory()
{
    std::error_code error;
    const auto path = std::filesystem::current_path(error);
    if (error) {
        return "<unavailable>";
    }
    return path.string();
}

[[nodiscard]] bool IsKeyPressedAny(std::initializer_list<int> keys)
{
    return std::any_of(keys.begin(), keys.end(), [](int key) { return IsKeyPressed(key); });
}

[[nodiscard]] bool PointInRect(Vector2 point, Rectangle rect)
{
    return CheckCollisionPointRec(point, rect);
}

[[nodiscard]] std::string Lowercase(std::string_view value)
{
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

using SteadyTimePoint = std::chrono::steady_clock::time_point;

[[nodiscard]] SteadyTimePoint Now()
{
    return std::chrono::steady_clock::now();
}

[[nodiscard]] long long ElapsedMs(SteadyTimePoint start, SteadyTimePoint finish)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();
}

void ToggleOverlayFlag(
    bool& flag,
    std::string_view name,
    std::uint64_t primitive_count,
    Logger& logger,
    bool& layout_dirty)
{
    flag = !flag;
    layout_dirty = true;

    std::ostringstream out;
    out << name << '=' << (flag ? "on" : "off");
    out << " debug_primitives=" << primitive_count;
    out << " delta=" << (flag ? "+" : "-") << primitive_count;
    logger.Info("render3d", out.str());
}


[[nodiscard]] std::optional<int> ReadIntegerEnvironment(std::string_view name)
{
    const std::string key{name};
    const char* raw_value = std::getenv(key.c_str());
    if (raw_value == nullptr || raw_value[0] == '\0') {
        return std::nullopt;
    }

    try {
        std::size_t consumed = 0;
        const int value = std::stoi(std::string(raw_value), &consumed, 10);
        if (consumed == std::string_view(raw_value).size()) {
            return value;
        }
    } catch (...) {
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> ReadStringEnvironment(std::string_view name)
{
    const std::string key{name};
    const char* raw_value = std::getenv(key.c_str());
    if (raw_value == nullptr || raw_value[0] == '\0') {
        return std::nullopt;
    }
    return std::string(raw_value);
}

[[nodiscard]] std::string NormalizeStartupCorner(std::string_view value)
{
    const std::string corner = Lowercase(value);
    if (corner == "nw" || corner == "ne" || corner == "sw" || corner == "se") {
        return corner;
    }
    return "se";
}

struct InitialTileWindow {
    bool limited = false;
    int left = 0;
    int top = 0;
    int width = 0;
    int height = 0;
};

[[nodiscard]] int ResolveInitialTileWindowSize(const RuntimeMap& map)
{
    const std::optional<int> env_size = ReadIntegerEnvironment("VOX3D_INITIAL_TILE_WINDOW");
    if (env_size.has_value()) {
        return std::max(0, *env_size);
    }

    constexpr int kDefaultInitialTileWindow = 256;
    if (map.info.width > kDefaultInitialTileWindow || map.info.height > kDefaultInitialTileWindow) {
        return kDefaultInitialTileWindow;
    }
    return 0;
}

[[nodiscard]] InitialTileWindow ResolveInitialTileWindow(const RuntimeMap& map, int requested_size)
{
    InitialTileWindow window;
    window.width = std::max(0, map.info.width);
    window.height = std::max(0, map.info.height);

    if (requested_size <= 0 || map.info.width <= 0 || map.info.height <= 0) {
        return window;
    }

    const int window_width = std::min(requested_size, map.info.width);
    const int window_height = std::min(requested_size, map.info.height);
    if (window_width >= map.info.width && window_height >= map.info.height) {
        return window;
    }

    const int center_x = map.info.width / 2;
    const int center_y = map.info.height / 2;
    window.limited = true;
    window.width = window_width;
    window.height = window_height;
    window.left = std::clamp(center_x - window_width / 2, 0, map.info.width - window_width);
    window.top = std::clamp(center_y - window_height / 2, 0, map.info.height - window_height);
    return window;
}

[[nodiscard]] bool ChunkIntersectsTileWindow(const ChunkInfo& chunk, const InitialTileWindow& window)
{
    const int chunk_left = chunk.bounds.min_x;
    const int chunk_top = chunk.bounds.min_y;
    const int chunk_right = chunk.bounds.max_x;
    const int chunk_bottom = chunk.bounds.max_y;
    const int window_right = window.left + window.width;
    const int window_bottom = window.top + window.height;
    return chunk_left < window_right && chunk_right > window.left && chunk_top < window_bottom && chunk_bottom > window.top;
}

[[nodiscard]] std::vector<std::uint8_t> BuildInitialChunkSelection(
    const ChunkGrid& chunks,
    const InitialTileWindow& window,
    int* selected_count)
{
    std::vector<std::uint8_t> selected(chunks.chunks.size(), 1U);
    if (selected_count != nullptr) {
        *selected_count = static_cast<int>(selected.size());
    }
    if (!window.limited || !chunks.IsValid()) {
        return selected;
    }

    selected.assign(chunks.chunks.size(), 0U);
    int count = 0;
    for (std::size_t index = 0; index < chunks.chunks.size(); ++index) {
        if (ChunkIntersectsTileWindow(chunks.chunks[index], window)) {
            selected[index] = 1U;
            ++count;
        }
    }

    if (count == 0) {
        selected.assign(chunks.chunks.size(), 1U);
        count = static_cast<int>(selected.size());
    }
    if (selected_count != nullptr) {
        *selected_count = count;
    }
    return selected;
}


[[nodiscard]] std::vector<std::size_t> BuildProgressivePendingChunks(
    const ChunkGrid& chunks,
    const std::vector<std::uint8_t>& initial_selection,
    const InitialTileWindow& window)
{
    std::vector<std::size_t> pending;
    if (!chunks.IsValid() || initial_selection.size() != chunks.chunks.size()) {
        return pending;
    }

    const double window_center_x = static_cast<double>(window.left) + static_cast<double>(window.width) * 0.5;
    const double window_center_y = static_cast<double>(window.top) + static_cast<double>(window.height) * 0.5;
    pending.reserve(chunks.chunks.size());
    for (std::size_t index = 0; index < chunks.chunks.size(); ++index) {
        if (initial_selection[index] != 0U) {
            continue;
        }
        pending.push_back(index);
    }

    std::sort(pending.begin(), pending.end(), [&](std::size_t lhs, std::size_t rhs) {
        const ChunkInfo& left_chunk = chunks.chunks[lhs];
        const ChunkInfo& right_chunk = chunks.chunks[rhs];
        const double left_x = (static_cast<double>(left_chunk.bounds.min_x) + static_cast<double>(left_chunk.bounds.max_x)) * 0.5;
        const double left_y = (static_cast<double>(left_chunk.bounds.min_y) + static_cast<double>(left_chunk.bounds.max_y)) * 0.5;
        const double right_x = (static_cast<double>(right_chunk.bounds.min_x) + static_cast<double>(right_chunk.bounds.max_x)) * 0.5;
        const double right_y = (static_cast<double>(right_chunk.bounds.min_y) + static_cast<double>(right_chunk.bounds.max_y)) * 0.5;
        const double left_dx = left_x - window_center_x;
        const double left_dy = left_y - window_center_y;
        const double right_dx = right_x - window_center_x;
        const double right_dy = right_y - window_center_y;
        const double left_distance = left_dx * left_dx + left_dy * left_dy;
        const double right_distance = right_dx * right_dx + right_dy * right_dy;
        if (left_distance == right_distance) {
            return lhs < rhs;
        }
        return left_distance < right_distance;
    });
    return pending;
}

struct ProgressiveBuildPriority {
    double target_x = 0.0;
    double target_y = 0.0;
    double lookahead_x = 0.0;
    double lookahead_y = 0.0;
    bool camera_based = false;
};

[[nodiscard]] ProgressiveBuildPriority BuildProgressivePriority(
    const RuntimeMap& map,
    const FreeFlyCameraStatus& camera,
    int chunk_size_tiles)
{
    ProgressiveBuildPriority priority;
    const double map_width = static_cast<double>(std::max(1, map.info.width));
    const double map_height = static_cast<double>(std::max(1, map.info.height));
    priority.target_x = map_width * 0.5;
    priority.target_y = map_height * 0.5;
    priority.lookahead_x = priority.target_x;
    priority.lookahead_y = priority.target_y;

    if (!camera.initialized) {
        return priority;
    }

    priority.target_x = std::clamp(static_cast<double>(camera.target.x) + map_width * 0.5, 0.0, map_width - 1.0);
    priority.target_y = std::clamp(map_height * 0.5 - static_cast<double>(camera.target.z), 0.0, map_height - 1.0);
    priority.lookahead_x = priority.target_x;
    priority.lookahead_y = priority.target_y;
    priority.camera_based = true;

    const double forward_x = static_cast<double>(camera.target.x - camera.position.x);
    const double forward_y = static_cast<double>(camera.position.z - camera.target.z);
    const double forward_len = std::sqrt(forward_x * forward_x + forward_y * forward_y);
    if (forward_len > 0.000001) {
        const double lookahead_tiles = static_cast<double>(std::max(1, chunk_size_tiles))
            * static_cast<double>(ReadIntegerEnvironment("VOX3D_PROGRESSIVE_LOOKAHEAD_CHUNKS").value_or(6));
        priority.lookahead_x = std::clamp(priority.target_x + forward_x / forward_len * lookahead_tiles, 0.0, map_width - 1.0);
        priority.lookahead_y = std::clamp(priority.target_y + forward_y / forward_len * lookahead_tiles, 0.0, map_height - 1.0);
    }
    return priority;
}

[[nodiscard]] double ChunkCenterDistanceSq(const ChunkInfo& chunk, double tile_x, double tile_y)
{
    const double center_x = (static_cast<double>(chunk.bounds.min_x) + static_cast<double>(chunk.bounds.max_x)) * 0.5;
    const double center_y = (static_cast<double>(chunk.bounds.min_y) + static_cast<double>(chunk.bounds.max_y)) * 0.5;
    const double dx = center_x - tile_x;
    const double dy = center_y - tile_y;
    return dx * dx + dy * dy;
}

[[nodiscard]] double ChunkInterestDistanceSq(const ChunkInfo& chunk, const ProgressiveBuildPriority& priority)
{
    return std::min(
        ChunkCenterDistanceSq(chunk, priority.target_x, priority.target_y),
        ChunkCenterDistanceSq(chunk, priority.lookahead_x, priority.lookahead_y));
}

[[nodiscard]] double ChunkPriorityScore(const ChunkInfo& chunk, const ProgressiveBuildPriority& priority)
{
    const double center_x = (static_cast<double>(chunk.bounds.min_x) + static_cast<double>(chunk.bounds.max_x)) * 0.5;
    const double center_y = (static_cast<double>(chunk.bounds.min_y) + static_cast<double>(chunk.bounds.max_y)) * 0.5;
    const double look_dx = center_x - priority.lookahead_x;
    const double look_dy = center_y - priority.lookahead_y;
    const double target_dx = center_x - priority.target_x;
    const double target_dy = center_y - priority.target_y;
    return look_dx * look_dx + look_dy * look_dy + (target_dx * target_dx + target_dy * target_dy) * 0.35;
}

[[nodiscard]] std::optional<std::size_t> PopBestProgressiveChunk(
    const ChunkGrid& chunks,
    std::vector<std::size_t>& pending,
    const ProgressiveBuildPriority& priority,
    int interest_radius_chunks,
    int chunk_size_tiles)
{
    if (pending.empty()) {
        return std::nullopt;
    }

    const double radius_tiles = static_cast<double>(std::max(1, interest_radius_chunks))
        * static_cast<double>(std::max(1, chunk_size_tiles));
    const double radius_sq = radius_tiles * radius_tiles;
    std::size_t best_position = pending.size();
    double best_score = std::numeric_limits<double>::infinity();
    for (std::size_t position = 0; position < pending.size(); ++position) {
        const std::size_t chunk_index = pending[position];
        if (chunk_index >= chunks.chunks.size()) {
            continue;
        }
        if (ChunkInterestDistanceSq(chunks.chunks[chunk_index], priority) > radius_sq) {
            continue;
        }
        const double score = ChunkPriorityScore(chunks.chunks[chunk_index], priority);
        if (score < best_score
            || (score == best_score && (best_position >= pending.size() || chunk_index < pending[best_position]))) {
            best_score = score;
            best_position = position;
        }
    }

    if (best_position >= pending.size()) {
        return std::nullopt;
    }

    const std::size_t chunk_index = pending[best_position];
    pending.erase(pending.begin() + static_cast<std::ptrdiff_t>(best_position));
    return chunk_index;
}

[[nodiscard]] int ResolveProgressiveBuildsPerFrame(bool partial_initial_mesh)
{
    if (!partial_initial_mesh) {
        return 0;
    }
    if (ReadIntegerEnvironment("VOX3D_DISABLE_PROGRESSIVE_BUILD").value_or(0) != 0) {
        return 0;
    }
    return std::clamp(ReadIntegerEnvironment("VOX3D_CHUNK_BUILDS_PER_FRAME").value_or(1), 0, 32);
}


[[nodiscard]] int ResolveProgressiveInterestRadiusChunks()
{
    return std::clamp(ReadIntegerEnvironment("VOX3D_CHUNK_INTEREST_RADIUS").value_or(10), 1, 64);
}

[[nodiscard]] int ResolveProgressiveKeepRadiusChunks()
{
    return std::clamp(ReadIntegerEnvironment("VOX3D_CHUNK_KEEP_RADIUS").value_or(14), 1, 96);
}

[[nodiscard]] int ResolveRenderChunkBudget()
{
    return std::clamp(ReadIntegerEnvironment("VOX3D_RENDER_CHUNK_BUDGET").value_or(640), 1, 100000);
}

[[nodiscard]] int ResolveRenderChunkHardLimit()
{
    const int budget = ResolveRenderChunkBudget();
    return std::max(budget, std::clamp(ReadIntegerEnvironment("VOX3D_RENDER_CHUNK_HARD_LIMIT").value_or(768), 1, 100000));
}

[[nodiscard]] int ResolveProgressiveEvictsPerFrame()
{
    return std::clamp(ReadIntegerEnvironment("VOX3D_CHUNK_EVICTS_PER_FRAME").value_or(4), 0, 256);
}

[[nodiscard]] int ResolveProgressiveTimeBudgetMs()
{
    return std::clamp(ReadIntegerEnvironment("VOX3D_PROGRESSIVE_TIME_BUDGET_MS").value_or(6), 0, 250);
}

void RecalculateCacheCountersLocal(ChunkMeshCache& cache)
{
    cache.info.non_empty_chunks = 0;
    cache.info.faces = 0;
    cache.info.vertices = 0;
    cache.info.indices = 0;
    cache.info.dirty_chunks = cache.DirtyCount();
    for (const ChunkMeshData& mesh : cache.chunks) {
        if (!mesh.faces.empty()) {
            ++cache.info.non_empty_chunks;
        }
        cache.info.faces += static_cast<std::uint64_t>(mesh.faces.size());
        cache.info.vertices += static_cast<std::uint64_t>(mesh.vertices.size());
        cache.info.indices += static_cast<std::uint64_t>(mesh.indices.size());
    }
}

void MergeSelectedCacheChunks(ChunkMeshCache& target, const ChunkMeshCache& source, const std::vector<std::uint8_t>& selected)
{
    if (target.chunks.size() != source.chunks.size() || selected.size() != source.chunks.size()) {
        return;
    }
    for (std::size_t index = 0; index < selected.size(); ++index) {
        if (selected[index] == 0U) {
            continue;
        }
        target.chunks[index] = source.chunks[index];
        if (index < target.dirty.size()) {
            target.dirty[index] = 0U;
        }
    }
    RecalculateCacheCountersLocal(target);
}

void RecalculateMeshBuildInfoLocal(ChunkMeshBuildResult& result)
{
    result.info.visible_faces = 0;
    result.info.terrain_raw_top_faces = 0;
    result.info.terrain_raw_wall_faces = 0;
    result.info.terrain_top_faces = 0;
    result.info.terrain_wall_faces = 0;
    result.info.terrain_cliff_faces = 0;
    result.info.vertices = 0;
    result.info.indices = 0;
    result.info.non_empty_chunks = 0;
    for (const ChunkMeshData& mesh : result.chunks) {
        if (!mesh.faces.empty()) {
            ++result.info.non_empty_chunks;
        }
        result.info.visible_faces += static_cast<std::uint64_t>(mesh.faces.size());
        result.info.vertices += static_cast<std::uint64_t>(mesh.vertices.size());
        result.info.indices += static_cast<std::uint64_t>(mesh.indices.size());
        for (const MeshFace& face : mesh.faces) {
            switch (face.terrain_pass) {
                case TerrainRenderPass::kTops:
                    ++result.info.terrain_raw_top_faces;
                    ++result.info.terrain_top_faces;
                    break;
                case TerrainRenderPass::kWalls:
                    ++result.info.terrain_raw_wall_faces;
                    ++result.info.terrain_wall_faces;
                    break;
                case TerrainRenderPass::kCliffs:
                    ++result.info.terrain_raw_wall_faces;
                    ++result.info.terrain_cliff_faces;
                    break;
                case TerrainRenderPass::kBody:
                    break;
            }
        }
    }
}

void MergeSelectedMeshBuildChunks(
    ChunkMeshBuildResult& target,
    const ChunkMeshBuildResult& source,
    const std::vector<std::uint8_t>& selected)
{
    if (target.chunks.size() != source.chunks.size() || selected.size() != source.chunks.size()) {
        return;
    }
    for (std::size_t index = 0; index < selected.size(); ++index) {
        if (selected[index] == 0U) {
            continue;
        }
        target.chunks[index] = source.chunks[index];
    }
    RecalculateMeshBuildInfoLocal(target);
}


void ClearSelectedCacheChunks(ChunkMeshCache& target, const std::vector<std::uint8_t>& selected)
{
    if (selected.size() != target.chunks.size()) {
        return;
    }
    for (std::size_t index = 0; index < selected.size(); ++index) {
        if (selected[index] == 0U) {
            continue;
        }
        target.chunks[index].vertices.clear();
        target.chunks[index].indices.clear();
        target.chunks[index].faces.clear();
        if (index < target.dirty.size()) {
            target.dirty[index] = 0U;
        }
    }
    RecalculateCacheCountersLocal(target);
}

void ClearSelectedMeshBuildChunks(ChunkMeshBuildResult& target, const std::vector<std::uint8_t>& selected)
{
    if (selected.size() != target.chunks.size()) {
        return;
    }
    for (std::size_t index = 0; index < selected.size(); ++index) {
        if (selected[index] == 0U) {
            continue;
        }
        target.chunks[index].vertices.clear();
        target.chunks[index].indices.clear();
        target.chunks[index].faces.clear();
    }
    RecalculateMeshBuildInfoLocal(target);
}

void AddPendingChunkIfMissing(std::vector<std::size_t>& pending, std::size_t chunk_index)
{
    if (std::find(pending.begin(), pending.end(), chunk_index) == pending.end()) {
        pending.push_back(chunk_index);
    }
}

[[nodiscard]] std::vector<std::size_t> SelectProgressiveEvictionChunks(
    const ChunkGrid& chunks,
    const std::vector<std::uint8_t>& resident,
    const ProgressiveBuildPriority& priority,
    int keep_radius_chunks,
    int chunk_size_tiles,
    int resident_budget,
    int resident_hard_limit,
    int evicts_per_frame)
{
    std::vector<std::size_t> evictable;
    const int resident_count = static_cast<int>(std::count(resident.begin(), resident.end(), static_cast<std::uint8_t>(1)));
    if (!chunks.IsValid() || resident_count < resident_hard_limit || evicts_per_frame <= 0) {
        return evictable;
    }

    const double keep_tiles = static_cast<double>(std::max(1, keep_radius_chunks))
        * static_cast<double>(std::max(1, chunk_size_tiles));
    const double keep_sq = keep_tiles * keep_tiles;
    const bool over_hard_limit = resident_count > resident_hard_limit;
    for (std::size_t index = 0; index < resident.size() && index < chunks.chunks.size(); ++index) {
        if (resident[index] == 0U) {
            continue;
        }
        const double distance_sq = ChunkInterestDistanceSq(chunks.chunks[index], priority);
        if (over_hard_limit || distance_sq > keep_sq) {
            evictable.push_back(index);
        }
    }

    std::sort(evictable.begin(), evictable.end(), [&](std::size_t lhs, std::size_t rhs) {
        const double left = ChunkInterestDistanceSq(chunks.chunks[lhs], priority);
        const double right = ChunkInterestDistanceSq(chunks.chunks[rhs], priority);
        if (left == right) {
            return lhs > rhs;
        }
        return left > right;
    });

    const int target_evict_count = std::max(0, resident_count - resident_budget);
    const std::size_t limit = static_cast<std::size_t>(std::min(evicts_per_frame, target_evict_count));
    if (evictable.size() > limit) {
        evictable.resize(limit);
    }
    return evictable;
}

void MergeFaceVisibility(FaceVisibilityResult& target, const FaceVisibilityResult& source)
{
    target.info.solid_blocks += source.info.solid_blocks;
    target.info.naive_faces += source.info.naive_faces;
    target.info.visible_faces += source.info.visible_faces;
    target.info.culled_faces += source.info.culled_faces;
    for (std::size_t index = 0; index < target.info.visible_by_direction.values.size(); ++index) {
        target.info.visible_by_direction.values[index] += source.info.visible_by_direction.values[index];
    }
    for (const auto& warning : source.diagnostics.warnings) {
        target.diagnostics.AddWarning(warning);
    }
}

[[nodiscard]] std::uint64_t WorldGridLineCount(const ChunkMeshBuildResult& mesh)
{
    if (!mesh.IsValid()) {
        return 0;
    }
    const int step = std::max(4, mesh.info.chunk_size_x > 0 ? mesh.info.chunk_size_x : 16);
    const auto x_lines = static_cast<std::uint64_t>((mesh.info.map_width + step) / step);
    const auto y_lines = static_cast<std::uint64_t>((mesh.info.map_height + step) / step);
    return x_lines + y_lines;
}

[[nodiscard]] std::uint64_t HeightMarkerCount(const RuntimeMap& map)
{
    if (!map.height.IsValid()) {
        return 0;
    }
    const int sample_step = std::max(1, std::max(map.info.width, map.info.height) / 96);
    const auto x_count = static_cast<std::uint64_t>((map.info.width + sample_step - 1) / sample_step);
    const auto y_count = static_cast<std::uint64_t>((map.info.height + sample_step - 1) / sample_step);
    return x_count * y_count;
}

[[nodiscard]] std::uint64_t OverlayPrimitiveCount(const WorkspaceState& workspace, WorkspacePanelItem item)
{
    switch (item) {
        case WorkspacePanelItem::kRenderChunkBounds:
            return workspace.chunk_grid.IsValid() ? static_cast<std::uint64_t>(workspace.chunk_grid.info.total_chunks) * 4ULL : 0;
        case WorkspacePanelItem::kRenderWorldGrid:
            return WorldGridLineCount(workspace.chunk_meshes);
        case WorkspacePanelItem::kRenderCollision:
            return workspace.runtime_map.info.collision_loaded
                ? static_cast<std::uint64_t>(workspace.runtime_map.info.blocked_cells)
                : 0;
        case WorkspacePanelItem::kRenderHeight:
            return HeightMarkerCount(workspace.runtime_map);
        case WorkspacePanelItem::k3DObjectsAll:
        case WorkspacePanelItem::k3DObjectsTrees:
        case WorkspacePanelItem::k3DObjectsBushes:
        case WorkspacePanelItem::k3DObjectsReeds:
        case WorkspacePanelItem::k3DObjectsRuins:
        case WorkspacePanelItem::k3DObjectsCover:
        case WorkspacePanelItem::k3DObjectsLoot:
        case WorkspacePanelItem::k3DObjectsStructures:
        case WorkspacePanelItem::k3DObjectsTrenches:
        case WorkspacePanelItem::k3DObjectsUnknown:
            return workspace.runtime_map.info.object_markers_loaded
                ? static_cast<std::uint64_t>(workspace.runtime_map.info.object_markers)
                : 0;
        default:
            return 0;
    }
}

[[nodiscard]] std::string PercentText(double ratio)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << ratio * 100.0 << '%';
    return out.str();
}

[[nodiscard]] int ToggleChunkSize(int chunk_size)
{
    return chunk_size == kChunkSize16 ? kChunkSize32 : kChunkSize16;
}

[[nodiscard]] WorkspaceColorMode NextColorMode(WorkspaceColorMode mode)
{
    switch (mode) {
        case WorkspaceColorMode::kTraversal:
            return WorkspaceColorMode::kGeographic;
        case WorkspaceColorMode::kGeographic:
            return WorkspaceColorMode::kChunkId;
        case WorkspaceColorMode::kChunkId:
            return WorkspaceColorMode::kFaceType;
        case WorkspaceColorMode::kFaceType:
            return WorkspaceColorMode::kTraversal;
    }
    return WorkspaceColorMode::kTraversal;
}

[[nodiscard]] RaylibChunkMeshColorMode ToRaylibColorMode(WorkspaceColorMode mode)
{
    switch (mode) {
        case WorkspaceColorMode::kTraversal:
            return RaylibChunkMeshColorMode::kTraversal;
        case WorkspaceColorMode::kGeographic:
            return RaylibChunkMeshColorMode::kGeographic;
        case WorkspaceColorMode::kChunkId:
            return RaylibChunkMeshColorMode::kChunkId;
        case WorkspaceColorMode::kFaceType:
            return RaylibChunkMeshColorMode::kFaceType;
    }
    return RaylibChunkMeshColorMode::kTraversal;
}

[[nodiscard]] WorkspaceVisibilityMode NextVisibilityMode(WorkspaceVisibilityMode mode)
{
    switch (mode) {
        case WorkspaceVisibilityMode::kAllChunks:
            return WorkspaceVisibilityMode::kRadiusFade;
        case WorkspaceVisibilityMode::kRadiusFade:
            return WorkspaceVisibilityMode::kHardCull;
        case WorkspaceVisibilityMode::kHardCull:
            return WorkspaceVisibilityMode::kFrustumCull;
        case WorkspaceVisibilityMode::kFrustumCull:
            return WorkspaceVisibilityMode::kAllChunks;
    }
    return WorkspaceVisibilityMode::kAllChunks;
}

[[nodiscard]] RaylibChunkVisibilityMode ToRaylibVisibilityMode(WorkspaceVisibilityMode mode)
{
    switch (mode) {
        case WorkspaceVisibilityMode::kAllChunks:
            return RaylibChunkVisibilityMode::kAllChunks;
        case WorkspaceVisibilityMode::kRadiusFade:
            return RaylibChunkVisibilityMode::kRadiusFade;
        case WorkspaceVisibilityMode::kHardCull:
            return RaylibChunkVisibilityMode::kHardCull;
        case WorkspaceVisibilityMode::kFrustumCull:
            return RaylibChunkVisibilityMode::kFrustumCull;
    }
    return RaylibChunkVisibilityMode::kAllChunks;
}

[[nodiscard]] float CurrentRenderAspectRatio()
{
    const int width = std::max(1, GetScreenWidth());
    const int height = std::max(1, GetScreenHeight());
    return static_cast<float>(width) / static_cast<float>(height);
}

[[nodiscard]] RaylibChunkVisibilityOptions BuildRaylibVisibilityOptions(
    const WorkspaceState& workspace,
    Rectangle /*viewport*/)
{
    return RaylibChunkVisibilityOptions{
        ToRaylibVisibilityMode(workspace.visibility_mode),
        workspace.visibility_radius_chunks,
        workspace.visibility_fade_ring_chunks,
        workspace.show_3d_hidden_chunk_bounds,
        CurrentRenderAspectRatio(),
    };
}

[[nodiscard]] RaylibTerrainPassOptions BuildRaylibTerrainPassOptions(const WorkspaceState& workspace)
{
    return RaylibTerrainPassOptions{
        workspace.show_terrain_tops,
        workspace.show_terrain_walls,
        workspace.show_terrain_cliffs,
    };
}

[[nodiscard]] WorkspaceVisibilityStats ToWorkspaceVisibilityStats(const RaylibChunkVisibilityStats& stats)
{
    WorkspaceVisibilityStats result;
    switch (stats.mode) {
        case RaylibChunkVisibilityMode::kAllChunks:
            result.mode = WorkspaceVisibilityMode::kAllChunks;
            break;
        case RaylibChunkVisibilityMode::kRadiusFade:
            result.mode = WorkspaceVisibilityMode::kRadiusFade;
            break;
        case RaylibChunkVisibilityMode::kHardCull:
            result.mode = WorkspaceVisibilityMode::kHardCull;
            break;
        case RaylibChunkVisibilityMode::kFrustumCull:
            result.mode = WorkspaceVisibilityMode::kFrustumCull;
            break;
    }
    result.radius_chunks = stats.radius_chunks;
    result.fade_ring_chunks = stats.fade_ring_chunks;
    result.resident_chunks = stats.resident_chunks;
    result.resident_models = stats.resident_models;
    result.visible_chunks = stats.visible_chunks;
    result.fade_chunks = stats.fade_chunks;
    result.hidden_chunks = stats.hidden_chunks;
    result.drawn_models = stats.drawn_models;
    result.culled_models = stats.culled_models;
    result.total_faces = stats.total_faces;
    result.drawn_faces = stats.drawn_faces;
    result.culled_faces = stats.culled_faces;
    return result;
}

[[nodiscard]] bool IsSupportedChunkSize(int chunk_size)
{
    return chunk_size == kChunkSize16 || chunk_size == kChunkSize32;
}

[[nodiscard]] std::string ChunkComparisonLogString(const WorkspaceChunkSizeComparison& comparison)
{
    if (!comparison.available) {
        return "status=unavailable";
    }

    std::ostringstream out;
    out << "before=" << comparison.before_chunk_size;
    out << " after=" << comparison.after_chunk_size;
    out << " chunks=" << comparison.before_total_chunks << "->" << comparison.after_total_chunks;
    out << " draw_models=" << comparison.before_draw_models << "->" << comparison.after_draw_models;
    out << " draw_delta=" << PercentText(comparison.DrawModelDeltaRatio());
    out << " faces=" << comparison.before_active_faces << "->" << comparison.after_active_faces;
    out << " face_delta=" << PercentText(comparison.FaceDeltaRatio());
    return out.str();
}

[[nodiscard]] ChunkMeshRebuildReport FullCacheBuildReport(const ChunkMeshCache& cache)
{
    ChunkMeshRebuildReport report;
    report.mode = cache.info.mode;
    report.attempted = true;
    report.valid = cache.IsValid();
    report.total_chunks = cache.info.total_chunks;
    report.dirty_chunks = static_cast<std::uint64_t>(cache.info.total_chunks);
    report.rebuilt_chunks = static_cast<std::uint64_t>(cache.info.total_chunks);
    report.reused_chunks = 0;
    report.old_faces = 0;
    report.new_faces = cache.info.faces;
    report.old_vertices = 0;
    report.new_vertices = cache.info.vertices;
    report.old_indices = 0;
    report.new_indices = cache.info.indices;
    return report;
}

[[nodiscard]] TileCoord CenterTile(const RuntimeMap& map)
{
    return TileCoord{map.info.width / 2, map.info.height / 2};
}

[[nodiscard]] int RaylibTraceLogLevel(std::string_view value)
{
    const std::string normalized = Lowercase(value);
    if (normalized == "trace") {
        return LOG_TRACE;
    }
    if (normalized == "debug") {
        return LOG_DEBUG;
    }
    if (normalized == "info") {
        return LOG_INFO;
    }
    if (normalized == "warn" || normalized == "warning") {
        return LOG_WARNING;
    }
    if (normalized == "error") {
        return LOG_ERROR;
    }
    if (normalized == "fatal") {
        return LOG_FATAL;
    }
    return LOG_WARNING;
}

}  // namespace

App::App(AppConfig config, Logger& logger, UiLabels labels)
    : config_(std::move(config)), logger_(logger), labels_(std::move(labels)), main_menu_(labels_)
{
}

bool App::Initialize()
{
    const SteadyTimePoint startup_start = Now();

    logger_.Info("app", "started version=" + config_.version + " mode=" + BuildMode());
    logger_.Info("app", "working_directory=" + CurrentWorkingDirectory());
    logger_.Info(
        "log",
        "level=" + std::string(ToString(config_.log_level)) + " color="
            + std::string(config_.no_color || !config_.log_color ? "disabled" : "auto")
            + " raylib_level=" + config_.raylib_log_level);

    unsigned int raylib_flags = 0;
    if (config_.window_vsync) {
        raylib_flags |= FLAG_VSYNC_HINT;
    }
    if (config_.window_resizable) {
        raylib_flags |= FLAG_WINDOW_RESIZABLE;
    }
    if (raylib_flags != 0) {
        SetConfigFlags(raylib_flags);
    }
    SetTraceLogLevel(RaylibTraceLogLevel(config_.raylib_log_level));
    SetExitKey(KEY_NULL);

    InitWindow(config_.base_width, config_.base_height, config_.app_name.c_str());
    SetExitKey(KEY_NULL);
    if (!IsWindowReady()) {
        logger_.Fatal(
            "window",
            "failed to initialize requested=" + std::to_string(config_.base_width) + "x"
                + std::to_string(config_.base_height) + " title=\"" + config_.app_name + "\"");
        return false;
    }

    window_initialized_ = true;

    const int monitor_index = GetCurrentMonitor();
    const Vector2 monitor_position = GetMonitorPosition(monitor_index);
    int monitor_width = GetMonitorWidth(monitor_index);
    int monitor_height = GetMonitorHeight(monitor_index);
    if (monitor_width <= 0 || monitor_height <= 0) {
        logger_.Warn("window", "monitor size unavailable after InitWindow, using conservative fallback 1920x1080");
        monitor_width = 1920;
        monitor_height = 1080;
    }

    window_config_ = CalculateWindowConfig(
        static_cast<int>(monitor_position.x),
        static_cast<int>(monitor_position.y),
        monitor_width,
        monitor_height,
        config_);
    window_config_.monitor_index = monitor_index;

    if (config_.window_fullscreen) {
        SetWindowSize(monitor_width, monitor_height);
        SetWindowPosition(static_cast<int>(monitor_position.x), static_cast<int>(monitor_position.y));
        if (!IsWindowFullscreen()) {
            ToggleFullscreen();
        }
        window_config_.window_width = GetScreenWidth();
        window_config_.window_height = GetScreenHeight();
        window_config_.window_x = static_cast<int>(monitor_position.x);
        window_config_.window_y = static_cast<int>(monitor_position.y);
        window_config_.ui_scale = CalculateUiScale(window_config_.window_width, window_config_.window_height, config_);
    } else {
        if (GetScreenWidth() != window_config_.window_width || GetScreenHeight() != window_config_.window_height) {
            SetWindowSize(window_config_.window_width, window_config_.window_height);
        }
        SetWindowPosition(window_config_.window_x, window_config_.window_y);
    }
    SetTargetFPS(config_.target_fps);

    const SteadyTimePoint map_package_start = Now();
    workspace_.map = LoadMapPackageInfo(config_.map_package_path);
    const SteadyTimePoint map_package_finish = Now();
    logger_.Info("map", ToLogString(workspace_.map));
    for (const auto& warning : workspace_.map.warnings) {
        logger_.Warn("map", warning);
    }

    const SteadyTimePoint runtime_map_start = Now();
    workspace_.runtime_map = BuildRuntimeMap(workspace_.map);
    const SteadyTimePoint runtime_map_finish = Now();
    logger_.Info("runtime_map", ToLogString(workspace_.runtime_map));
    for (const auto& warning : workspace_.runtime_map.diagnostics.warnings) {
        logger_.Warn("runtime_map", warning);
    }

    const SteadyTimePoint chunk_pipeline_start = Now();
    RebuildChunkPipeline(workspace_.chunk_size_tiles, "initial");
    const SteadyTimePoint chunk_pipeline_finish = Now();
    main_menu_.SetItemEnabled(MenuItemId::kLoadGame, workspace_.map.loaded);

    const SteadyTimePoint fonts_start = Now();
    LoadUiFonts();
    const SteadyTimePoint fonts_finish = Now();

    RefreshProcessMemoryInfo();

    const SteadyTimePoint layout_start = Now();
    RebuildLayout();
    const SteadyTimePoint layout_finish = Now();

    const SteadyTimePoint camera_start = Now();
    if (chunk_mesh_preview_.IsUploaded()) {
        const int initial_window_size = ResolveInitialTileWindowSize(workspace_.runtime_map);
        const InitialTileWindow initial_window = ResolveInitialTileWindow(workspace_.runtime_map, initial_window_size);
        const std::string startup_view = Lowercase(ReadStringEnvironment("VOX3D_STARTUP_VIEW").value_or("window_corner"));
        if (initial_window.limited && startup_view != "map" && startup_view != "flyin") {
            const std::string corner = NormalizeStartupCorner(ReadStringEnvironment("VOX3D_STARTUP_CORNER").value_or("se"));
            preview_camera_.SetTileWindowCornerView(
                workspace_.chunk_meshes,
                initial_window.left,
                initial_window.top,
                initial_window.width,
                initial_window.height,
                corner);
            std::ostringstream out;
            out << "mode=window_corner";
            out << " corner=" << corner;
            out << " initial_window=" << initial_window.left << ',' << initial_window.top << ','
                << initial_window.width << 'x' << initial_window.height;
            out << ' ' << ToLogString(preview_camera_.Status());
            logger_.Info("startup_view", out.str());
        } else {
            preview_camera_.StartFlyInToMap(workspace_.chunk_meshes, layout_cache_.workspace.map_overview);
            logger_.Info("camera3d", "startup fly-in " + ToLogString(preview_camera_.Status()));
        }
    }
    const SteadyTimePoint camera_finish = Now();

    {
        const SteadyTimePoint startup_finish = Now();
        std::ostringstream out;
        out << "map_package_ms=" << ElapsedMs(map_package_start, map_package_finish);
        out << " runtime_map_ms=" << ElapsedMs(runtime_map_start, runtime_map_finish);
        out << " chunk_pipeline_ms=" << ElapsedMs(chunk_pipeline_start, chunk_pipeline_finish);
        out << " fonts_ms=" << ElapsedMs(fonts_start, fonts_finish);
        out << " layout_ms=" << ElapsedMs(layout_start, layout_finish);
        out << " camera_ms=" << ElapsedMs(camera_start, camera_finish);
        out << " total_startup_ms=" << ElapsedMs(startup_start, startup_finish);
        logger_.Info("startup_profile", out.str());
    }

    {
        std::ostringstream out;
        out << "monitor=" << window_config_.monitor_width << 'x' << window_config_.monitor_height << " fullscreen="
            << (config_.window_fullscreen ? "yes" : "no") << " window="
            << window_config_.window_width << 'x' << window_config_.window_height << " pos=" << window_config_.window_x
            << ',' << window_config_.window_y << " ui_scale=" << window_config_.ui_scale;
        logger_.Info("window", out.str());
    }
    {
        std::ostringstream out;
        out << main_menu_.State();
        logger_.Debug("menu", out.str());
    }
    logger_.Info("app", "opened screen=workspace");

    return true;
}

int App::Run()
{
    if (!window_initialized_) {
        logger_.Fatal("app", "Run called before successful Initialize");
        return 1;
    }

    running_ = true;
    while (running_) {
        suppress_window_close_request_this_frame_ = false;

        const float dt = GetFrameTime();
        HandleInput(dt);
        Update(dt);

        const bool close_requested = WindowShouldClose();
        if (close_requested && suppress_window_close_request_this_frame_) {
            logger_.Debug("window", "close request suppressed after mouse release");
            window_close_request_armed_ = false;
        } else if (close_requested && window_close_request_armed_) {
            logger_.Info("window", "close requested");
            RequestExitConfirmation(true);
            window_close_request_armed_ = false;
        } else if (!close_requested) {
            window_close_request_armed_ = true;
        }

        Draw();
    }

    logger_.Info("app", "shutting down");
    return 0;
}

void App::Shutdown()
{
    preview_camera_.ReleaseMouse();
    UnloadPreviewResources();
    UnloadUiFonts();
    if (window_initialized_) {
        CloseWindow();
        window_initialized_ = false;
    }
}

void App::HandleInput(float dt)
{
    hovered_item_ = labels_.debug_none;

    if (dialog_.type != ModalDialog::kNone) {
        if (dialog_input_blocked_until_next_frame_) {
            dialog_input_blocked_until_next_frame_ = false;
            return;
        }
        HandleDialogInput();
        return;
    }

    if (screen_ == AppScreen::kMainMenu) {
        HandleMainMenuInput();
        return;
    }

    HandleScreenInput(dt);
}

void App::HandleMainMenuInput()
{
    if (IsKeyPressedAny({KEY_UP, KEY_W})) {
        if (main_menu_.SelectPrevious()) {
            LogSelectedItemChanged();
        }
    }
    if (IsKeyPressedAny({KEY_DOWN, KEY_S})) {
        if (main_menu_.SelectNext()) {
            LogSelectedItemChanged();
        }
    }
    if (IsKeyPressed(KEY_ENTER)) {
        ActivateSelectedMenuItem();
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        RequestExitConfirmation();
    }

    if (layout_dirty_) {
        RebuildLayout();
    }

    const Vector2 mouse = GetMousePosition();
    for (const auto& item_bounds : layout_cache_.main_menu.items) {
        const auto& item = main_menu_.State().items[static_cast<std::size_t>(item_bounds.index)];
        if (!PointInRect(mouse, item_bounds.bounds)) {
            continue;
        }

        hovered_item_ = std::string(ToString(item.id));
        if (item.enabled) {
            if (main_menu_.SelectByIndex(item_bounds.index)) {
                LogSelectedItemChanged();
            }
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                ActivateSelectedMenuItem();
            }
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            logger_.Debug("menu", "disabled item activation ignored id=" + std::string(ToString(item.id)));
        }
        break;
    }
}

void App::HandleScreenInput(float dt)
{
    if (screen_ == AppScreen::kWorkspace) {
        HandleWorkspaceInput(dt);
        return;
    }
    if (screen_ == AppScreen::kSettingsPlaceholder) {
        HandlePlaceholderInput();
    }
}

void App::HandleWorkspaceInput(float dt)
{
    const bool release_capture_by_escape = IsKeyPressed(KEY_ESCAPE);
    const bool release_capture_by_hotkey = IsKeyPressed(KEY_F2);
    const bool release_capture_by_right_mouse = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    const bool path_pick_active_before_input = workspace_.path_pick_mode != WorkspacePathPickMode::kSelect;
    if (path_pick_active_before_input && (release_capture_by_escape || release_capture_by_right_mouse)) {
        preview_camera_.ReleaseMouse();
        SetPathPickMode(WorkspacePathPickMode::kSelect, release_capture_by_escape ? "escape" : "right_mouse");
        if (release_capture_by_escape) {
            suppress_window_close_request_this_frame_ = true;
        }
        logger_.Debug("path", "path pick cancelled");
        return;
    }

    if (workspace_.show_3d_preview && preview_camera_.IsCursorCaptured()
        && (release_capture_by_escape || release_capture_by_hotkey || release_capture_by_right_mouse)) {
        preview_camera_.ReleaseMouse();
        if (release_capture_by_escape) {
            suppress_window_close_request_this_frame_ = true;
        }

        std::string release_reason = "f2";
        if (release_capture_by_escape) {
            release_reason = "escape";
        } else if (release_capture_by_right_mouse) {
            release_reason = "right_mouse";
        }
        logger_.Debug("camera3d", "mouse capture released by " + release_reason);
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        RequestExitConfirmation();
        return;
    }

    if (IsKeyPressed(KEY_F3) && workspace_.show_3d_preview && workspace_.runtime_map.IsValid()) {
        BeginPathPickMode("hotkey_f3");
    }

    if (layout_dirty_) {
        RebuildLayout();
    }

    const Vector2 panel_mouse = GetMousePosition();
    const bool mouse_over_workspace_panel = PointInRect(panel_mouse, layout_cache_.workspace.tool_menu);
    if (!preview_camera_.IsCursorCaptured() && mouse_over_workspace_panel) {
        const float wheel = GetMouseWheelMove();
        if (wheel > 0.0001F) {
            ScrollWorkspaceMenu(-3, "wheel");
        } else if (wheel < -0.0001F) {
            ScrollWorkspaceMenu(3, "wheel");
        }
    }
    if (workspace_.selected_panel_tab == WorkspacePanelTab::kMenu
        || workspace_.selected_panel_tab == WorkspacePanelTab::kStats
        || workspace_.selected_panel_tab == WorkspacePanelTab::kInspect) {
        if (IsKeyPressed(KEY_PAGE_UP)) {
            ScrollWorkspaceMenu(-6, "page_up");
        }
        if (IsKeyPressed(KEY_PAGE_DOWN)) {
            ScrollWorkspaceMenu(6, "page_down");
        }
        if (IsKeyPressed(KEY_HOME)) {
            ScrollWorkspaceMenu(-1000000, "home");
        }
        if (IsKeyPressed(KEY_END)) {
            ScrollWorkspaceMenu(1000000, "end");
        }
    }

    const bool camera_mode = workspace_.show_3d_preview && chunk_mesh_preview_.IsUploaded() && preview_camera_.IsInitialized();
    bool panel_tab_hotkey_pressed = false;
    if (!preview_camera_.IsCursorCaptured()) {
        if (IsKeyPressed(KEY_V)) {
            SetWorkspacePanelTab(WorkspacePanelTab::kMenu, "hotkey_v");
            panel_tab_hotkey_pressed = true;
        } else if (IsKeyPressed(KEY_S)) {
            SetWorkspacePanelTab(WorkspacePanelTab::kStats, "hotkey_s");
            panel_tab_hotkey_pressed = true;
        } else if (IsKeyPressed(KEY_I)) {
            SetWorkspacePanelTab(WorkspacePanelTab::kInspect, "hotkey_i");
            panel_tab_hotkey_pressed = true;
        }
    }

    if (layout_dirty_) {
        RebuildLayout();
    }

    if (camera_mode) {
        if (IsKeyPressed(KEY_R)) {
            preview_camera_.ResetView();
            logger_.Info("camera3d", "reset view " + ToLogString(preview_camera_.Status()));
        }
        if (IsKeyPressed(KEY_F)) {
            FitPreviewCameraToViewport("hotkey");
        }
        if (IsKeyPressed(KEY_F4)) {
            ToggleOverlayFlag(
                workspace_.show_3d_chunk_bounds,
                "chunk_bounds",
                OverlayPrimitiveCount(workspace_, WorkspacePanelItem::kRenderChunkBounds),
                logger_,
                layout_dirty_);
        }
        if (IsKeyPressed(KEY_F5)) {
            ToggleOverlayFlag(
                workspace_.show_3d_world_grid,
                "world_grid",
                OverlayPrimitiveCount(workspace_, WorkspacePanelItem::kRenderWorldGrid),
                logger_,
                layout_dirty_);
        }
        if (IsKeyPressed(KEY_F6)) {
            ToggleOverlayFlag(
                workspace_.show_3d_collision_overlay,
                "collision_overlay",
                OverlayPrimitiveCount(workspace_, WorkspacePanelItem::kRenderCollision),
                logger_,
                layout_dirty_);
        }
        if (IsKeyPressed(KEY_F7)) {
            ToggleOverlayFlag(
                workspace_.show_3d_height_overlay,
                "height_overlay",
                OverlayPrimitiveCount(workspace_, WorkspacePanelItem::kRenderHeight),
                logger_,
                layout_dirty_);
        }
        if (IsKeyPressed(KEY_F8)) {
            ChunkMeshBuildMode next_mode = ChunkMeshBuildMode::kSimpleFaces;
            if (workspace_.mesh_mode == ChunkMeshBuildMode::kSimpleFaces) {
                next_mode = ChunkMeshBuildMode::kGreedyFaces;
            } else if (workspace_.mesh_mode == ChunkMeshBuildMode::kGreedyFaces) {
                next_mode = ChunkMeshBuildMode::kTerrainSurface;
            }
            SetMeshBuildMode(next_mode, "hotkey");
        }
        if (IsKeyPressed(KEY_F9)) {
            SetChunkSize(ToggleChunkSize(workspace_.chunk_size_tiles), "hotkey");
        }
        if (IsKeyPressed(KEY_F10)) {
            RunDirtyRebuildProbe("hotkey");
        }
        if (IsKeyPressed(KEY_F11)) {
            CycleColorMode("hotkey");
        }
        if (IsKeyPressed(KEY_F12)) {
            CycleVisibilityMode("hotkey");
        }
        if (IsKeyPressed(KEY_T)) {
            ToggleTransitionOverlay("hotkey");
        }
        if (IsKeyPressed(KEY_M)) {
            ToggleMovementProbeOverlay("hotkey");
        }
        if (IsKeyPressed(KEY_V) && !panel_tab_hotkey_pressed) {
            TogglePassabilityValidationOverlay("hotkey");
        }
        if (IsKeyPressed(KEY_P)) {
            BeginPathPickMode("hotkey");
        }
        if (IsKeyPressed(KEY_X)) {
            ClearPathProbe("hotkey");
        }
        const Vector2 pick_mouse = GetMousePosition();
        const bool path_pick_active_before_click = workspace_.path_pick_mode != WorkspacePathPickMode::kSelect;
        if (!preview_camera_.IsCursorCaptured() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
            && PointInRect(pick_mouse, layout_cache_.workspace.map_overview)) {
            SelectTileAtMouse(pick_mouse, "mouse");
        }
        preview_camera_.Update(dt, layout_cache_.workspace.map_overview, !path_pick_active_before_click);
    } else {
        preview_camera_.Update(dt, layout_cache_.workspace.map_overview, false);
    }

    if (!panel_tab_hotkey_pressed) {
        if (camera_mode) {
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_LEFT)) {
                SelectPreviousWorkspaceTool();
            }
            if (IsKeyPressedAny({KEY_DOWN, KEY_RIGHT, KEY_TAB})) {
                SelectNextWorkspaceTool();
            }
        } else {
            if (IsKeyPressedAny({KEY_UP, KEY_W, KEY_LEFT, KEY_A})) {
                SelectPreviousWorkspaceTool();
            }
            if (IsKeyPressedAny({KEY_DOWN, KEY_S, KEY_RIGHT, KEY_D, KEY_TAB})) {
                SelectNextWorkspaceTool();
            }
        }
    }
    if (IsKeyPressed(KEY_ENTER) && workspace_.selected_panel_tab != WorkspacePanelTab::kMenu) {
        SetWorkspacePanelTab(WorkspacePanelTab::kMenu, "enter");
    }

    if (layout_dirty_) {
        RebuildLayout();
    }

    const Vector2 mouse = GetMousePosition();
    for (const auto& tab_bounds : layout_cache_.workspace.panel_tabs) {
        if (!PointInRect(mouse, tab_bounds.bounds)) {
            continue;
        }

        hovered_item_ = "workspace_tab_" + std::string(ToString(tab_bounds.tab));
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            SetWorkspacePanelTab(tab_bounds.tab, "mouse");
        }
        return;
    }

    for (const auto& item_bounds : layout_cache_.workspace.panel_items) {
        if (!PointInRect(mouse, item_bounds.bounds)) {
            continue;
        }

        hovered_item_ = "workspace_" + std::string(ToString(item_bounds.item));
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ActivateWorkspacePanelItem(item_bounds.item);
        }
        return;
    }
}

void App::HandlePlaceholderInput()
{
    if (IsKeyPressed(KEY_ESCAPE)) {
        SetCurrentScreen(AppScreen::kMainMenu, "back");
        return;
    }

    if (IsKeyPressedAny({KEY_LEFT, KEY_A, KEY_RIGHT, KEY_D, KEY_TAB})) {
        placeholder_selected_action_ = placeholder_selected_action_ == PlaceholderAction::kMainMenu
            ? PlaceholderAction::kExit
            : PlaceholderAction::kMainMenu;
        logger_.Debug("placeholder", "selected action=" + std::string(ToString(placeholder_selected_action_)));
    }

    if (layout_dirty_) {
        RebuildLayout();
    }

    const Vector2 mouse = GetMousePosition();
    for (const auto& action_bounds : layout_cache_.placeholder.actions) {
        if (!PointInRect(mouse, action_bounds.bounds)) {
            continue;
        }

        hovered_item_ = "placeholder_" + std::string(ToString(action_bounds.action));
        if (placeholder_selected_action_ != action_bounds.action) {
            placeholder_selected_action_ = action_bounds.action;
            logger_.Debug("placeholder", "selected action=" + std::string(ToString(placeholder_selected_action_)));
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ActivatePlaceholderAction();
            return;
        }
        break;
    }

    if (IsKeyPressed(KEY_ENTER)) {
        ActivatePlaceholderAction();
    }
}

void App::HandleDialogInput()
{
    if (IsKeyPressedAny({KEY_LEFT, KEY_A})) {
        dialog_.selected_choice = DialogChoice::kYes;
        logger_.Debug("dialog", "selected choice=yes");
    }
    if (IsKeyPressedAny({KEY_RIGHT, KEY_D})) {
        dialog_.selected_choice = DialogChoice::kNo;
        logger_.Debug("dialog", "selected choice=no");
    }
    if (IsKeyPressed(KEY_TAB)) {
        dialog_.selected_choice = dialog_.selected_choice == DialogChoice::kYes ? DialogChoice::kNo : DialogChoice::kYes;
        logger_.Debug("dialog", "selected choice=" + std::string(ToString(dialog_.selected_choice)));
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        CancelExitConfirmation();
        return;
    }

    if (layout_dirty_) {
        RebuildLayout();
    }

    const Vector2 mouse = GetMousePosition();
    const DialogButtonBounds& buttons = layout_cache_.exit_dialog.buttons;
    if (PointInRect(mouse, buttons.yes)) {
        hovered_item_ = "dialog_yes";
        if (dialog_.selected_choice != DialogChoice::kYes) {
            dialog_.selected_choice = DialogChoice::kYes;
            logger_.Debug("dialog", "selected choice=yes");
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            AcceptExitConfirmation();
            return;
        }
    } else if (PointInRect(mouse, buttons.no)) {
        hovered_item_ = "dialog_no";
        if (dialog_.selected_choice != DialogChoice::kNo) {
            dialog_.selected_choice = DialogChoice::kNo;
            logger_.Debug("dialog", "selected choice=no");
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            CancelExitConfirmation();
            return;
        }
    }

    if (IsKeyPressed(KEY_ENTER)) {
        if (dialog_.selected_choice == DialogChoice::kYes) {
            AcceptExitConfirmation();
        } else {
            CancelExitConfirmation();
        }
    }
}

void App::Update(float dt)
{
    AdvanceProgressiveChunkBuild(dt);

    process_memory_sample_timer_ += dt;
    if (process_memory_sample_timer_ >= 0.50F) {
        RefreshProcessMemoryInfo();
        process_memory_sample_timer_ = 0.0F;
    }

    UpdateVisibilityStats();

    if (!config_.window_resizable || !IsWindowResized()) {
        return;
    }

    window_config_.window_width = GetScreenWidth();
    window_config_.window_height = GetScreenHeight();
    window_config_.ui_scale = CalculateUiScale(window_config_.window_width, window_config_.window_height, config_);
    layout_dirty_ = true;
    logger_.Debug(
        "window",
        "resized window=" + std::to_string(window_config_.window_width) + "x"
            + std::to_string(window_config_.window_height) + " ui_scale=" + std::to_string(window_config_.ui_scale));
}


void App::AdvanceProgressiveChunkBuild(float dt)
{
    if (!workspace_.progressive_build_enabled || workspace_.progressive_build_complete
        || workspace_.progressive_build_per_frame <= 0 || workspace_.progressive_pending_chunks.empty()
        || !workspace_.chunk_grid.IsValid() || !workspace_.voxel_world.IsValid()) {
        return;
    }

    const ProgressiveBuildPriority priority = BuildProgressivePriority(
        workspace_.runtime_map,
        preview_camera_.Status(),
        workspace_.chunk_size_tiles);
    const int interest_radius_chunks = ResolveProgressiveInterestRadiusChunks();
    const int keep_radius_chunks = std::max(interest_radius_chunks, ResolveProgressiveKeepRadiusChunks());
    const int render_budget = ResolveRenderChunkBudget();
    const int hard_limit = std::max(render_budget, ResolveRenderChunkHardLimit());
    const int evicts_per_frame = ResolveProgressiveEvictsPerFrame();
    const int time_budget_ms = ResolveProgressiveTimeBudgetMs();

    if (workspace_.progressive_budget_wait_timer > 0.0F) {
        workspace_.progressive_budget_wait_timer =
            std::max(0.0F, workspace_.progressive_budget_wait_timer - dt);
        workspace_.progressive_log_timer += dt;
        if (workspace_.progressive_log_timer >= 1.0F) {
            std::ostringstream out;
            out << "state=budget_wait";
            out << " resident=" << workspace_.progressive_chunks_built << '/' << workspace_.progressive_chunks_total;
            out << " pending=" << workspace_.progressive_chunks_pending;
            out << " built_batch=0";
            out << " evicted_batch=0";
            out << " time_budget_ms=" << time_budget_ms;
            out << " cooldown_ms=" << static_cast<int>(std::lround(workspace_.progressive_budget_wait_timer * 1000.0F));
            out << " interest_radius=" << interest_radius_chunks;
            out << " keep_radius=" << keep_radius_chunks;
            out << " budget=" << render_budget;
            out << " hard_limit=" << hard_limit;
            out << " priority=" << (priority.camera_based ? "camera" : "map");
            out << " target=" << static_cast<int>(std::lround(priority.target_x)) << ','
                << static_cast<int>(std::lround(priority.target_y));
            out << " lookahead=" << static_cast<int>(std::lround(priority.lookahead_x)) << ','
                << static_cast<int>(std::lround(priority.lookahead_y));
            out << " models=" << chunk_mesh_preview_.Stats().models;
            logger_.Info("progressive_build", out.str());
            workspace_.progressive_log_timer = 0.0F;
        }
        return;
    }

    int evicted_count = 0;
    const std::vector<std::size_t> evicted_chunks = SelectProgressiveEvictionChunks(
        workspace_.chunk_grid,
        workspace_.progressive_built_chunks,
        priority,
        keep_radius_chunks,
        workspace_.chunk_size_tiles,
        render_budget,
        hard_limit,
        evicts_per_frame);
    if (!evicted_chunks.empty()) {
        std::vector<std::uint8_t> evicted_selection(workspace_.chunk_grid.chunks.size(), 0U);
        std::vector<ChunkCoord> evicted_coords;
        evicted_coords.reserve(evicted_chunks.size());
        for (std::size_t chunk_index : evicted_chunks) {
            if (chunk_index >= evicted_selection.size() || chunk_index >= workspace_.chunk_grid.chunks.size()) {
                continue;
            }
            evicted_selection[chunk_index] = 1U;
            evicted_coords.push_back(workspace_.chunk_grid.chunks[chunk_index].coord);
            workspace_.progressive_built_chunks[chunk_index] = 0U;
            AddPendingChunkIfMissing(workspace_.progressive_pending_chunks, chunk_index);
            ++evicted_count;
        }
        if (evicted_count > 0) {
            ClearSelectedCacheChunks(workspace_.simple_chunk_mesh_cache, evicted_selection);
            ClearSelectedCacheChunks(workspace_.greedy_chunk_mesh_cache, evicted_selection);
            ClearSelectedMeshBuildChunks(workspace_.simple_chunk_meshes, evicted_selection);
            ClearSelectedMeshBuildChunks(workspace_.greedy_chunk_meshes, evicted_selection);
            ClearSelectedMeshBuildChunks(workspace_.terrain_chunk_meshes, evicted_selection);
            const std::size_t unloaded_chunks = chunk_mesh_preview_.UnloadChunks(evicted_coords);
            (void)unloaded_chunks;
            SetActiveMeshCacheFromMode();
        }
    }

    const int resident_count = static_cast<int>(std::count(
        workspace_.progressive_built_chunks.begin(),
        workspace_.progressive_built_chunks.end(),
        static_cast<std::uint8_t>(1)));
    const int build_capacity = std::max(0, hard_limit - resident_count);
    const int batch_size = std::min(
        {workspace_.progressive_build_per_frame,
         static_cast<int>(workspace_.progressive_pending_chunks.size()),
         build_capacity});

    std::vector<std::uint8_t> selected(workspace_.chunk_grid.chunks.size(), 0U);
    int selected_count = 0;
    for (int index = 0; index < batch_size; ++index) {
        std::optional<std::size_t> maybe_chunk_index = PopBestProgressiveChunk(
            workspace_.chunk_grid,
            workspace_.progressive_pending_chunks,
            priority,
            interest_radius_chunks,
            workspace_.chunk_size_tiles);
        if (!maybe_chunk_index.has_value()) {
            break;
        }
        const std::size_t chunk_index = *maybe_chunk_index;
        if (chunk_index >= selected.size()) {
            continue;
        }
        selected[chunk_index] = 1U;
        if (chunk_index < workspace_.progressive_built_chunks.size()) {
            workspace_.progressive_built_chunks[chunk_index] = 1U;
        }
        ++selected_count;
    }

    const SteadyTimePoint batch_start = Now();
    bool uploaded = true;
    if (selected_count > 0) {
        FaceVisibilityResult batch_visibility = BuildFaceVisibilityForSelectedChunks(
            workspace_.voxel_world,
            workspace_.chunk_grid,
            selected);
        ChunkMeshCache batch_simple = BuildChunkMeshCacheForSelectedChunks(
            workspace_.voxel_world,
            workspace_.chunk_grid,
            ChunkMeshBuildMode::kSimpleFaces,
            selected);
        ChunkMeshCache batch_greedy = BuildChunkMeshCacheForSelectedChunks(
            workspace_.voxel_world,
            workspace_.chunk_grid,
            ChunkMeshBuildMode::kGreedyFaces,
            selected);
        ChunkMeshBuildResult batch_terrain = BuildTerrainChunkMeshesForSelectedChunks(
            workspace_.runtime_map,
            workspace_.chunk_grid,
            selected);

        MergeFaceVisibility(workspace_.face_visibility, batch_visibility);
        MergeSelectedCacheChunks(workspace_.simple_chunk_mesh_cache, batch_simple, selected);
        MergeSelectedCacheChunks(workspace_.greedy_chunk_mesh_cache, batch_greedy, selected);
        MergeSelectedMeshBuildChunks(workspace_.terrain_chunk_meshes, batch_terrain, selected);
        workspace_.simple_chunk_meshes = ToChunkMeshBuildResult(workspace_.simple_chunk_mesh_cache);
        workspace_.greedy_chunk_meshes = ToChunkMeshBuildResult(workspace_.greedy_chunk_mesh_cache);
        SetActiveMeshCacheFromMode();

        ChunkMeshBuildResult active_batch;
        if (workspace_.mesh_mode == ChunkMeshBuildMode::kTerrainSurface) {
            active_batch = std::move(batch_terrain);
        } else if (workspace_.mesh_mode == ChunkMeshBuildMode::kGreedyFaces) {
            active_batch = ToChunkMeshBuildResult(batch_greedy);
        } else {
            active_batch = ToChunkMeshBuildResult(batch_simple);
        }
        uploaded = chunk_mesh_preview_.UploadAdditional(active_batch, ToRaylibColorMode(workspace_.color_mode));
        if (!uploaded) {
            logger_.Warn("progressive_build", "batch produced no uploaded chunks");
        }
    } else {
        SetActiveMeshCacheFromMode();
    }

    RefreshMeshOptimizationStats();
    UpdateVisibilityStats();

    workspace_.progressive_chunks_built = static_cast<std::uint64_t>(std::count(
        workspace_.progressive_built_chunks.begin(),
        workspace_.progressive_built_chunks.end(),
        static_cast<std::uint8_t>(1)));
    workspace_.progressive_chunks_pending = static_cast<std::uint64_t>(workspace_.progressive_pending_chunks.size());
    const SteadyTimePoint batch_finish = Now();
    const long long batch_ms = ElapsedMs(batch_start, batch_finish);
    if (selected_count > 0 && time_budget_ms > 0 && batch_ms > time_budget_ms) {
        const float over_budget_seconds = static_cast<float>(batch_ms - time_budget_ms) / 1000.0F;
        workspace_.progressive_budget_wait_timer = std::clamp(over_budget_seconds, 0.0F, 0.25F);
    }
    workspace_.progressive_log_timer += dt;

    const bool idle = selected_count == 0 && evicted_count == 0;
    if (workspace_.progressive_log_timer >= 1.0F || workspace_.progressive_pending_chunks.empty()) {
        std::ostringstream out;
        out << "state=" << (idle ? "idle" : "active");
        out << " resident=" << workspace_.progressive_chunks_built << '/' << workspace_.progressive_chunks_total;
        out << " pending=" << workspace_.progressive_chunks_pending;
        out << " built_batch=" << selected_count;
        out << " evicted_batch=" << evicted_count;
        out << " batch_ms=" << batch_ms;
        out << " time_budget_ms=" << time_budget_ms;
        out << " cooldown_ms=" << static_cast<int>(std::lround(workspace_.progressive_budget_wait_timer * 1000.0F));
        out << " interest_radius=" << interest_radius_chunks;
        out << " keep_radius=" << keep_radius_chunks;
        out << " budget=" << render_budget;
        out << " hard_limit=" << hard_limit;
        out << " priority=" << (priority.camera_based ? "camera" : "map");
        out << " target=" << static_cast<int>(std::lround(priority.target_x)) << ','
            << static_cast<int>(std::lround(priority.target_y));
        out << " lookahead=" << static_cast<int>(std::lround(priority.lookahead_x)) << ','
            << static_cast<int>(std::lround(priority.lookahead_y));
        out << " models=" << chunk_mesh_preview_.Stats().models;
        logger_.Info("progressive_build", out.str());
        workspace_.progressive_log_timer = 0.0F;
    }

    if (workspace_.progressive_pending_chunks.empty()) {
        workspace_.progressive_build_complete = true;
        workspace_.progressive_build_enabled = false;
        logger_.Info(
            "progressive_build",
            "completed resident=" + std::to_string(workspace_.progressive_chunks_built) + '/'
                + std::to_string(workspace_.progressive_chunks_total));
    }

    layout_dirty_ = true;
}

void App::Draw()
{
    if (layout_dirty_) {
        RebuildLayout();
    }

    BeginDrawing();
    ClearBackground(Color{18, 20, 28, 255});

    switch (screen_) {
        case AppScreen::kMainMenu:
            DrawMainMenu(main_menu_.State(), UiFonts(), labels_, layout_cache_);
            break;
        case AppScreen::kWorkspace:
            DrawWorkspace(
                workspace_,
                &chunk_mesh_preview_,
                &preview_camera_.Camera(),
                preview_camera_.Status(),
                UiFonts(),
                labels_,
                layout_cache_);
            break;
        case AppScreen::kSettingsPlaceholder:
            DrawPlaceholderScreen(labels_.placeholder_settings_title, placeholder_selected_action_, UiFonts(), labels_, layout_cache_);
            break;
    }

    DrawFpsCounter(UiFonts(), labels_, layout_cache_, process_memory_, config_.version);
    if (config_.debug_ui) {
        DrawDebugOverlay(UiFonts(), config_, window_config_, screen_, dialog_.type, main_menu_.State(), workspace_, hovered_item_, labels_, layout_cache_);
    }
    if (dialog_.type == ModalDialog::kExitConfirmation) {
        DrawExitDialog(dialog_, UiFonts(), labels_, layout_cache_);
    }

    EndDrawing();
}

void App::RebuildLayout()
{
    layout_cache_ = RebuildUiLayout(
        main_menu_.State(),
        UiFonts(),
        window_config_,
        config_,
        labels_,
        workspace_,
        preview_camera_.Status());
    layout_dirty_ = false;
}

void App::RefreshMeshOptimizationStats()
{
    workspace_.mesh_stats = MeshOptimizationStats{};
    workspace_.mesh_stats.active_mode = workspace_.mesh_mode;

    if (workspace_.face_visibility.IsValid()) {
        workspace_.mesh_stats.solid_blocks = workspace_.face_visibility.info.solid_blocks;
        workspace_.mesh_stats.naive_faces = workspace_.face_visibility.info.naive_faces;
        workspace_.mesh_stats.culled_faces = workspace_.face_visibility.info.culled_faces;
    }
    if (workspace_.simple_chunk_meshes.IsValid()) {
        workspace_.mesh_stats.simple_faces = workspace_.simple_chunk_meshes.info.visible_faces;
    }
    if (workspace_.greedy_chunk_meshes.IsValid()) {
        workspace_.mesh_stats.greedy_faces = workspace_.greedy_chunk_meshes.info.visible_faces;
    }
    if (workspace_.terrain_chunk_meshes.IsValid()) {
        workspace_.mesh_stats.terrain_faces = workspace_.terrain_chunk_meshes.info.visible_faces;
        workspace_.mesh_stats.terrain_raw_top_faces = workspace_.terrain_chunk_meshes.info.terrain_raw_top_faces;
        workspace_.mesh_stats.terrain_raw_wall_faces = workspace_.terrain_chunk_meshes.info.terrain_raw_wall_faces;
        workspace_.mesh_stats.terrain_top_faces = workspace_.terrain_chunk_meshes.info.terrain_top_faces;
        workspace_.mesh_stats.terrain_wall_faces = workspace_.terrain_chunk_meshes.info.terrain_wall_faces;
        workspace_.mesh_stats.terrain_cliff_faces = workspace_.terrain_chunk_meshes.info.terrain_cliff_faces;
    }
    if (workspace_.chunk_meshes.IsValid()) {
        workspace_.mesh_stats.active_faces = workspace_.chunk_meshes.info.visible_faces;
        workspace_.mesh_stats.active_vertices = workspace_.chunk_meshes.info.vertices;
        workspace_.mesh_stats.active_indices = workspace_.chunk_meshes.info.indices;
        workspace_.mesh_stats.mesh_chunks = workspace_.chunk_meshes.info.non_empty_chunks;
    }

    const RaylibChunkMeshPreviewStats& upload = chunk_mesh_preview_.Stats();
    workspace_.mesh_stats.draw_models = upload.models;
    workspace_.mesh_stats.skipped_chunks = upload.skipped_chunks;
}

void App::RefreshChunkSizeComparison(
    int before_chunk_size,
    const ChunkGridInfo& before_grid_info,
    const MeshOptimizationStats& before_stats,
    bool had_before_stats)
{
    workspace_.chunk_size_comparison = WorkspaceChunkSizeComparison{};
    if (!had_before_stats || !workspace_.chunk_grid.IsValid() || !workspace_.chunk_meshes.IsValid()) {
        return;
    }

    workspace_.chunk_size_comparison.available = true;
    workspace_.chunk_size_comparison.before_chunk_size = before_chunk_size;
    workspace_.chunk_size_comparison.after_chunk_size = workspace_.chunk_size_tiles;
    workspace_.chunk_size_comparison.before_total_chunks = before_grid_info.total_chunks;
    workspace_.chunk_size_comparison.after_total_chunks = workspace_.chunk_grid.info.total_chunks;
    workspace_.chunk_size_comparison.before_draw_models = before_stats.draw_models;
    workspace_.chunk_size_comparison.after_draw_models = workspace_.mesh_stats.draw_models;
    workspace_.chunk_size_comparison.before_active_faces = before_stats.active_faces;
    workspace_.chunk_size_comparison.after_active_faces = workspace_.mesh_stats.active_faces;
}

void App::RebuildChunkPipeline(int chunk_size, std::string_view reason)
{
    const SteadyTimePoint pipeline_start = Now();

    if (!IsSupportedChunkSize(chunk_size)) {
        logger_.Warn("chunk_pipeline", "unsupported chunk size=" + std::to_string(chunk_size));
        return;
    }

    const bool had_before_stats = workspace_.chunk_grid.IsValid() && workspace_.chunk_meshes.IsValid();
    const int before_chunk_size = workspace_.chunk_size_tiles;
    const ChunkGridInfo before_grid_info = workspace_.chunk_grid.info;
    const MeshOptimizationStats before_stats = workspace_.mesh_stats;

    const SteadyTimePoint chunk_grid_start = Now();
    ChunkGrid next_chunk_grid = BuildChunkGrid(workspace_.runtime_map, ChunkGridOptions{chunk_size, chunk_size});
    const SteadyTimePoint chunk_grid_finish = Now();
    logger_.Info("chunk_grid", "reason=" + std::string(reason) + " " + ToLogString(next_chunk_grid));
    for (const auto& warning : next_chunk_grid.diagnostics.warnings) {
        logger_.Warn("chunk_grid", warning);
    }

    const int initial_tile_window_size = ResolveInitialTileWindowSize(workspace_.runtime_map);
    const InitialTileWindow initial_tile_window = ResolveInitialTileWindow(
        workspace_.runtime_map,
        initial_tile_window_size);
    int initial_selected_chunks = 0;
    const std::vector<std::uint8_t> initial_chunk_selection = BuildInitialChunkSelection(
        next_chunk_grid,
        initial_tile_window,
        &initial_selected_chunks);
    const bool partial_initial_mesh = initial_tile_window.limited
        && initial_selected_chunks < static_cast<int>(next_chunk_grid.chunks.size());

    const SteadyTimePoint voxel_world_start = Now();
    VoxelWorld next_voxel_world = BuildVoxelWorld(workspace_.runtime_map, next_chunk_grid);
    const SteadyTimePoint voxel_world_finish = Now();
    logger_.Info("voxel_world", "reason=" + std::string(reason) + " " + ToLogString(next_voxel_world));
    for (const auto& warning : next_voxel_world.diagnostics.warnings) {
        logger_.Warn("voxel_world", warning);
    }

    if (partial_initial_mesh) {
        std::ostringstream out;
        out << "reason=" << reason;
        out << " mode=initial_tile_window";
        out << " window=" << initial_tile_window.left << ',' << initial_tile_window.top << ','
            << initial_tile_window.width << 'x' << initial_tile_window.height;
        out << " initial_chunks=" << initial_selected_chunks;
        out << " skipped_chunks=" << (next_chunk_grid.info.total_chunks - initial_selected_chunks);
        out << " total_chunks=" << next_chunk_grid.info.total_chunks;
        logger_.Info("chunk_pipeline", out.str());
    }

    const SteadyTimePoint face_visibility_start = Now();
    FaceVisibilityResult next_face_visibility = partial_initial_mesh
        ? BuildFaceVisibilityForSelectedChunks(next_voxel_world, next_chunk_grid, initial_chunk_selection)
        : BuildFaceVisibility(next_voxel_world);
    const SteadyTimePoint face_visibility_finish = Now();
    logger_.Info("face_visibility", "reason=" + std::string(reason) + " " + ToLogString(next_face_visibility));
    for (const auto& warning : next_face_visibility.diagnostics.warnings) {
        logger_.Warn("face_visibility", warning);
    }

    const SteadyTimePoint mesh_simple_start = Now();
    ChunkMeshCache next_simple_cache = partial_initial_mesh
        ? BuildChunkMeshCacheForSelectedChunks(
            next_voxel_world,
            next_chunk_grid,
            ChunkMeshBuildMode::kSimpleFaces,
            initial_chunk_selection)
        : BuildChunkMeshCache(
            next_voxel_world,
            next_chunk_grid,
            ChunkMeshBuildMode::kSimpleFaces);
    const SteadyTimePoint mesh_simple_finish = Now();
    logger_.Info("chunk_mesh_cache", "reason=" + std::string(reason) + " " + ToLogString(next_simple_cache));
    for (const auto& warning : next_simple_cache.diagnostics.warnings) {
        logger_.Warn("chunk_mesh_cache", warning);
    }

    const SteadyTimePoint mesh_greedy_start = Now();
    ChunkMeshCache next_greedy_cache = partial_initial_mesh
        ? BuildChunkMeshCacheForSelectedChunks(
            next_voxel_world,
            next_chunk_grid,
            ChunkMeshBuildMode::kGreedyFaces,
            initial_chunk_selection)
        : BuildChunkMeshCache(
            next_voxel_world,
            next_chunk_grid,
            ChunkMeshBuildMode::kGreedyFaces);
    const SteadyTimePoint mesh_greedy_finish = Now();
    logger_.Info("chunk_mesh_cache", "reason=" + std::string(reason) + " " + ToLogString(next_greedy_cache));
    for (const auto& warning : next_greedy_cache.diagnostics.warnings) {
        logger_.Warn("chunk_mesh_cache", warning);
    }

    const SteadyTimePoint terrain_mesh_start = Now();
    ChunkMeshBuildResult next_terrain_meshes = partial_initial_mesh
        ? BuildTerrainChunkMeshesForSelectedChunks(workspace_.runtime_map, next_chunk_grid, initial_chunk_selection)
        : BuildTerrainChunkMeshes(workspace_.runtime_map, next_chunk_grid);
    const SteadyTimePoint terrain_mesh_finish = Now();
    logger_.Info("terrain_mesh", "reason=" + std::string(reason) + " " + ToLogString(next_terrain_meshes));
    for (const auto& warning : next_terrain_meshes.diagnostics.warnings) {
        logger_.Warn("terrain_mesh", warning);
    }

    const SteadyTimePoint transitions_start = Now();
    TransitionFeatureSet next_transition_features = BuildTransitionFeatures(workspace_.runtime_map);
    const SteadyTimePoint transitions_finish = Now();
    logger_.Info("transitions", "reason=" + std::string(reason) + " " + ToLogString(next_transition_features));
    for (const auto& warning : next_transition_features.diagnostics.warnings) {
        logger_.Warn("transitions", warning);
    }

    workspace_.chunk_size_tiles = chunk_size;
    workspace_.chunk_grid = std::move(next_chunk_grid);
    workspace_.voxel_world = std::move(next_voxel_world);
    workspace_.face_visibility = std::move(next_face_visibility);
    workspace_.simple_chunk_mesh_cache = std::move(next_simple_cache);
    workspace_.greedy_chunk_mesh_cache = std::move(next_greedy_cache);
    workspace_.terrain_chunk_meshes = std::move(next_terrain_meshes);
    workspace_.transition_features = std::move(next_transition_features);

    workspace_.progressive_build_enabled = false;
    workspace_.progressive_build_complete = true;
    workspace_.progressive_build_per_frame = 0;
    workspace_.progressive_chunks_total = static_cast<std::uint64_t>(workspace_.chunk_grid.info.total_chunks);
    workspace_.progressive_chunks_built = static_cast<std::uint64_t>(initial_selected_chunks);
    workspace_.progressive_chunks_pending = 0;
    workspace_.progressive_log_timer = 0.0F;
    workspace_.progressive_pending_chunks.clear();
    workspace_.progressive_built_chunks = initial_chunk_selection;
    if (partial_initial_mesh) {
        workspace_.progressive_build_per_frame = ResolveProgressiveBuildsPerFrame(partial_initial_mesh);
        workspace_.progressive_pending_chunks = BuildProgressivePendingChunks(
            workspace_.chunk_grid,
            initial_chunk_selection,
            initial_tile_window);
        workspace_.progressive_chunks_pending = static_cast<std::uint64_t>(workspace_.progressive_pending_chunks.size());
        workspace_.progressive_build_enabled = workspace_.progressive_build_per_frame > 0
            && !workspace_.progressive_pending_chunks.empty();
        workspace_.progressive_build_complete = !workspace_.progressive_build_enabled;
    }
    if (workspace_.progressive_build_enabled) {
        std::ostringstream out;
        out << "started built=" << workspace_.progressive_chunks_built << '/' << workspace_.progressive_chunks_total;
        out << " pending=" << workspace_.progressive_chunks_pending;
        out << " chunks_per_frame=" << workspace_.progressive_build_per_frame;
        out << " priority=camera_target";
        logger_.Info("progressive_build", out.str());
    }

    if (workspace_.validation_mode == WorkspaceValidationMode::kOff) {
        ClearPassabilityValidation("chunk_pipeline_disabled");
    } else if (workspace_.passability_validation_dirty) {
        if (workspace_.validation_mode == WorkspaceValidationMode::kOnLoad) {
            RunPassabilityValidation(reason);
        } else {
            workspace_.passability_validation = PassabilityValidationReport{};
            workspace_.passability_validation_status = WorkspaceValidationStatus::kNotRun;
            workspace_.passability_validation_last_run_ms = 0.0;
            workspace_.show_passability_issues = false;
            logger_.Info(
                "passability",
                "status=not_run mode=" + std::string(ToString(workspace_.validation_mode))
                    + " reason=" + std::string(reason));
        }
    }
    if (workspace_.selected_tile.IsValid()) {
        workspace_.selected_tile = InspectTile(
            workspace_.runtime_map,
            workspace_.chunk_grid,
            workspace_.transition_features,
            workspace_.selected_tile.tile);
        workspace_.movement_probe = BuildMovementProbe(
            workspace_.runtime_map,
            workspace_.transition_features,
            workspace_.selected_tile.tile);
    } else {
        workspace_.movement_probe = MovementProbeResult{};
    }
    workspace_.simple_chunk_meshes = ToChunkMeshBuildResult(workspace_.simple_chunk_mesh_cache);
    workspace_.greedy_chunk_meshes = ToChunkMeshBuildResult(workspace_.greedy_chunk_mesh_cache);
    SetActiveMeshCacheFromMode();
    workspace_.last_mesh_rebuild = workspace_.mesh_mode == ChunkMeshBuildMode::kTerrainSurface
        ? ChunkMeshRebuildReport{}
        : FullCacheBuildReport(workspace_.chunk_mesh_cache);

    const SteadyTimePoint render_upload_start = Now();
    UploadActiveChunkMesh(reason);
    const SteadyTimePoint render_upload_finish = Now();

    {
        const SteadyTimePoint pipeline_finish = Now();
        std::ostringstream out;
        out << "reason=" << reason;
        out << " chunk_grid_ms=" << ElapsedMs(chunk_grid_start, chunk_grid_finish);
        out << " voxel_world_ms=" << ElapsedMs(voxel_world_start, voxel_world_finish);
        out << " face_visibility_ms=" << ElapsedMs(face_visibility_start, face_visibility_finish);
        out << " mesh_simple_ms=" << ElapsedMs(mesh_simple_start, mesh_simple_finish);
        out << " mesh_greedy_ms=" << ElapsedMs(mesh_greedy_start, mesh_greedy_finish);
        out << " terrain_mesh_ms=" << ElapsedMs(terrain_mesh_start, terrain_mesh_finish);
        out << " transitions_ms=" << ElapsedMs(transitions_start, transitions_finish);
        out << " render_upload_ms=" << ElapsedMs(render_upload_start, render_upload_finish);
        out << " initial_area=" << (partial_initial_mesh ? "window" : "full");
        if (partial_initial_mesh) {
            out << " initial_window=" << initial_tile_window.left << ',' << initial_tile_window.top << ','
                << initial_tile_window.width << 'x' << initial_tile_window.height;
            out << " initial_chunks=" << initial_selected_chunks;
            out << " skipped_chunks=" << (next_chunk_grid.info.total_chunks - initial_selected_chunks);
        }
        out << " total_ms=" << ElapsedMs(pipeline_start, pipeline_finish);
        logger_.Info("chunk_pipeline_profile", out.str());
    }

    RefreshChunkSizeComparison(before_chunk_size, before_grid_info, before_stats, had_before_stats);
    if (workspace_.chunk_size_comparison.available) {
        logger_.Info("chunk_profit", ChunkComparisonLogString(workspace_.chunk_size_comparison));
    }

    layout_dirty_ = true;
}

void App::SetChunkSize(int chunk_size, std::string_view reason)
{
    if (!IsSupportedChunkSize(chunk_size)) {
        logger_.Warn("chunk_pipeline", "chunk size switch ignored unsupported size=" + std::to_string(chunk_size));
        return;
    }
    if (workspace_.chunk_size_tiles == chunk_size && workspace_.chunk_grid.IsValid()) {
        logger_.Debug("chunk_pipeline", "chunk size unchanged size=" + std::to_string(chunk_size));
        return;
    }

    RebuildChunkPipeline(chunk_size, reason);
}

void App::SetActiveMeshCacheFromMode()
{
    if (workspace_.mesh_mode == ChunkMeshBuildMode::kTerrainSurface) {
        workspace_.chunk_mesh_cache = ChunkMeshCache{};
        workspace_.chunk_meshes = workspace_.terrain_chunk_meshes;
    } else if (workspace_.mesh_mode == ChunkMeshBuildMode::kGreedyFaces) {
        workspace_.chunk_mesh_cache = workspace_.greedy_chunk_mesh_cache;
        workspace_.chunk_meshes = workspace_.greedy_chunk_meshes;
    } else {
        workspace_.chunk_mesh_cache = workspace_.simple_chunk_mesh_cache;
        workspace_.chunk_meshes = workspace_.simple_chunk_meshes;
    }
}

void App::RunDirtyRebuildProbe(std::string_view reason)
{
    if (!workspace_.runtime_map.IsValid() || !workspace_.chunk_grid.IsValid()) {
        logger_.Warn("dirty_rebuild", "probe ignored because runtime map or chunk grid is invalid");
        return;
    }
    if (workspace_.mesh_mode == ChunkMeshBuildMode::kTerrainSurface) {
        logger_.Warn("dirty_rebuild", "probe ignored because terrain surface meshes do not use voxel chunk cache yet");
        return;
    }

    ChunkMeshCache* active_cache = workspace_.mesh_mode == ChunkMeshBuildMode::kGreedyFaces
        ? &workspace_.greedy_chunk_mesh_cache
        : &workspace_.simple_chunk_mesh_cache;
    if (!active_cache->IsValid()) {
        logger_.Warn("dirty_rebuild", "probe ignored because active cache is invalid");
        return;
    }

    const TileCoord tile = CenterTile(workspace_.runtime_map);
    const std::uint64_t marked = active_cache->MarkTileAndBorderChunksDirty(tile, workspace_.chunk_grid);
    workspace_.last_mesh_rebuild = RebuildDirtyChunkMeshes(workspace_.voxel_world, workspace_.chunk_grid, active_cache);

    if (workspace_.mesh_mode == ChunkMeshBuildMode::kGreedyFaces) {
        workspace_.greedy_chunk_meshes = ToChunkMeshBuildResult(workspace_.greedy_chunk_mesh_cache);
    } else {
        workspace_.simple_chunk_meshes = ToChunkMeshBuildResult(workspace_.simple_chunk_mesh_cache);
    }
    SetActiveMeshCacheFromMode();
    UploadActiveChunkMesh(reason);

    std::ostringstream out;
    out << "reason=" << reason;
    out << " probe_tile=" << tile.x << ',' << tile.y;
    out << " newly_marked=" << marked;
    out << ' ' << ToLogString(workspace_.last_mesh_rebuild);
    logger_.Info("dirty_rebuild", out.str());
    for (const auto& warning : workspace_.last_mesh_rebuild.diagnostics.warnings) {
        logger_.Warn("dirty_rebuild", warning);
    }

    layout_dirty_ = true;
}

void App::UploadActiveChunkMesh(std::string_view reason)
{
    const bool uploaded = chunk_mesh_preview_.Upload(workspace_.chunk_meshes, ToRaylibColorMode(workspace_.color_mode));
    RefreshMeshOptimizationStats();
    UpdateVisibilityStats();
    logger_.Info(
        "render3d",
        "upload reason=" + std::string(reason) + " color=" + std::string(ToString(workspace_.color_mode)) + " "
            + ToLogString(chunk_mesh_preview_.Stats()));
    logger_.Info("mesh_stats", ToLogString(workspace_.mesh_stats));
    if (!uploaded) {
        logger_.Warn("render3d", "3D preview mesh upload failed or produced no drawable chunks");
    }
}

void App::SetMeshBuildMode(ChunkMeshBuildMode mode, std::string_view reason)
{
    if (workspace_.mesh_mode == mode && workspace_.chunk_meshes.IsValid()) {
        logger_.Debug("mesh_stats", "mesh mode unchanged mode=" + std::string(ToString(mode)));
        return;
    }

    const ChunkMeshBuildResult& selected = mode == ChunkMeshBuildMode::kTerrainSurface
        ? workspace_.terrain_chunk_meshes
        : (mode == ChunkMeshBuildMode::kGreedyFaces ? workspace_.greedy_chunk_meshes : workspace_.simple_chunk_meshes);
    if (!selected.IsValid()) {
        logger_.Warn("mesh_stats", "mesh mode switch ignored invalid mode=" + std::string(ToString(mode)));
        return;
    }

    workspace_.mesh_mode = mode;
    SetActiveMeshCacheFromMode();
    UploadActiveChunkMesh(reason);
    layout_dirty_ = true;
}

void App::SetColorMode(WorkspaceColorMode mode, std::string_view reason)
{
    if (workspace_.color_mode == mode && chunk_mesh_preview_.IsUploaded()) {
        logger_.Debug("render3d", "color mode unchanged mode=" + std::string(ToString(mode)));
        return;
    }

    workspace_.color_mode = mode;
    if (workspace_.chunk_meshes.IsValid()) {
        UploadActiveChunkMesh(reason);
    }
    layout_dirty_ = true;
    logger_.Info("render3d", "color mode=" + std::string(ToString(mode)) + " reason=" + std::string(reason));
}

void App::CycleColorMode(std::string_view reason)
{
    SetColorMode(NextColorMode(workspace_.color_mode), reason);
}

void App::SetVisibilityMode(WorkspaceVisibilityMode mode, std::string_view reason)
{
    if (workspace_.visibility_mode == mode) {
        logger_.Debug("visibility", "mode unchanged mode=" + std::string(ToString(mode)));
        return;
    }

    workspace_.visibility_mode = mode;
    UpdateVisibilityStats();
    layout_dirty_ = true;
    logger_.Info("visibility", "reason=" + std::string(reason) + " " + ToLogString(chunk_mesh_preview_.CalculateVisibilityStats(
        workspace_.chunk_meshes,
        preview_camera_.Camera(),
        BuildRaylibVisibilityOptions(workspace_, layout_cache_.workspace.map_overview),
        BuildRaylibTerrainPassOptions(workspace_))));
}

void App::CycleVisibilityMode(std::string_view reason)
{
    SetVisibilityMode(NextVisibilityMode(workspace_.visibility_mode), reason);
}

void App::AdjustVisibilityRadius(int delta, std::string_view reason)
{
    constexpr int kMinRadius = 0;
    constexpr int kMaxRadius = 12;
    const int next_radius = std::clamp(workspace_.visibility_radius_chunks + delta, kMinRadius, kMaxRadius);
    if (next_radius == workspace_.visibility_radius_chunks) {
        logger_.Debug("visibility", "radius unchanged radius=" + std::to_string(workspace_.visibility_radius_chunks));
        return;
    }

    workspace_.visibility_radius_chunks = next_radius;
    UpdateVisibilityStats();
    layout_dirty_ = true;
    logger_.Info("visibility", "reason=" + std::string(reason) + " radius=" + std::to_string(next_radius) + " "
        + ToLogString(chunk_mesh_preview_.CalculateVisibilityStats(
            workspace_.chunk_meshes,
            preview_camera_.Camera(),
            BuildRaylibVisibilityOptions(workspace_, layout_cache_.workspace.map_overview),
        BuildRaylibTerrainPassOptions(workspace_))));
}

void App::AdjustVisibilityFadeRing(int delta, std::string_view reason)
{
    constexpr int kMinFadeRing = 0;
    constexpr int kMaxFadeRing = 4;
    const int next_fade_ring = std::clamp(workspace_.visibility_fade_ring_chunks + delta, kMinFadeRing, kMaxFadeRing);
    if (next_fade_ring == workspace_.visibility_fade_ring_chunks) {
        logger_.Debug("visibility", "fade ring unchanged fade_ring=" + std::to_string(workspace_.visibility_fade_ring_chunks));
        return;
    }

    workspace_.visibility_fade_ring_chunks = next_fade_ring;
    UpdateVisibilityStats();
    layout_dirty_ = true;
    logger_.Info("visibility", "reason=" + std::string(reason) + " fade_ring=" + std::to_string(next_fade_ring) + " "
        + ToLogString(chunk_mesh_preview_.CalculateVisibilityStats(
            workspace_.chunk_meshes,
            preview_camera_.Camera(),
            BuildRaylibVisibilityOptions(workspace_, layout_cache_.workspace.map_overview),
        BuildRaylibTerrainPassOptions(workspace_))));
}

void App::UpdateVisibilityStats()
{
    if (!chunk_mesh_preview_.IsUploaded() || !workspace_.chunk_meshes.IsValid()) {
        workspace_.visibility_stats = WorkspaceVisibilityStats{};
        workspace_.visibility_stats.mode = workspace_.visibility_mode;
        workspace_.visibility_stats.radius_chunks = workspace_.visibility_radius_chunks;
        workspace_.visibility_stats.fade_ring_chunks = workspace_.visibility_fade_ring_chunks;
        return;
    }

    workspace_.visibility_stats = ToWorkspaceVisibilityStats(chunk_mesh_preview_.CalculateVisibilityStats(
        workspace_.chunk_meshes,
        preview_camera_.Camera(),
        BuildRaylibVisibilityOptions(workspace_, layout_cache_.workspace.map_overview),
        BuildRaylibTerrainPassOptions(workspace_)));
}

void App::ActivateSelectedMenuItem()
{
    const MenuItem* item = main_menu_.SelectedItem();
    if (item == nullptr) {
        logger_.Debug("menu", "activation ignored because no enabled item is selected");
        return;
    }

    logger_.Info(
        "menu",
        "item activated id=" + std::string(ToString(item->id)) + " title=\"" + item->title + "\"");

    switch (item->id) {
        case MenuItemId::kNewGame:
            SetCurrentScreen(AppScreen::kWorkspace, "menu_workspace");
            break;
        case MenuItemId::kLoadGame:
            if (workspace_.map.loaded) {
                SetCurrentScreen(AppScreen::kWorkspace, "menu_load_map");
            } else {
                logger_.Debug("menu", "disabled item activation ignored id=load_game");
            }
            break;
        case MenuItemId::kSettings:
            SetCurrentScreen(AppScreen::kSettingsPlaceholder, "menu_settings");
            break;
        case MenuItemId::kExit:
            RequestExitConfirmation();
            break;
    }
}

void App::ActivatePlaceholderAction()
{
    logger_.Info("placeholder", "action activated id=" + std::string(ToString(placeholder_selected_action_)));
    switch (placeholder_selected_action_) {
        case PlaceholderAction::kMainMenu:
            SetCurrentScreen(AppScreen::kMainMenu, "placeholder_action");
            break;
        case PlaceholderAction::kExit:
            RequestExitConfirmation();
            break;
    }
}


bool App::SetWorkspacePanelTab(WorkspacePanelTab tab, std::string_view reason)
{
    if (workspace_.selected_panel_tab == tab) {
        return false;
    }

    workspace_.selected_panel_tab = tab;
    layout_dirty_ = true;
    logger_.Debug(
        "workspace",
        "panel tab=" + std::string(ToString(workspace_.selected_panel_tab))
            + " reason=" + std::string(reason));
    return true;
}

void App::SelectPreviousWorkspaceTool()
{
    switch (workspace_.selected_panel_tab) {
        case WorkspacePanelTab::kMenu:
            workspace_.selected_panel_tab = WorkspacePanelTab::kInspect;
            break;
        case WorkspacePanelTab::kStats:
            workspace_.selected_panel_tab = WorkspacePanelTab::kMenu;
            break;
        case WorkspacePanelTab::kInspect:
            workspace_.selected_panel_tab = WorkspacePanelTab::kStats;
            break;
        case WorkspacePanelTab::kHelp:
            workspace_.selected_panel_tab = WorkspacePanelTab::kInspect;
            break;
    }
    layout_dirty_ = true;
    logger_.Debug("workspace", "panel tab=" + std::string(ToString(workspace_.selected_panel_tab)));
}

void App::SelectNextWorkspaceTool()
{
    switch (workspace_.selected_panel_tab) {
        case WorkspacePanelTab::kMenu:
            workspace_.selected_panel_tab = WorkspacePanelTab::kStats;
            break;
        case WorkspacePanelTab::kStats:
            workspace_.selected_panel_tab = WorkspacePanelTab::kInspect;
            break;
        case WorkspacePanelTab::kInspect:
            workspace_.selected_panel_tab = WorkspacePanelTab::kMenu;
            break;
        case WorkspacePanelTab::kHelp:
            workspace_.selected_panel_tab = WorkspacePanelTab::kMenu;
            break;
    }
    layout_dirty_ = true;
    logger_.Debug("workspace", "panel tab=" + std::string(ToString(workspace_.selected_panel_tab)));
}

void App::ToggleWorkspaceTool(WorkspaceTool tool)
{
    if (workspace_.selected_tool == tool) {
        workspace_.selected_tool_expanded = !workspace_.selected_tool_expanded;
    } else {
        workspace_.selected_tool = tool;
        workspace_.selected_tool_expanded = true;
    }
    layout_dirty_ = true;
    logger_.Debug(
        "workspace",
        "tool clicked tool=" + std::string(ToString(workspace_.selected_tool))
            + " expanded=" + std::string(workspace_.selected_tool_expanded ? "true" : "false"));
}

void App::ToggleTransitionOverlay(std::string_view reason)
{
    if (!workspace_.transition_features.IsValid() || workspace_.transition_features.features.empty()) {
        logger_.Debug("transitions", "overlay toggle ignored because no transition features are available");
        return;
    }

    workspace_.show_transition_overlay = !workspace_.show_transition_overlay;
    layout_dirty_ = true;
    logger_.Info("transitions", std::string("overlay=")
        + (workspace_.show_transition_overlay ? "on" : "off")
        + " reason=" + std::string(reason));
}

void App::ToggleMovementProbeOverlay(std::string_view reason)
{
    if (!workspace_.selected_tile.IsValid() || !workspace_.movement_probe.IsValid()) {
        logger_.Debug("movement", "probe overlay toggle ignored because no tile is selected");
        return;
    }

    workspace_.show_movement_probe = !workspace_.show_movement_probe;
    layout_dirty_ = true;
    logger_.Info("movement", std::string("probe_overlay=")
        + (workspace_.show_movement_probe ? "on" : "off")
        + " reason=" + std::string(reason));
}

void App::SetPathProfile(PathProfile profile, std::string_view reason)
{
    if (workspace_.path_profile == profile) {
        logger_.Debug("path", "profile unchanged profile=" + std::string(ToString(profile)));
        return;
    }

    workspace_.path_profile = profile;
    if (workspace_.path_probe.IsValid()) {
        workspace_.path_probe = PathProbeResult{};
    }
    layout_dirty_ = true;
    logger_.Info("path", "profile=" + std::string(ToString(profile)) + " reason=" + std::string(reason));
}

void App::BeginPathPickMode(std::string_view reason)
{
    preview_camera_.ReleaseMouse();
    workspace_.has_path_start = false;
    workspace_.has_path_goal = false;
    workspace_.path_start = TileCoord{};
    workspace_.path_goal = TileCoord{};
    workspace_.path_probe = PathProbeResult{};
    layout_dirty_ = true;
    SetPathPickMode(WorkspacePathPickMode::kPickStart, reason);
}

void App::SetPathPickMode(WorkspacePathPickMode mode, std::string_view reason)
{
    if (workspace_.path_pick_mode == mode) {
        logger_.Debug("path", "pick mode unchanged mode=" + std::string(ToString(mode)));
        return;
    }

    if (mode != WorkspacePathPickMode::kSelect) {
        preview_camera_.ReleaseMouse();
    }
    workspace_.path_pick_mode = mode;
    layout_dirty_ = true;
    logger_.Info("path", "pick_mode=" + std::string(ToString(mode)) + " reason=" + std::string(reason));
}

void App::RunPathProbeFromSelection(std::string_view reason)
{
    if (!workspace_.runtime_map.IsValid() || !workspace_.transition_features.IsValid()) {
        logger_.Warn("path", "path probe ignored because runtime map or transition features are unavailable");
        return;
    }
    if (!workspace_.has_path_start || !workspace_.has_path_goal) {
        logger_.Debug("path", "path probe ignored because start or goal is missing");
        return;
    }

    workspace_.path_probe = RunPathProbe(
        workspace_.runtime_map,
        workspace_.transition_features,
        workspace_.path_start,
        workspace_.path_goal,
        PathProbeOptions{workspace_.path_profile});
    layout_dirty_ = true;

    logger_.Info("path", "reason=" + std::string(reason) + " " + ToLogString(workspace_.path_probe));
    for (const auto& warning : workspace_.path_probe.diagnostics.warnings) {
        logger_.Warn("path", warning);
    }
}

void App::ClearPathProbe(std::string_view reason)
{
    workspace_.has_path_start = false;
    workspace_.has_path_goal = false;
    workspace_.path_start = TileCoord{};
    workspace_.path_goal = TileCoord{};
    workspace_.path_probe = PathProbeResult{};
    workspace_.path_pick_mode = WorkspacePathPickMode::kSelect;
    layout_dirty_ = true;
    logger_.Info("path", "clear reason=" + std::string(reason));
}

void App::SetSelectedTileAsPathEndpoint(bool set_goal, std::string_view reason)
{
    if (!workspace_.selected_tile.IsValid()) {
        logger_.Debug("path", "endpoint assignment ignored because no tile is selected");
        return;
    }

    if (set_goal) {
        workspace_.path_goal = workspace_.selected_tile.tile;
        workspace_.has_path_goal = true;
    } else {
        workspace_.path_start = workspace_.selected_tile.tile;
        workspace_.has_path_start = true;
    }
    workspace_.path_probe = PathProbeResult{};
    layout_dirty_ = true;

    logger_.Info(
        "path",
        std::string(set_goal ? "goal=" : "start=")
            + std::to_string(workspace_.selected_tile.tile.x) + ","
            + std::to_string(workspace_.selected_tile.tile.y)
            + " reason=" + std::string(reason));
}

void App::SetValidationMode(WorkspaceValidationMode mode, std::string_view reason)
{
    if (workspace_.validation_mode == mode) {
        logger_.Debug("passability", "validation mode unchanged mode=" + std::string(ToString(mode)));
        return;
    }

    workspace_.validation_mode = mode;
    if (mode == WorkspaceValidationMode::kOff) {
        ClearPassabilityValidation(reason);
    } else if (workspace_.passability_validation_status == WorkspaceValidationStatus::kDisabled) {
        workspace_.passability_validation_status = WorkspaceValidationStatus::kNotRun;
        workspace_.passability_validation_dirty = true;
    }

    logger_.Info(
        "passability",
        "mode=" + std::string(ToString(workspace_.validation_mode)) + " reason=" + std::string(reason));

    if (mode == WorkspaceValidationMode::kOnLoad && workspace_.passability_validation_dirty
        && workspace_.runtime_map.IsValid() && workspace_.transition_features.IsValid()) {
        RunPassabilityValidation("mode_on_load");
    }

    layout_dirty_ = true;
}

void App::RunPassabilityValidation(std::string_view reason)
{
    if (workspace_.validation_mode == WorkspaceValidationMode::kOff) {
        logger_.Debug("passability", "validation run ignored because validation mode is off");
        return;
    }
    if (!workspace_.runtime_map.IsValid() || !workspace_.transition_features.IsValid()) {
        logger_.Warn("passability", "validation run ignored because map or transition features are unavailable");
        return;
    }

    const auto start = std::chrono::steady_clock::now();
    PassabilityValidationReport next_passability_validation = ValidatePassability(
        workspace_.runtime_map,
        workspace_.transition_features);
    const auto finish = std::chrono::steady_clock::now();
    const std::chrono::duration<double, std::milli> elapsed = finish - start;

    workspace_.passability_validation_last_run_ms = elapsed.count();
    workspace_.passability_validation_status = WorkspaceValidationStatus::kDone;
    workspace_.passability_validation_dirty = false;
    workspace_.passability_validation = std::move(next_passability_validation);

    std::ostringstream out;
    out << "reason=" << reason << ' ' << ToLogString(workspace_.passability_validation)
        << " duration_ms=" << std::fixed << std::setprecision(2)
        << workspace_.passability_validation_last_run_ms;
    logger_.Info("passability", out.str());
    for (const auto& warning : workspace_.passability_validation.diagnostics.warnings) {
        logger_.Warn("passability", warning);
    }

    layout_dirty_ = true;
}

void App::ClearPassabilityValidation(std::string_view reason)
{
    workspace_.passability_validation = PassabilityValidationReport{};
    workspace_.passability_validation_last_run_ms = 0.0;
    workspace_.show_passability_issues = false;
    if (workspace_.validation_mode == WorkspaceValidationMode::kOff) {
        workspace_.passability_validation_status = WorkspaceValidationStatus::kDisabled;
        workspace_.passability_validation_dirty = false;
    } else {
        workspace_.passability_validation_status = WorkspaceValidationStatus::kNotRun;
        workspace_.passability_validation_dirty = workspace_.runtime_map.IsValid() && workspace_.transition_features.IsValid();
    }

    logger_.Info(
        "passability",
        "clear reason=" + std::string(reason)
            + " status=" + std::string(ToString(workspace_.passability_validation_status)));
    layout_dirty_ = true;
}

void App::TogglePassabilityValidationOverlay(std::string_view reason)
{
    if (!workspace_.passability_validation.IsValid() || workspace_.passability_validation.issues.empty()) {
        logger_.Debug("passability", "overlay toggle ignored because no validation issues are available");
        return;
    }

    workspace_.show_passability_issues = !workspace_.show_passability_issues;
    layout_dirty_ = true;
    logger_.Info("passability", std::string("overlay=")
        + (workspace_.show_passability_issues ? "on" : "off")
        + " reason=" + std::string(reason));
}

void App::SelectTileAtMouse(Vector2 mouse, std::string_view reason)
{
    if (!workspace_.show_3d_preview || !chunk_mesh_preview_.IsUploaded() || !workspace_.runtime_map.IsValid()) {
        return;
    }

    const bool path_pick_active = workspace_.path_pick_mode != WorkspacePathPickMode::kSelect;

    const auto picked_tile = chunk_mesh_preview_.PickTile(
        mouse,
        layout_cache_.workspace.map_overview,
        workspace_.runtime_map,
        preview_camera_.Camera());
    if (!picked_tile.has_value()) {
        workspace_.selected_tile = TileInspectResult{};
        workspace_.movement_probe = MovementProbeResult{};
        if (!path_pick_active) {
            workspace_.selected_panel_tab = WorkspacePanelTab::kInspect;
        }
        layout_dirty_ = true;
        logger_.Debug("inspect", "tile pick missed reason=" + std::string(reason));
        return;
    }

    workspace_.selected_tile = InspectTile(
        workspace_.runtime_map,
        workspace_.chunk_grid,
        workspace_.transition_features,
        *picked_tile);
    workspace_.movement_probe = BuildMovementProbe(
        workspace_.runtime_map,
        workspace_.transition_features,
        workspace_.selected_tile.tile);
    if (!path_pick_active) {
        workspace_.selected_panel_tab = WorkspacePanelTab::kInspect;
    }
    layout_dirty_ = true;
    logger_.Info("inspect", "reason=" + std::string(reason) + " " + ToLogString(workspace_.selected_tile));
    logger_.Info("movement", "reason=" + std::string(reason) + " " + ToLogString(workspace_.movement_probe));

    if (workspace_.path_pick_mode == WorkspacePathPickMode::kPickStart
        || workspace_.path_pick_mode == WorkspacePathPickMode::kPickGoal) {
        const bool set_goal = workspace_.path_pick_mode == WorkspacePathPickMode::kPickGoal;
        SetSelectedTileAsPathEndpoint(set_goal, "pick_tool");
        if (set_goal) {
            SetPathPickMode(WorkspacePathPickMode::kSelect, "pick_tool_complete");
            RunPathProbeFromSelection("pick_tool_complete");
        } else {
            SetPathPickMode(WorkspacePathPickMode::kPickGoal, "pick_tool_next_goal");
        }
    }
}

void App::ScrollWorkspaceMenu(int delta_rows, std::string_view reason)
{
    const bool scrollable_panel = workspace_.selected_panel_tab == WorkspacePanelTab::kMenu
        || workspace_.selected_panel_tab == WorkspacePanelTab::kStats
        || workspace_.selected_panel_tab == WorkspacePanelTab::kInspect;
    if (!scrollable_panel || delta_rows == 0) {
        return;
    }

    const WorkspaceLayout& workspace_layout = layout_cache_.workspace;
    const int max_scroll_rows = std::max(
        0,
        workspace_layout.panel_total_rows - workspace_layout.panel_visible_rows);
    const int next_scroll = std::clamp(
        workspace_.menu_scroll_rows + delta_rows,
        0,
        max_scroll_rows);
    if (next_scroll == workspace_.menu_scroll_rows) {
        return;
    }

    workspace_.menu_scroll_rows = next_scroll;
    layout_dirty_ = true;
    logger_.Debug("workspace", "menu_scroll=" + std::to_string(workspace_.menu_scroll_rows)
        + " reason=" + std::string(reason));
}

void App::ActivateWorkspacePanelItem(WorkspacePanelItem item)
{
    const bool read_only_tree_panel = workspace_.selected_panel_tab == WorkspacePanelTab::kStats
        || workspace_.selected_panel_tab == WorkspacePanelTab::kInspect;
    if (read_only_tree_panel && IsCollapsibleWorkspacePanelGroup(item)) {
        ToggleWorkspacePanelGroup(&workspace_, item);
        layout_dirty_ = true;
        logger_.Debug("workspace", "read-only group toggled id=" + std::string(ToString(item)));
        return;
    }

    bool activatable = false;
    for (const WorkspacePanelItemState& state : BuildWorkspacePanelItems(workspace_)) {
        if (state.item != item) {
            continue;
        }
        activatable = state.enabled
            && (state.kind == WorkspacePanelItemKind::kAction
                || state.kind == WorkspacePanelItemKind::kCheckbox
                || state.kind == WorkspacePanelItemKind::kRadio
                || (state.kind == WorkspacePanelItemKind::kGroup && IsCollapsibleWorkspacePanelGroup(state.item)));
        break;
    }

    if (!activatable) {
        logger_.Debug("workspace", "inactive panel item ignored id=" + std::string(ToString(item)));
        return;
    }

    if (IsCollapsibleWorkspacePanelGroup(item)) {
        ToggleWorkspacePanelGroup(&workspace_, item);
        layout_dirty_ = true;
        logger_.Debug("workspace", "panel group toggled id=" + std::string(ToString(item)));
        return;
    }

    switch (item) {
        case WorkspacePanelItem::kLayerTerrain:
            workspace_.show_terrain_layer = !workspace_.show_terrain_layer;
            break;
        case WorkspacePanelItem::kLayerElevation:
            workspace_.show_elevation_layer = !workspace_.show_elevation_layer;
            break;
        case WorkspacePanelItem::kLayerCollision:
            workspace_.show_collision_layer = !workspace_.show_collision_layer;
            break;
        case WorkspacePanelItem::kLayerGrid:
            workspace_.show_grid_layer = !workspace_.show_grid_layer;
            break;
        case WorkspacePanelItem::kMode2DMap:
            workspace_.show_3d_preview = false;
            preview_camera_.ReleaseMouse();
            logger_.Info("workspace", "preview mode=2d");
            break;
        case WorkspacePanelItem::kMode3DWorld:
        case WorkspacePanelItem::kRenderTerrainMesh:
            workspace_.show_3d_preview = chunk_mesh_preview_.IsUploaded();
            if (workspace_.show_3d_preview && !preview_camera_.IsInitialized()) {
                FitPreviewCameraToViewport("panel");
            }
            logger_.Info("workspace", std::string("preview mode=") + (workspace_.show_3d_preview ? "3d" : "3d_unavailable"));
            break;
        case WorkspacePanelItem::kViewFitMap:
            FitPreviewCameraToViewport("panel");
            break;
        case WorkspacePanelItem::kViewResetView:
            preview_camera_.ResetView();
            logger_.Info("camera3d", "reset view " + ToLogString(preview_camera_.Status()));
            break;
        case WorkspacePanelItem::kRenderChunkBounds:
            ToggleOverlayFlag(
                workspace_.show_3d_chunk_bounds,
                "chunk_bounds",
                OverlayPrimitiveCount(workspace_, item),
                logger_,
                layout_dirty_);
            break;
        case WorkspacePanelItem::kRenderWorldGrid:
            ToggleOverlayFlag(
                workspace_.show_3d_world_grid,
                "world_grid",
                OverlayPrimitiveCount(workspace_, item),
                logger_,
                layout_dirty_);
            break;
        case WorkspacePanelItem::kRenderCollision:
            ToggleOverlayFlag(
                workspace_.show_3d_collision_overlay,
                "collision_overlay",
                OverlayPrimitiveCount(workspace_, item),
                logger_,
                layout_dirty_);
            break;
        case WorkspacePanelItem::kRenderHeight:
            ToggleOverlayFlag(
                workspace_.show_3d_height_overlay,
                "height_overlay",
                OverlayPrimitiveCount(workspace_, item),
                logger_,
                layout_dirty_);
            break;
        case WorkspacePanelItem::k3DObjectsAll: {
            const bool enable_all = !(workspace_.show_3d_object_trees
                && workspace_.show_3d_object_bushes
                && workspace_.show_3d_object_reeds
                && workspace_.show_3d_object_ruins
                && workspace_.show_3d_object_cover
                && workspace_.show_3d_object_loot
                && workspace_.show_3d_object_structures
                && workspace_.show_3d_object_trenches
                && workspace_.show_3d_object_unknown);
            workspace_.show_3d_object_trees = enable_all;
            workspace_.show_3d_object_bushes = enable_all;
            workspace_.show_3d_object_reeds = enable_all;
            workspace_.show_3d_object_ruins = enable_all;
            workspace_.show_3d_object_cover = enable_all;
            workspace_.show_3d_object_loot = enable_all;
            workspace_.show_3d_object_structures = enable_all;
            workspace_.show_3d_object_trenches = enable_all;
            workspace_.show_3d_object_unknown = enable_all;
            layout_dirty_ = true;
            logger_.Info("workspace", std::string("object_filters all=") + (enable_all ? "on" : "off"));
            break;
        }
        case WorkspacePanelItem::k3DObjectsTrees:
            ToggleOverlayFlag(workspace_.show_3d_object_trees, "objects_trees", OverlayPrimitiveCount(workspace_, item), logger_, layout_dirty_);
            break;
        case WorkspacePanelItem::k3DObjectsBushes:
            ToggleOverlayFlag(workspace_.show_3d_object_bushes, "objects_bushes", OverlayPrimitiveCount(workspace_, item), logger_, layout_dirty_);
            break;
        case WorkspacePanelItem::k3DObjectsReeds:
            ToggleOverlayFlag(workspace_.show_3d_object_reeds, "objects_reeds", OverlayPrimitiveCount(workspace_, item), logger_, layout_dirty_);
            break;
        case WorkspacePanelItem::k3DObjectsRuins:
            ToggleOverlayFlag(workspace_.show_3d_object_ruins, "objects_ruins", OverlayPrimitiveCount(workspace_, item), logger_, layout_dirty_);
            break;
        case WorkspacePanelItem::k3DObjectsCover:
            ToggleOverlayFlag(workspace_.show_3d_object_cover, "objects_cover", OverlayPrimitiveCount(workspace_, item), logger_, layout_dirty_);
            break;
        case WorkspacePanelItem::k3DObjectsLoot:
            ToggleOverlayFlag(workspace_.show_3d_object_loot, "objects_loot", OverlayPrimitiveCount(workspace_, item), logger_, layout_dirty_);
            break;
        case WorkspacePanelItem::k3DObjectsStructures:
            ToggleOverlayFlag(workspace_.show_3d_object_structures, "objects_structures", OverlayPrimitiveCount(workspace_, item), logger_, layout_dirty_);
            break;
        case WorkspacePanelItem::k3DObjectsTrenches:
            ToggleOverlayFlag(workspace_.show_3d_object_trenches, "objects_trenches", OverlayPrimitiveCount(workspace_, item), logger_, layout_dirty_);
            break;
        case WorkspacePanelItem::k3DObjectsUnknown:
            ToggleOverlayFlag(workspace_.show_3d_object_unknown, "objects_unknown", OverlayPrimitiveCount(workspace_, item), logger_, layout_dirty_);
            break;
        case WorkspacePanelItem::k3DColorTraversal:
            SetColorMode(WorkspaceColorMode::kTraversal, "panel");
            break;
        case WorkspacePanelItem::k3DColorGeographic:
            SetColorMode(WorkspaceColorMode::kGeographic, "panel");
            break;
        case WorkspacePanelItem::k3DColorChunkId:
            SetColorMode(WorkspaceColorMode::kChunkId, "panel");
            break;
        case WorkspacePanelItem::k3DColorFaceType:
            SetColorMode(WorkspaceColorMode::kFaceType, "panel");
            break;
        case WorkspacePanelItem::k3DVisibilityAllChunks:
            SetVisibilityMode(WorkspaceVisibilityMode::kAllChunks, "panel");
            break;
        case WorkspacePanelItem::k3DVisibilityRadiusFade:
            SetVisibilityMode(WorkspaceVisibilityMode::kRadiusFade, "panel");
            break;
        case WorkspacePanelItem::k3DVisibilityHardCull:
            SetVisibilityMode(WorkspaceVisibilityMode::kHardCull, "panel");
            break;
        case WorkspacePanelItem::k3DVisibilityFrustumCull:
            SetVisibilityMode(WorkspaceVisibilityMode::kFrustumCull, "panel");
            break;
        case WorkspacePanelItem::k3DVisibilityRadiusMinus:
            AdjustVisibilityRadius(-1, "panel");
            break;
        case WorkspacePanelItem::k3DVisibilityRadiusPlus:
            AdjustVisibilityRadius(1, "panel");
            break;
        case WorkspacePanelItem::k3DVisibilityFadeMinus:
            AdjustVisibilityFadeRing(-1, "panel");
            break;
        case WorkspacePanelItem::k3DVisibilityFadePlus:
            AdjustVisibilityFadeRing(1, "panel");
            break;
        case WorkspacePanelItem::k3DShowHiddenBounds:
            workspace_.show_3d_hidden_chunk_bounds = !workspace_.show_3d_hidden_chunk_bounds;
            UpdateVisibilityStats();
            logger_.Info("visibility", std::string("hidden_bounds=")
                + (workspace_.show_3d_hidden_chunk_bounds ? "on" : "off"));
            break;
        case WorkspacePanelItem::k3DTerrainPassTops:
            workspace_.show_terrain_tops = !workspace_.show_terrain_tops;
            UpdateVisibilityStats();
            layout_dirty_ = true;
            logger_.Info("render3d", std::string("terrain_pass_tops=")
                + (workspace_.show_terrain_tops ? "on" : "off"));
            break;
        case WorkspacePanelItem::k3DTerrainPassWalls:
            workspace_.show_terrain_walls = !workspace_.show_terrain_walls;
            UpdateVisibilityStats();
            layout_dirty_ = true;
            logger_.Info("render3d", std::string("terrain_pass_walls=")
                + (workspace_.show_terrain_walls ? "on" : "off"));
            break;
        case WorkspacePanelItem::k3DTerrainPassCliffs:
            workspace_.show_terrain_cliffs = !workspace_.show_terrain_cliffs;
            UpdateVisibilityStats();
            layout_dirty_ = true;
            logger_.Info("render3d", std::string("terrain_pass_cliffs=")
                + (workspace_.show_terrain_cliffs ? "on" : "off"));
            break;
        case WorkspacePanelItem::k3DShowTransitions:
            ToggleTransitionOverlay("panel");
            break;
        case WorkspacePanelItem::k3DTransitionRamps:
            workspace_.show_transition_ramps = !workspace_.show_transition_ramps;
            layout_dirty_ = true;
            logger_.Info("transitions", std::string("ramps=")
                + (workspace_.show_transition_ramps ? "on" : "off"));
            break;
        case WorkspacePanelItem::k3DTransitionStairs:
            workspace_.show_transition_stairs = !workspace_.show_transition_stairs;
            layout_dirty_ = true;
            logger_.Info("transitions", std::string("stairs=")
                + (workspace_.show_transition_stairs ? "on" : "off"));
            break;
        case WorkspacePanelItem::k3DTransitionBridges:
            workspace_.show_transition_bridges = !workspace_.show_transition_bridges;
            layout_dirty_ = true;
            logger_.Info("transitions", std::string("bridges=")
                + (workspace_.show_transition_bridges ? "on" : "off"));
            break;
        case WorkspacePanelItem::k3DTransitionDrops:
            workspace_.show_transition_drops = !workspace_.show_transition_drops;
            layout_dirty_ = true;
            logger_.Info("transitions", std::string("drops=")
                + (workspace_.show_transition_drops ? "on" : "off"));
            break;
        case WorkspacePanelItem::k3DShowMovementProbe:
            ToggleMovementProbeOverlay("panel");
            break;
        case WorkspacePanelItem::k3DPathProfileShortest:
            SetPathProfile(PathProfile::kShortest, "panel");
            break;
        case WorkspacePanelItem::k3DPathProfileSafe:
            SetPathProfile(PathProfile::kSafe, "panel");
            break;
        case WorkspacePanelItem::k3DPathToolSelect:
            SetPathPickMode(WorkspacePathPickMode::kSelect, "panel");
            break;
        case WorkspacePanelItem::k3DPathToolPickStart:
            BeginPathPickMode("panel");
            break;
        case WorkspacePanelItem::k3DPathToolPickGoal:
            SetPathPickMode(WorkspacePathPickMode::kPickGoal, "panel");
            break;
        case WorkspacePanelItem::k3DRunPathProbe:
            RunPathProbeFromSelection("panel");
            break;
        case WorkspacePanelItem::k3DClearPathProbe:
            ClearPathProbe("panel");
            break;
        case WorkspacePanelItem::k3DShowPath:
            workspace_.show_path_overlay = !workspace_.show_path_overlay;
            layout_dirty_ = true;
            logger_.Info("path", std::string("overlay=") + (workspace_.show_path_overlay ? "on" : "off"));
            break;
        case WorkspacePanelItem::k3DShowPathVisited:
            workspace_.show_path_visited = !workspace_.show_path_visited;
            layout_dirty_ = true;
            logger_.Info("path", std::string("visited_overlay=") + (workspace_.show_path_visited ? "on" : "off"));
            break;
        case WorkspacePanelItem::k3DValidationModeOff:
            SetValidationMode(WorkspaceValidationMode::kOff, "panel");
            break;
        case WorkspacePanelItem::k3DValidationModeManual:
            SetValidationMode(WorkspaceValidationMode::kManual, "panel");
            break;
        case WorkspacePanelItem::k3DValidationModeOnLoad:
            SetValidationMode(WorkspaceValidationMode::kOnLoad, "panel");
            break;
        case WorkspacePanelItem::k3DRunPassabilityValidation:
            RunPassabilityValidation("panel");
            break;
        case WorkspacePanelItem::k3DClearPassabilityValidation:
            ClearPassabilityValidation("panel");
            break;
        case WorkspacePanelItem::k3DShowPassabilityIssues:
            TogglePassabilityValidationOverlay("panel");
            break;
        case WorkspacePanelItem::k3DValidationInvalidTransitions:
            workspace_.show_passability_invalid_transitions = !workspace_.show_passability_invalid_transitions;
            layout_dirty_ = true;
            logger_.Info("passability", std::string("invalid_transitions=")
                + (workspace_.show_passability_invalid_transitions ? "on" : "off"));
            break;
        case WorkspacePanelItem::k3DValidationBlockedTransitions:
            workspace_.show_passability_blocked_transitions = !workspace_.show_passability_blocked_transitions;
            layout_dirty_ = true;
            logger_.Info("passability", std::string("blocked_transitions=")
                + (workspace_.show_passability_blocked_transitions ? "on" : "off"));
            break;
        case WorkspacePanelItem::k3DValidationSuspiciousDrops:
            workspace_.show_passability_suspicious_drops = !workspace_.show_passability_suspicious_drops;
            layout_dirty_ = true;
            logger_.Info("passability", std::string("suspicious_drops=")
                + (workspace_.show_passability_suspicious_drops ? "on" : "off"));
            break;
        case WorkspacePanelItem::k3DValidationIsolatedTiles:
            workspace_.show_passability_isolated_tiles = !workspace_.show_passability_isolated_tiles;
            layout_dirty_ = true;
            logger_.Info("passability", std::string("isolated_tiles=")
                + (workspace_.show_passability_isolated_tiles ? "on" : "off"));
            break;
        case WorkspacePanelItem::k3DMeshSimple:
            SetMeshBuildMode(ChunkMeshBuildMode::kSimpleFaces, "panel");
            break;
        case WorkspacePanelItem::k3DChunkSize16:
            SetChunkSize(kChunkSize16, "panel");
            break;
        case WorkspacePanelItem::k3DChunkSize32:
            SetChunkSize(kChunkSize32, "panel");
            break;
        case WorkspacePanelItem::k3DMeshGreedy:
            SetMeshBuildMode(ChunkMeshBuildMode::kGreedyFaces, "panel");
            break;
        case WorkspacePanelItem::k3DMeshTerrainSurface:
            SetMeshBuildMode(ChunkMeshBuildMode::kTerrainSurface, "panel");
            break;
        case WorkspacePanelItem::k3DDirtyRebuildProbe:
            RunDirtyRebuildProbe("panel");
            break;
        default:
            break;
    }

    layout_dirty_ = true;
    logger_.Debug("workspace", "panel item activated id=" + std::string(ToString(item)));
}



void App::FitPreviewCameraToViewport(std::string_view reason)
{
    if (!chunk_mesh_preview_.IsUploaded() || !workspace_.chunk_meshes.IsValid()) {
        logger_.Debug("camera3d", "fit ignored reason=" + std::string(reason));
        return;
    }
    if (layout_dirty_) {
        RebuildLayout();
    }
    preview_camera_.FitToMap(workspace_.chunk_meshes, layout_cache_.workspace.map_overview);
    logger_.Info("camera3d", "fit map reason=" + std::string(reason) + " " + ToLogString(preview_camera_.Status()));
}

void App::SetCurrentScreen(AppScreen screen, std::string_view reason)
{
    if (screen_ == screen) {
        return;
    }
    screen_ = screen;
    if (screen_ == AppScreen::kSettingsPlaceholder) {
        placeholder_selected_action_ = PlaceholderAction::kMainMenu;
    }
    logger_.Info("app", "screen changed screen=" + std::string(ToString(screen_)) + " reason=" + std::string(reason));
}

void App::RequestExitConfirmation(bool from_window_close)
{
    if (dialog_.type == ModalDialog::kExitConfirmation) {
        return;
    }
    preview_camera_.ReleaseMouse();
    exit_requested_from_window_ = from_window_close;
    dialog_.type = ModalDialog::kExitConfirmation;
    dialog_.selected_choice = DialogChoice::kNo;
    dialog_input_blocked_until_next_frame_ = true;
    logger_.Debug("dialog", "opened type=exit_confirmation default=no");
}

void App::CancelExitConfirmation()
{
    if (dialog_.type == ModalDialog::kNone) {
        return;
    }
    const bool was_window_close_request = exit_requested_from_window_;
    dialog_.type = ModalDialog::kNone;
    dialog_.selected_choice = DialogChoice::kNo;
    exit_requested_from_window_ = false;
    dialog_input_blocked_until_next_frame_ = false;
    logger_.Info("app", "exit cancelled by user");
    if (was_window_close_request) {
        logger_.Info("window", "close cancelled by user");
    }
}

void App::AcceptExitConfirmation()
{
    exit_requested_from_window_ = false;
    dialog_input_blocked_until_next_frame_ = false;
    logger_.Info("app", "exit accepted by user");
    running_ = false;
}

namespace {

[[nodiscard]] int FontAtlasSize(float base_size, float font_scale, int minimum, int maximum)
{
    const float resolved_scale = std::clamp(font_scale, 0.50F, 2.00F);
    return std::clamp(static_cast<int>(std::round(base_size * resolved_scale)), minimum, maximum);
}

[[nodiscard]] Font LoadConfiguredFont(
    const std::filesystem::path& path,
    int atlas_size,
    TextureFilter filter,
    std::string_view role,
    Logger& logger,
    bool& loaded)
{
    const std::string font_path = path.string();
    if (!FileExists(font_path.c_str())) {
        logger.Warn("assets", std::string(role) + " font not found path=\"" + font_path + "\", using default font");
        loaded = false;
        return GetFontDefault();
    }

    const std::vector<int> codepoints = BuildFontCodepoints();
    Font font = LoadFontEx(font_path.c_str(), atlas_size, const_cast<int*>(codepoints.data()), static_cast<int>(codepoints.size()));
    if (font.texture.id == 0) {
        logger.Warn("assets", std::string("failed to load ") + std::string(role) + " font path=\"" + font_path + "\", using default font");
        loaded = false;
        return GetFontDefault();
    }

    SetTextureFilter(font.texture, filter);
    loaded = true;
    logger.Info(
        "assets",
        std::string(role) + " font loaded path=\"" + font_path + "\" atlas_size=" + std::to_string(atlas_size));
    return font;
}

}  // namespace

void App::LoadUiFonts()
{
    title_font_ = LoadConfiguredFont(
        config_.ui_title_font_path,
        FontAtlasSize(38.0F, config_.ui_font_scale, 22, 72),
        TEXTURE_FILTER_BILINEAR,
        "title",
        logger_,
        title_font_loaded_);

    text_font_ = LoadConfiguredFont(
        config_.ui_text_font_path,
        FontAtlasSize(26.0F, config_.ui_font_scale, 16, 56),
        TEXTURE_FILTER_BILINEAR,
        "text",
        logger_,
        text_font_loaded_);
}

void App::UnloadUiFonts()
{
    if (title_font_loaded_) {
        UnloadFont(title_font_);
        title_font_loaded_ = false;
        title_font_ = Font{};
        logger_.Debug("assets", "title font unloaded");
    }
    if (text_font_loaded_) {
        UnloadFont(text_font_);
        text_font_loaded_ = false;
        text_font_ = Font{};
        logger_.Debug("assets", "text font unloaded");
    }
}


void App::UnloadPreviewResources()
{
    if (chunk_mesh_preview_.IsUploaded()) {
        chunk_mesh_preview_.Unload();
        logger_.Debug("render3d", "preview resources unloaded");
    }
}

void App::RefreshProcessMemoryInfo()
{
    process_memory_ = QueryProcessMemoryInfo();
}

void App::LogSelectedItemChanged() const
{
    const MenuItem* item = main_menu_.SelectedItem();
    if (item == nullptr) {
        logger_.Debug("menu", "selected item changed selected=none");
        return;
    }
    logger_.Debug(
        "menu",
        "selected item changed index=" + std::to_string(main_menu_.State().selected_index) + " id="
            + std::string(ToString(item->id)));
}

UiFontSet App::UiFonts() const
{
    return UiFontSet{
        title_font_loaded_ ? title_font_ : GetFontDefault(),
        text_font_loaded_ ? text_font_ : GetFontDefault(),
    };
}

AppScreen App::CurrentScreen() const
{
    return screen_;
}

std::string_view ToString(AppScreen screen)
{
    switch (screen) {
        case AppScreen::kMainMenu:
            return "main_menu";
        case AppScreen::kWorkspace:
            return "main_render";
        case AppScreen::kSettingsPlaceholder:
            return "settings_placeholder";
    }
    return "unknown";
}

}  // namespace vox3d
