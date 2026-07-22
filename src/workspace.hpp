#pragma once

#include "vox3d/chunk/chunk_grid.hpp"
#include "vox3d/inspect/map_inspector.hpp"
#include "vox3d/map/map_package.hpp"
#include "vox3d/map/runtime_map.hpp"
#include "vox3d/mesh/face_visibility.hpp"
#include "vox3d/mesh/chunk_mesh_cache.hpp"
#include "vox3d/mesh/mesh_data.hpp"
#include "vox3d/movement/movement_probe.hpp"
#include "vox3d/path/path_probe.hpp"
#include "vox3d/transition/transition_feature.hpp"
#include "vox3d/validation/passability_validator.hpp"
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
    kTraversal,
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
 * @brief Execution policy for expensive map-wide validation passes.
 */
enum class WorkspaceValidationMode {
    kOff,
    kManual,
    kOnLoad,
};

/**
 * @brief Cached passability validation report state shown in the workspace UI.
 */
enum class WorkspaceValidationStatus {
    kDisabled,
    kNotRun,
    kDone,
};

/**
 * @brief Current modal state of the two-click path picking workflow.
 */
enum class WorkspacePathPickMode {
    kSelect,
    kPickStart,
    kPickGoal,
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
    kLayerMovementCost,
    kLayerProjectileBlock,
    kLayerVisionBlock,
    kLayerCover,
    kLayerConcealment,
    k2DOverlayGroup,
    kLayerGrid,
    k2DChunks,
    k2DVxmapRegions,
    k2DStartGoal,
    k2DObjects,
    k2DPlaces,
    k2DMarkers,
    k2DRoutes,
    k2DWorldGraph,
    k2DGameplayZones,
    k2DElevationFeatures,
    k2DElevationTransitions,

    k3DRenderGroup,
    kRenderTerrainMesh,
    k3DColorModeGroup,
    k3DColorTraversal,
    k3DColorGeographic,
    k3DColorChunkId,
    k3DColorFaceType,
    k3DDebugOverlaysGroup,
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
    k3DMovementGroup,
    k3DShowMovementProbe,
    k3DPathGroup,
    k3DPathProfileShortest,
    k3DPathProfileSafe,
    k3DPathStatusValue,
    k3DPathStartValue,
    k3DPathGoalValue,
    k3DPathToolSelect,
    k3DPathToolPickStart,
    k3DPathToolPickGoal,
    k3DRunPathProbe,
    k3DClearPathProbe,
    k3DShowPath,
    k3DShowPathVisited,
    k3DValidationGroup,
    k3DValidationModeOff,
    k3DValidationModeManual,
    k3DValidationModeOnLoad,
    k3DRunPassabilityValidation,
    k3DClearPassabilityValidation,
    k3DShowPassabilityIssues,
    k3DValidationInvalidTransitions,
    k3DValidationBlockedTransitions,
    k3DValidationSuspiciousDrops,
    k3DValidationIsolatedTiles,
    kRenderChunkBounds,
    kRenderWorldGrid,
    kRenderCollision,
    kRenderHeight,
    k3DObjectsGroup,
    k3DObjectsAll,
    k3DObjectsTrees,
    k3DObjectsBushes,
    k3DObjectsReeds,
    k3DObjectsRuins,
    k3DObjectsCover,
    k3DObjectsLoot,
    k3DObjectsStructures,
    k3DObjectsTrenches,
    k3DObjectsUnknown,
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

