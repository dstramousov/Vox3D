#pragma once

#include "vox3d/chunk/chunk_grid.hpp"
#include "vox3d/map/map_package.hpp"
#include "vox3d/map/runtime_map.hpp"
#include "vox3d/mesh/face_visibility.hpp"
#include "vox3d/mesh/chunk_mesh_cache.hpp"
#include "vox3d/mesh/mesh_data.hpp"
#include "vox3d/transition/transition_feature.hpp"
#include "vox3d/voxel/voxel_world.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace vox3d {

/**
 * @brief Top-level workspace section shown in the right-side tree panel.
 */
enum class WorkspaceTool {
    kMode,
    kMap2D,
    kWorld3D,
    kSelection,
    kPackageData,
    kDebug,
    kSettings,
};

/**
 * @brief High-level right panel tab shown above workspace controls.
 */
enum class WorkspacePanelTab {
    kMenu,
    kStats,
    kInspect,
    kHelp,
};

/**
 * @brief Semantic kind of a workspace tree row.
 */
enum class WorkspacePanelItemKind {
    kGroup,
    kAction,
    kCheckbox,
    kRadio,
    kValue,
};

/**
 * @brief Diagnostic vertex-color mode used by the 3D workspace preview.
 */
enum class WorkspaceColorMode {
    kMaterial,
    kGeographic,
    kChunkId,
    kFaceType,
};

/**
 * @brief Chunk visibility presentation mode used by the 3D workspace preview.
 */
enum class WorkspaceVisibilityMode {
    kAllChunks,
    kRadiusFade,
    kHardCull,
    kFrustumCull,
};

/**
 * @brief Clickable item shown inside an expanded workspace tree section.
 */
enum class WorkspacePanelItem {
    kMenuModeGroup,
    kMode2DMap,
    kMode3DWorld,

    k2DNavigationGroup,
    k2DFitView,
    k2DResetView,
    k2DZoomIn,
    k2DZoomOut,
    k2DBaseLayerGroup,
    kLayerTerrain,
    kLayerElevation,
    kLayerCollision,
    k2DOverlayGroup,
    kLayerGrid,
    k2DChunks,
    k2DStartGoal,
    k2DObjects,
    k2DPlaces,
    k2DMarkers,
    k2DRoutes,
    k2DWorldGraph,
    k2DGameplayZones,
    k2DElevationFeatures,
    k2DElevationTransitions,

    k3DCameraGroup,
    kViewFitMap,
    kViewResetView,
    k3DCaptureMouse,
    k3DReleaseMouse,
    k3DRenderGroup,
    kRenderTerrainMesh,
    k3DColorModeGroup,
    k3DColorMaterial,
    k3DColorGeographic,
    k3DColorChunkId,
    k3DColorFaceType,
    k3DVisibilityGroup,
    k3DVisibilityAllChunks,
    k3DVisibilityRadiusFade,
    k3DVisibilityHardCull,
    k3DVisibilityFrustumCull,
    k3DVisibilityRadiusMinus,
    k3DVisibilityRadiusPlus,
    k3DVisibilityFadeMinus,
    k3DVisibilityFadePlus,
    k3DShowHiddenBounds,
    k3DTerrainPassGroup,
    k3DTerrainPassTops,
    k3DTerrainPassWalls,
    k3DTerrainPassCliffs,
    k3DTransitionGroup,
    k3DShowTransitions,
    k3DTransitionRamps,
    k3DTransitionStairs,
    k3DTransitionBridges,
    k3DTransitionDrops,
    kRenderChunkBounds,
    kRenderWorldGrid,
    kRenderCollision,
    kRenderHeight,
    k3DMeshGroup,
    k3DChunkSizeGroup,
    k3DChunkSize16,
    k3DChunkSize32,
    k3DChunkSizeProfit,
    k3DMeshSimple,
    k3DMeshGreedy,
    k3DMeshTerrainSurface,
    k3DDrawModels,
    k3DVisibleFaces,
    k3DCulledFaces,
    k3DGreedySaved,
    k3DTerrainFaces,
    k3DTerrainTopFaces,
    k3DTerrainWallFaces,
    k3DTerrainVsGreedy,
    k3DTotalSaved,
    k3DChunkMeshes,
    k3DDirtyRebuildProbe,
    k3DDirtyChunks,
    k3DRebuiltChunks,
    k3DRebuildSaved,

    kSelectionTileGroup,
    kSelectionTileInfo,
    kSelectionVoxelGroup,
    kSelectionVoxelInfo,
    kSelectionChunkGroup,
    kSelectionChunkInfo,
    kSelectionActionsGroup,
    kSelectionInspect,
    kSelectionFocus,
    kSelectionCopyInfo,

    kPackageMetadataGroup,
    kMapPackage,
    kMapValidate,
    kPackageRuntimeGridsGroup,
    kPackageHeightGrid,
    kPackageCollisionGrid,
    kPackageMovementCostGrid,
    kPackageWorldDataGroup,
    kPackageObjects,
    kPackageMarkers,
    kPackageRoutes,
    kPackageGameplayZones,

    kDebugRuntimeMap,
    kDebugChunkGrid,
    kDebugVoxelWorld,
    kDebugFaceVisibility,
    kDebugChunkMesh,
    kDebugCamera,
    kDebugMemory,
    kDebugFps,
    kDebugLogs,

    kSettingsLanguage,
    kSettingsCamera,
    kSettingsRender,
};

/**
 * @brief Runtime state for one workspace tree row.
 */
