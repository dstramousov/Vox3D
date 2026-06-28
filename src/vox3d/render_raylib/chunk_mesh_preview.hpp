#pragma once

#include "vox3d/chunk/chunk_grid.hpp"
#include "vox3d/map/runtime_map.hpp"
#include "vox3d/mesh/mesh_data.hpp"
#include "vox3d/render/chunk_visibility.hpp"
#include "vox3d/transition/transition_feature.hpp"

#include <raylib.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace vox3d {

/**
 * @brief Vertex-color palette used when uploading chunk meshes to raylib.
 */
enum class RaylibChunkMeshColorMode : std::uint8_t {
    kMaterial,
    kGeographic,
    kChunkId,
    kFaceType,
};

/**
 * @brief Chunk visibility mode used by the raylib debug preview.
 */
enum class RaylibChunkVisibilityMode : std::uint8_t {
    kAllChunks,
    kRadiusFade,
    kHardCull,
    kFrustumCull,
};

/**
 * @brief Converts a raylib chunk-mesh color mode to a stable diagnostic name.
 *
 * @param mode Color mode identifier.
 * @return Stable lowercase string representation.
 */
[[nodiscard]] std::string_view ToString(RaylibChunkMeshColorMode mode);

/**
 * @brief Converts a raylib chunk visibility mode to a stable diagnostic name.
 *
 * @param mode Visibility mode identifier.
 * @return Stable lowercase string representation.
 */
[[nodiscard]] std::string_view ToString(RaylibChunkVisibilityMode mode);

/**
 * @brief 3D debug overlay visibility flags for the chunk-mesh preview.
 */
struct RaylibChunkMeshDebugOverlayOptions {
    bool show_chunk_bounds = false;
    bool show_world_grid = false;
    bool show_collision = false;
    bool show_height = false;
};

/**
 * @brief Chunk visibility options used by the 3D preview draw pass.
 */
struct RaylibChunkVisibilityOptions {
    RaylibChunkVisibilityMode mode = RaylibChunkVisibilityMode::kAllChunks;
    int radius_chunks = 2;
    int fade_ring_chunks = 1;
    bool show_hidden_bounds = false;
    float viewport_aspect_ratio = 1.0F;
};

/**
 * @brief Terrain render pass visibility flags used by the 3D preview.
 */
struct RaylibTerrainPassOptions {
    bool show_tops = true;
    bool show_walls = true;
    bool show_cliffs = true;
};

/**
 * @brief Transition feature overlay visibility flags used by the 3D preview.
 */
struct RaylibTransitionOverlayOptions {
    bool show = false;
    bool show_ramps = true;
    bool show_stairs = true;
    bool show_bridges = true;
    bool show_drops = true;
};

/**
 * @brief Last measured visibility and draw-culling counters.
 */
struct RaylibChunkVisibilityStats {
    RaylibChunkVisibilityMode mode = RaylibChunkVisibilityMode::kAllChunks;
    int radius_chunks = 0;
    int fade_ring_chunks = 0;
    std::uint64_t resident_chunks = 0;
    std::uint64_t resident_models = 0;
    std::uint64_t visible_chunks = 0;
    std::uint64_t fade_chunks = 0;
    std::uint64_t hidden_chunks = 0;
    std::uint64_t drawn_models = 0;
    std::uint64_t culled_models = 0;
    std::uint64_t total_faces = 0;
    std::uint64_t drawn_faces = 0;
    std::uint64_t culled_faces = 0;

    /**
     * @brief Returns true when visibility counters reference uploaded chunks.
     *
     * @return True if at least one resident chunk was counted.
     */
    [[nodiscard]] bool IsValid() const;

    /**
     * @brief Returns the share of renderer models skipped by visibility culling.
     *
     * @return Ratio in range [0, 1], or zero when no resident chunks exist.
     */
    [[nodiscard]] double DrawSavedRatio() const;

    /**
     * @brief Returns the share of mesh faces skipped by visibility culling.
     *
     * @return Ratio in range [0, 1], or zero when no faces exist.
     */
    [[nodiscard]] double FaceSavedRatio() const;
};

/**
 * @brief Upload statistics for the raylib chunk-mesh preview renderer.
 */
struct RaylibChunkMeshPreviewStats {
    bool uploaded = false;
    std::uint64_t models = 0;
    std::uint64_t faces = 0;
    std::uint64_t vertices = 0;
    std::uint64_t indices = 0;
    std::uint64_t skipped_chunks = 0;

    /**
     * @brief Returns true when at least one renderer model is ready to draw.
     *
     * @return True if GPU resources were uploaded successfully.
     */
    [[nodiscard]] bool IsValid() const;
};

/**
 * @brief One uploaded raylib model plus the chunk metadata needed for culling.
 */