    kStatsMapGroup,
    kStatsColorGroup,
    kStatsVisibilityGroup,
    kStatsTransitionsGroup,
    kStatsMovementGroup,
    kStatsPathGroup,
    kStatsPassabilityGroup,
    kStatsMeshGroup,
    kStatsComparisonGroup,
    kStatsChunkProfitGroup,
    kStatsDirtyCacheGroup,
    kStatsCameraGroup,

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
    std::vector<WorkspacePanelItem> collapsed_panel_groups{
        WorkspacePanelItem::kMenuModeGroup,
        WorkspacePanelItem::k2DNavigationGroup,
        WorkspacePanelItem::k2DBaseLayerGroup,
        WorkspacePanelItem::k2DOverlayGroup,
        WorkspacePanelItem::k3DRenderGroup,
        WorkspacePanelItem::k3DVisibilityGroup,
        WorkspacePanelItem::k3DTerrainPassGroup,
        WorkspacePanelItem::k3DObjectsGroup,
        WorkspacePanelItem::k3DTransitionGroup,
        WorkspacePanelItem::k3DMovementGroup,
        WorkspacePanelItem::k3DPathGroup,
        WorkspacePanelItem::k3DValidationGroup,
        WorkspacePanelItem::k3DMeshGroup,
        WorkspacePanelItem::kStatsMapGroup,
        WorkspacePanelItem::kStatsColorGroup,
        WorkspacePanelItem::kStatsVisibilityGroup,
        WorkspacePanelItem::kStatsTransitionsGroup,
        WorkspacePanelItem::kStatsMovementGroup,
        WorkspacePanelItem::kStatsPathGroup,
        WorkspacePanelItem::kStatsPassabilityGroup,
        WorkspacePanelItem::kStatsMeshGroup,
        WorkspacePanelItem::kStatsComparisonGroup,
        WorkspacePanelItem::kStatsChunkProfitGroup,
        WorkspacePanelItem::kStatsDirtyCacheGroup,
        WorkspacePanelItem::kStatsCameraGroup,
    };
    Map2DBaseLayer map_2d_base_layer = Map2DBaseLayer::kTerrain;
    bool show_grid_layer = false;
    bool show_2d_chunks = false;
    bool show_2d_vxmap_regions = false;
    bool show_2d_start_goal = false;
    bool show_3d_preview = true;
    bool show_3d_chunk_bounds = false;
    bool show_3d_world_grid = false;
    bool show_3d_collision_overlay = false;
    bool show_3d_height_overlay = false;
    bool show_3d_object_trees = false;
    bool show_3d_object_bushes = false;
    bool show_3d_object_reeds = false;
    bool show_3d_object_ruins = false;
    bool show_3d_object_cover = false;
    bool show_3d_object_loot = false;
    bool show_3d_object_structures = false;
    bool show_3d_object_trenches = false;
    bool show_3d_object_unknown = false;
    WorkspaceColorMode color_mode = WorkspaceColorMode::kGeographic;
    WorkspaceVisibilityMode visibility_mode = WorkspaceVisibilityMode::kFrustumCull;
    int visibility_radius_chunks = 2;
    int visibility_fade_ring_chunks = 1;
    bool show_3d_hidden_chunk_bounds = false;
    bool show_terrain_tops = true;
    bool show_terrain_walls = true;
    bool show_terrain_cliffs = true;
    bool show_transition_overlay = false;
    int menu_scroll_rows = 0;
    bool show_transition_ramps = true;
    bool show_transition_stairs = true;
    bool show_transition_bridges = true;
    bool show_transition_drops = true;
    bool show_movement_probe = true;
    PathProfile path_profile = PathProfile::kShortest;
    WorkspacePathPickMode path_pick_mode = WorkspacePathPickMode::kSelect;
    bool has_path_start = false;
    bool has_path_goal = false;
    TileCoord path_start;
    TileCoord path_goal;
    bool show_path_overlay = true;
    bool show_path_visited = false;
    PathProbeResult path_probe;
    WorkspaceValidationMode validation_mode = WorkspaceValidationMode::kManual;
    WorkspaceValidationStatus passability_validation_status = WorkspaceValidationStatus::kNotRun;
    double passability_validation_last_run_ms = 0.0;
    bool passability_validation_dirty = true;
    bool show_passability_issues = false;
    bool show_passability_invalid_transitions = true;
    bool show_passability_blocked_transitions = true;
    bool show_passability_suspicious_drops = true;
    bool show_passability_isolated_tiles = true;
    TileInspectResult selected_tile;
    MovementProbeResult movement_probe;
    PassabilityValidationReport passability_validation;
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
    bool progressive_build_enabled = false;
    bool progressive_build_complete = false;
    int progressive_build_per_frame = 0;
    std::uint64_t progressive_chunks_total = 0;
    std::uint64_t progressive_chunks_built = 0;
    std::uint64_t progressive_chunks_pending = 0;
    float progressive_log_timer = 0.0F;
    float progressive_budget_wait_timer = 0.0F;
    std::vector<std::size_t> progressive_pending_chunks;
    std::vector<std::uint8_t> progressive_built_chunks;
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
 * @brief Converts a validation execution mode to a stable lowercase name.
 *
 * @param mode Validation execution mode.
 * @return String representation.
 */
[[nodiscard]] std::string_view ToString(WorkspaceValidationMode mode);

/**
 * @brief Converts a validation report status to a stable lowercase name.
 *
 * @param status Validation report status.
 * @return String representation.
 */
[[nodiscard]] std::string_view ToString(WorkspaceValidationStatus status);

/**
 * @brief Converts a path endpoint picking mode to a stable lowercase name.
 *
 * @param mode Path endpoint picking mode.
 * @return String representation.
 */
[[nodiscard]] std::string_view ToString(WorkspacePathPickMode mode);

/**
 * @brief Converts a workspace panel item identifier to a stable lowercase name.
 *
 * @param item Workspace panel item identifier.
 * @return String representation.
 */
[[nodiscard]] std::string_view ToString(WorkspacePanelItem item);

/**
 * @brief Checks whether a workspace panel row is a collapsible top-level group.
 *
 * Only top-level menu groups can be collapsed by the user. Nested group rows are
 * visual separators and are intentionally not interactive.
 *
 * @param item Workspace panel item identifier.
 * @return True when the item is a collapsible top-level group.
 */
[[nodiscard]] bool IsCollapsibleWorkspacePanelGroup(WorkspacePanelItem item);

/**
 * @brief Checks whether a collapsible workspace panel group is currently closed.
 *
 * Unknown or non-collapsible items are treated as expanded. The state is
 * runtime-only and is not persisted in user configuration.
 *
 * @param workspace Workspace runtime state.
 * @param item Workspace panel group identifier.
 * @return True when the group is collapsed.
 */
[[nodiscard]] bool IsWorkspacePanelGroupCollapsed(const WorkspaceState& workspace, WorkspacePanelItem item);

/**
 * @brief Toggles a collapsible workspace panel group in runtime state.
 *
 * Non-collapsible items are ignored. Collapsed state is stored only for the
 * current application run.
 *
 * @param workspace Workspace runtime state to mutate. Must not be nullptr.
 * @param item Workspace panel group identifier.
 */
void ToggleWorkspacePanelGroup(WorkspaceState* workspace, WorkspacePanelItem item);

/**
 * @brief Builds the visible tree rows for the currently selected workspace section.
 *
 * @param workspace Workspace state.
 * @return Rows shown under the selected workspace section.
 */
[[nodiscard]] std::vector<WorkspacePanelItemState> BuildWorkspacePanelItems(const WorkspaceState& workspace);

}  // namespace vox3d