struct WorkspacePanelItemState {
    WorkspacePanelItem item = WorkspacePanelItem::kMode2DMap;
    WorkspacePanelItemKind kind = WorkspacePanelItemKind::kAction;
    int depth = 1;
    bool enabled = false;
    bool checked = false;
};


/**
 * @brief Last measured visibility and draw-culling counters for the 3D preview.
 */
struct WorkspaceVisibilityStats {
    WorkspaceVisibilityMode mode = WorkspaceVisibilityMode::kAllChunks;
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
 * @brief Last measured comparison between two chunk-size builds.
 */
struct WorkspaceChunkSizeComparison {
    bool available = false;
    int before_chunk_size = 0;
    int after_chunk_size = 0;
    int before_total_chunks = 0;
    int after_total_chunks = 0;
    std::uint64_t before_draw_models = 0;
    std::uint64_t after_draw_models = 0;
    std::uint64_t before_active_faces = 0;
    std::uint64_t after_active_faces = 0;

    /**
     * @brief Returns relative draw-model count change after switching chunk size.
     *
     * Negative values mean fewer renderer models. Positive values mean more
     * renderer models. Zero is returned when the baseline is unavailable.
     *
     * @return Relative change from before_draw_models to after_draw_models.
     */
    [[nodiscard]] double DrawModelDeltaRatio() const;

    /**
     * @brief Returns relative active-face count change after switching chunk size.
     *
     * Negative values mean fewer active faces. Positive values mean more active
     * faces. Zero is returned when the baseline is unavailable.
     *
     * @return Relative change from before_active_faces to after_active_faces.
     */
    [[nodiscard]] double FaceDeltaRatio() const;
};

/**
 * @brief Runtime state for the main workspace screen.
 */
struct WorkspaceState {
    WorkspacePanelTab selected_panel_tab = WorkspacePanelTab::kMenu;
    WorkspaceTool selected_tool = WorkspaceTool::kMode;
    bool selected_tool_expanded = true;
    bool show_terrain_layer = true;
    bool show_elevation_layer = false;
    bool show_collision_layer = false;
    bool show_grid_layer = false;
    bool show_3d_preview = false;
    bool show_3d_chunk_bounds = false;
    bool show_3d_world_grid = false;
    bool show_3d_collision_overlay = false;
    bool show_3d_height_overlay = false;
    WorkspaceColorMode color_mode = WorkspaceColorMode::kMaterial;
    WorkspaceVisibilityMode visibility_mode = WorkspaceVisibilityMode::kAllChunks;
    int visibility_radius_chunks = 2;
    int visibility_fade_ring_chunks = 1;
    bool show_3d_hidden_chunk_bounds = false;
    bool show_terrain_tops = true;
    bool show_terrain_walls = true;
    bool show_terrain_cliffs = true;
    bool show_transition_overlay = false;
    bool show_transition_ramps = true;
    bool show_transition_stairs = true;
    bool show_transition_bridges = true;
    bool show_transition_drops = true;
    WorkspaceVisibilityStats visibility_stats;
    int chunk_size_tiles = 16;
    WorkspaceChunkSizeComparison chunk_size_comparison;
    MapPackageInfo map;
    RuntimeMap runtime_map;
    ChunkGrid chunk_grid;
    VoxelWorld voxel_world;
    FaceVisibilityResult face_visibility;
    ChunkMeshBuildMode mesh_mode = ChunkMeshBuildMode::kSimpleFaces;
    ChunkMeshCache simple_chunk_mesh_cache;
    ChunkMeshCache greedy_chunk_mesh_cache;
    ChunkMeshCache chunk_mesh_cache;
    ChunkMeshBuildResult simple_chunk_meshes;
    ChunkMeshBuildResult greedy_chunk_meshes;
    ChunkMeshBuildResult terrain_chunk_meshes;
    ChunkMeshBuildResult chunk_meshes;
    TransitionFeatureSet transition_features;
    MeshOptimizationStats mesh_stats;
    ChunkMeshRebuildReport last_mesh_rebuild;
};

/**
 * @brief Converts a workspace tool identifier to a stable lowercase name.
 *
 * @param tool Workspace tool identifier.
 * @return String representation.
 */
[[nodiscard]] std::string_view ToString(WorkspaceTool tool);

/**
 * @brief Converts a workspace panel tab identifier to a stable lowercase name.
 *
 * @param tab Workspace panel tab identifier.
 * @return String representation.
 */
[[nodiscard]] std::string_view ToString(WorkspacePanelTab tab);

/**
 * @brief Converts a workspace color mode identifier to a stable lowercase name.
 *
 * @param mode Color mode identifier.
 * @return String representation.
 */
[[nodiscard]] std::string_view ToString(WorkspaceColorMode mode);

/**
 * @brief Converts a workspace visibility mode identifier to a stable lowercase name.
 *
 * @param mode Visibility mode identifier.
 * @return String representation.
 */
[[nodiscard]] std::string_view ToString(WorkspaceVisibilityMode mode);

/**
 * @brief Converts a workspace panel item identifier to a stable lowercase name.
 *
 * @param item Workspace panel item identifier.
 * @return String representation.
 */
[[nodiscard]] std::string_view ToString(WorkspacePanelItem item);

/**
 * @brief Builds the visible tree rows for the currently selected workspace section.
 *
 * @param workspace Workspace state.
 * @return Rows shown under the selected workspace section.
 */
[[nodiscard]] std::vector<WorkspacePanelItemState> BuildWorkspacePanelItems(const WorkspaceState& workspace);

}  // namespace vox3d