struct RaylibUploadedChunkModel {
    Model model{};
    ChunkCoord coord{};
    TileBounds bounds{};
    Aabb3f world_bounds{};
    TerrainRenderPass terrain_pass = TerrainRenderPass::kBody;
    std::size_t visibility_item_index = 0;
    std::uint64_t faces = 0;
};

/**
 * @brief Raylib-backed debug preview renderer for generated chunk meshes.
 *
 * The renderer owns raylib Model resources created from renderer-independent
 * ChunkMeshData. Upload and unload must happen while a raylib window/context is
 * alive. The class does not own source map, voxel, or chunk data.
 */
class RaylibChunkMeshPreview {
public:
    /**
     * @brief Releases any uploaded preview resources.
     */
    ~RaylibChunkMeshPreview();

    RaylibChunkMeshPreview() = default;
    RaylibChunkMeshPreview(const RaylibChunkMeshPreview&) = delete;
    RaylibChunkMeshPreview& operator=(const RaylibChunkMeshPreview&) = delete;
    RaylibChunkMeshPreview(RaylibChunkMeshPreview&&) = delete;
    RaylibChunkMeshPreview& operator=(RaylibChunkMeshPreview&&) = delete;

    /**
     * @brief Uploads chunk mesh data into raylib Model resources.
     *
     * Existing preview resources are unloaded before the new upload attempt.
     * Chunks that exceed raylib's 16-bit mesh index limit are skipped and
     * reported in the resulting stats.
     *
     * @param build_result Renderer-independent chunk mesh data.
     * @param color_mode Vertex-color mode applied during upload.
     * @return True if at least one non-empty chunk model was uploaded.
     */
    [[nodiscard]] bool Upload(const ChunkMeshBuildResult& build_result, RaylibChunkMeshColorMode color_mode);

    /**
     * @brief Draws uploaded chunk mesh models inside the viewport rectangle.
     *
     * @param viewport Screen-space viewport rectangle.
     * @param build_result Original mesh build summary used for map dimensions.
     * @param camera Camera used for the 3D preview draw pass.
     * @param runtime_map Optional runtime map used by debug overlays.
     * @param chunk_grid Optional chunk grid used by debug overlays.
     * @param overlays 3D debug overlay visibility flags.
     * @param visibility Chunk visibility and soft-fade options.
     * @param terrain_passes Terrain render-pass visibility flags.
     * @param transition_features Optional transition feature set drawn as debug markers.
     * @param transitions Transition feature overlay visibility flags.
     */
    void Draw(
        Rectangle viewport,
        const ChunkMeshBuildResult& build_result,
        const Camera3D& camera,
        const RuntimeMap* runtime_map = nullptr,
        const ChunkGrid* chunk_grid = nullptr,
        RaylibChunkMeshDebugOverlayOptions overlays = {},
        RaylibChunkVisibilityOptions visibility = {},
        RaylibTerrainPassOptions terrain_passes = {},
        const TransitionFeatureSet* transition_features = nullptr,
        RaylibTransitionOverlayOptions transitions = {}) const;

    /**
     * @brief Calculates visibility counters for the current camera and options.
     *
     * The method does not draw and can be called before rendering to update UI
     * and log diagnostics.
     *
     * @param build_result Original mesh build summary used for map dimensions.
     * @param camera Camera used for the 3D preview draw pass.
     * @param visibility Chunk visibility and soft-fade options.
     * @param terrain_passes Terrain render-pass visibility flags used when counting models.
     * @return Visibility counters for uploaded chunks.
     */
    [[nodiscard]] RaylibChunkVisibilityStats CalculateVisibilityStats(
        const ChunkMeshBuildResult& build_result,
        const Camera3D& camera,
        RaylibChunkVisibilityOptions visibility = {},
        RaylibTerrainPassOptions terrain_passes = {}) const;

    /**
     * @brief Releases uploaded raylib Model resources.
     */
    void Unload();

    /**
     * @brief Returns true when preview resources are available.
     *
     * @return True if one or more models are uploaded.
     */
    [[nodiscard]] bool IsUploaded() const;

    /**
     * @brief Returns current upload statistics.
     *
     * @return Upload statistics for diagnostics and UI display.
     */
    [[nodiscard]] const RaylibChunkMeshPreviewStats& Stats() const;

private:
    std::vector<RaylibUploadedChunkModel> chunks_;
    std::vector<ChunkVisibilityItem> visibility_items_;
    RaylibChunkMeshPreviewStats stats_;
};

/**
 * @brief Builds a compact stable log string for raylib preview diagnostics.
 *
 * @param stats Raylib preview upload statistics.
 * @return Compact human-readable summary.
 */
[[nodiscard]] std::string ToLogString(const RaylibChunkMeshPreviewStats& stats);

/**
 * @brief Builds a compact stable log string for visibility diagnostics.
 *
 * @param stats Visibility counters.
 * @return Compact human-readable summary.
 */
[[nodiscard]] std::string ToLogString(const RaylibChunkVisibilityStats& stats);

}  // namespace vox3d
