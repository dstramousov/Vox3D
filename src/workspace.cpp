#include "workspace.hpp"

namespace vox3d {
namespace {

using Item = WorkspacePanelItem;
using Kind = WorkspacePanelItemKind;

[[nodiscard]] WorkspacePanelItemState Group(Item item, int depth)
{
    return {item, Kind::kGroup, depth, false, false};
}

[[nodiscard]] WorkspacePanelItemState Action(Item item, int depth, bool enabled)
{
    return {item, Kind::kAction, depth, enabled, false};
}

[[nodiscard]] WorkspacePanelItemState Checkbox(Item item, int depth, bool enabled, bool checked)
{
    return {item, Kind::kCheckbox, depth, enabled, checked};
}

[[nodiscard]] WorkspacePanelItemState Radio(Item item, int depth, bool enabled, bool checked)
{
    return {item, Kind::kRadio, depth, enabled, checked};
}

[[nodiscard]] double DeltaRatio(std::uint64_t before, std::uint64_t after)
{
    if (before == 0) {
        return 0.0;
    }
    return (static_cast<double>(after) - static_cast<double>(before)) / static_cast<double>(before);
}

}  // namespace

std::string_view ToString(WorkspaceTool tool)
{
    switch (tool) {
        case WorkspaceTool::kMode:
            return "mode";
        case WorkspaceTool::kMap2D:
            return "2d_map";
        case WorkspaceTool::kWorld3D:
            return "3d_world";
        case WorkspaceTool::kSelection:
            return "selection";
        case WorkspaceTool::kPackageData:
            return "package_data";
        case WorkspaceTool::kDebug:
            return "debug";
        case WorkspaceTool::kSettings:
            return "settings";
    }
    return "unknown";
}

std::string_view ToString(WorkspacePanelTab tab)
{
    switch (tab) {
        case WorkspacePanelTab::kMenu:
            return "menu";
        case WorkspacePanelTab::kStats:
            return "stats";
        case WorkspacePanelTab::kInspect:
            return "inspect";
        case WorkspacePanelTab::kHelp:
            return "help";
    }
    return "unknown";
}

std::string_view ToString(WorkspaceColorMode mode)
{
    switch (mode) {
        case WorkspaceColorMode::kMaterial:
            return "material";
        case WorkspaceColorMode::kGeographic:
            return "geographic";
        case WorkspaceColorMode::kChunkId:
            return "chunk_id";
        case WorkspaceColorMode::kFaceType:
            return "face_type";
    }
    return "unknown";
}

std::string_view ToString(WorkspaceVisibilityMode mode)
{
    switch (mode) {
        case WorkspaceVisibilityMode::kAllChunks:
            return "all_chunks";
        case WorkspaceVisibilityMode::kRadiusFade:
            return "radius_fade";
        case WorkspaceVisibilityMode::kHardCull:
            return "hard_cull";
        case WorkspaceVisibilityMode::kFrustumCull:
            return "frustum_cull";
    }
    return "unknown";
}

std::string_view ToString(WorkspaceValidationMode mode)
{
    switch (mode) {
        case WorkspaceValidationMode::kOff:
            return "off";
        case WorkspaceValidationMode::kManual:
            return "manual";
        case WorkspaceValidationMode::kOnLoad:
            return "on_load";
    }
    return "unknown";
}

std::string_view ToString(WorkspaceValidationStatus status)
{
    switch (status) {
        case WorkspaceValidationStatus::kDisabled:
            return "disabled";
        case WorkspaceValidationStatus::kNotRun:
            return "not_run";
        case WorkspaceValidationStatus::kDone:
            return "done";
    }
    return "unknown";
}

double WorkspaceVisibilityStats::DrawSavedRatio() const
{
    return resident_models == 0 ? 0.0 : static_cast<double>(culled_models) / static_cast<double>(resident_models);
}

double WorkspaceVisibilityStats::FaceSavedRatio() const
{
    return total_faces == 0 ? 0.0 : static_cast<double>(culled_faces) / static_cast<double>(total_faces);
}

double WorkspaceChunkSizeComparison::DrawModelDeltaRatio() const
{
    return available ? DeltaRatio(before_draw_models, after_draw_models) : 0.0;
}

double WorkspaceChunkSizeComparison::FaceDeltaRatio() const
{
    return available ? DeltaRatio(before_active_faces, after_active_faces) : 0.0;
}

std::string_view ToString(WorkspacePanelItem item)
{
    switch (item) {
        case WorkspacePanelItem::kMenuModeGroup:
            return "menu_mode";
        case WorkspacePanelItem::kMode2DMap:
            return "mode_2d_map";
        case WorkspacePanelItem::kMode3DWorld:
            return "mode_3d_world";
        case WorkspacePanelItem::k2DNavigationGroup:
            return "2d_navigation";
        case WorkspacePanelItem::k2DFitView:
            return "2d_fit_view";
        case WorkspacePanelItem::k2DResetView:
            return "2d_reset_view";
        case WorkspacePanelItem::k2DZoomIn:
            return "2d_zoom_in";
        case WorkspacePanelItem::k2DZoomOut:
            return "2d_zoom_out";
        case WorkspacePanelItem::k2DBaseLayerGroup:
            return "2d_base_layer";
        case WorkspacePanelItem::kLayerTerrain:
            return "layer_terrain";
        case WorkspacePanelItem::kLayerElevation:
            return "layer_elevation";
        case WorkspacePanelItem::kLayerCollision:
            return "layer_collision";
        case WorkspacePanelItem::k2DOverlayGroup:
            return "2d_overlays";
        case WorkspacePanelItem::kLayerGrid:
            return "layer_grid";
        case WorkspacePanelItem::k2DChunks:
            return "2d_chunks";
        case WorkspacePanelItem::k2DStartGoal:
            return "2d_start_goal";
        case WorkspacePanelItem::k2DObjects:
            return "2d_objects";
        case WorkspacePanelItem::k2DPlaces:
            return "2d_places";
        case WorkspacePanelItem::k2DMarkers:
            return "2d_markers";
        case WorkspacePanelItem::k2DRoutes:
            return "2d_routes";
        case WorkspacePanelItem::k2DWorldGraph:
            return "2d_world_graph";
        case WorkspacePanelItem::k2DGameplayZones:
            return "2d_gameplay_zones";
        case WorkspacePanelItem::k2DElevationFeatures:
            return "2d_elevation_features";
        case WorkspacePanelItem::k2DElevationTransitions:
            return "2d_elevation_transitions";
        case WorkspacePanelItem::k3DCameraGroup:
            return "3d_camera";
        case WorkspacePanelItem::kViewFitMap:
            return "3d_fit_map";
        case WorkspacePanelItem::kViewResetView:
            return "3d_reset_view";
        case WorkspacePanelItem::k3DCaptureMouse:
            return "3d_capture_mouse";
        case WorkspacePanelItem::k3DReleaseMouse:
            return "3d_release_mouse";
        case WorkspacePanelItem::k3DRenderGroup:
            return "3d_render";
        case WorkspacePanelItem::kRenderTerrainMesh:
            return "3d_terrain_mesh";
        case WorkspacePanelItem::k3DColorModeGroup:
            return "3d_color_mode";
        case WorkspacePanelItem::k3DColorMaterial:
            return "3d_color_material";
        case WorkspacePanelItem::k3DColorGeographic:
            return "3d_color_geographic";
        case WorkspacePanelItem::k3DColorChunkId:
            return "3d_color_chunk_id";
        case WorkspacePanelItem::k3DColorFaceType:
            return "3d_color_face_type";
        case WorkspacePanelItem::k3DVisibilityGroup:
            return "3d_visibility";
        case WorkspacePanelItem::k3DVisibilityAllChunks:
            return "3d_visibility_all_chunks";
        case WorkspacePanelItem::k3DVisibilityRadiusFade:
            return "3d_visibility_radius_fade";
        case WorkspacePanelItem::k3DVisibilityHardCull:
            return "3d_visibility_hard_cull";
        case WorkspacePanelItem::k3DVisibilityFrustumCull:
            return "3d_visibility_frustum_cull";
        case WorkspacePanelItem::k3DVisibilityRadiusMinus:
            return "3d_visibility_radius_minus";
        case WorkspacePanelItem::k3DVisibilityRadiusPlus:
            return "3d_visibility_radius_plus";
        case WorkspacePanelItem::k3DVisibilityFadeMinus:
            return "3d_visibility_fade_minus";
        case WorkspacePanelItem::k3DVisibilityFadePlus:
            return "3d_visibility_fade_plus";
        case WorkspacePanelItem::k3DShowHiddenBounds:
            return "3d_show_hidden_bounds";
        case WorkspacePanelItem::k3DTerrainPassGroup:
            return "3d_terrain_passes";
        case WorkspacePanelItem::k3DTerrainPassTops:
            return "3d_terrain_pass_tops";
        case WorkspacePanelItem::k3DTerrainPassWalls:
            return "3d_terrain_pass_walls";
        case WorkspacePanelItem::k3DTerrainPassCliffs:
            return "3d_terrain_pass_cliffs";
        case WorkspacePanelItem::k3DTransitionGroup:
            return "3d_transitions";
        case WorkspacePanelItem::k3DShowTransitions:
            return "3d_show_transitions";
        case WorkspacePanelItem::k3DTransitionRamps:
            return "3d_transition_ramps";
        case WorkspacePanelItem::k3DTransitionStairs:
            return "3d_transition_stairs";
        case WorkspacePanelItem::k3DTransitionBridges:
            return "3d_transition_bridges";
        case WorkspacePanelItem::k3DTransitionDrops:
            return "3d_transition_drops";
        case WorkspacePanelItem::k3DMovementGroup:
            return "3d_movement";
        case WorkspacePanelItem::k3DShowMovementProbe:
            return "3d_show_movement_probe";
        case WorkspacePanelItem::k3DPathGroup:
            return "3d_path";
        case WorkspacePanelItem::k3DPathProfileShortest:
            return "3d_path_profile_shortest";
        case WorkspacePanelItem::k3DPathProfileSafe:
            return "3d_path_profile_safe";
        case WorkspacePanelItem::k3DSetSelectedAsPathStart:
            return "3d_path_set_selected_start";
        case WorkspacePanelItem::k3DSetSelectedAsPathGoal:
            return "3d_path_set_selected_goal";
        case WorkspacePanelItem::k3DRunPathProbe:
            return "3d_run_path_probe";
        case WorkspacePanelItem::k3DClearPathProbe:
            return "3d_clear_path_probe";
        case WorkspacePanelItem::k3DShowPath:
            return "3d_show_path";
        case WorkspacePanelItem::k3DShowPathVisited:
            return "3d_show_path_visited";
        case WorkspacePanelItem::k3DValidationGroup:
            return "3d_validation";
        case WorkspacePanelItem::k3DValidationModeOff:
            return "3d_validation_mode_off";
        case WorkspacePanelItem::k3DValidationModeManual:
            return "3d_validation_mode_manual";
        case WorkspacePanelItem::k3DValidationModeOnLoad:
            return "3d_validation_mode_on_load";
        case WorkspacePanelItem::k3DRunPassabilityValidation:
            return "3d_run_passability_validation";
        case WorkspacePanelItem::k3DClearPassabilityValidation:
            return "3d_clear_passability_validation";
        case WorkspacePanelItem::k3DShowPassabilityIssues:
            return "3d_show_passability_issues";
        case WorkspacePanelItem::k3DValidationInvalidTransitions:
            return "3d_validation_invalid_transitions";
        case WorkspacePanelItem::k3DValidationBlockedTransitions:
            return "3d_validation_blocked_transitions";
        case WorkspacePanelItem::k3DValidationSuspiciousDrops:
            return "3d_validation_suspicious_drops";
        case WorkspacePanelItem::k3DValidationIsolatedTiles:
            return "3d_validation_isolated_tiles";
        case WorkspacePanelItem::kRenderChunkBounds:
            return "render_chunk_bounds";
        case WorkspacePanelItem::kRenderWorldGrid:
            return "render_world_grid";
        case WorkspacePanelItem::kRenderCollision:
            return "render_collision";
        case WorkspacePanelItem::kRenderHeight:
            return "render_height";
        case WorkspacePanelItem::k3DMeshGroup:
            return "3d_mesh";
        case WorkspacePanelItem::k3DChunkSizeGroup:
            return "3d_chunk_size";
        case WorkspacePanelItem::k3DChunkSize16:
            return "3d_chunk_size_16";
        case WorkspacePanelItem::k3DChunkSize32:
            return "3d_chunk_size_32";
        case WorkspacePanelItem::k3DChunkSizeProfit:
            return "3d_chunk_size_profit";
        case WorkspacePanelItem::k3DMeshSimple:
            return "3d_mesh_simple";
        case WorkspacePanelItem::k3DMeshGreedy:
            return "3d_mesh_greedy";
        case WorkspacePanelItem::k3DMeshTerrainSurface:
            return "3d_mesh_terrain_surface";
        case WorkspacePanelItem::k3DDrawModels:
            return "3d_draw_models";
        case WorkspacePanelItem::k3DVisibleFaces:
            return "3d_visible_faces";
        case WorkspacePanelItem::k3DCulledFaces:
            return "3d_culled_faces";
        case WorkspacePanelItem::k3DGreedySaved:
            return "3d_greedy_saved";
        case WorkspacePanelItem::k3DTerrainFaces:
            return "3d_terrain_faces";
        case WorkspacePanelItem::k3DTerrainTopFaces:
            return "3d_terrain_top_faces";
        case WorkspacePanelItem::k3DTerrainWallFaces:
            return "3d_terrain_wall_faces";
        case WorkspacePanelItem::k3DTerrainVsGreedy:
            return "3d_terrain_vs_greedy";
        case WorkspacePanelItem::k3DTotalSaved:
            return "3d_total_saved";
        case WorkspacePanelItem::k3DChunkMeshes:
            return "3d_chunk_meshes";
        case WorkspacePanelItem::k3DDirtyRebuildProbe:
            return "3d_dirty_rebuild_probe";
        case WorkspacePanelItem::k3DDirtyChunks:
            return "3d_dirty_chunks";
        case WorkspacePanelItem::k3DRebuiltChunks:
            return "3d_rebuilt_chunks";
        case WorkspacePanelItem::k3DRebuildSaved:
            return "3d_rebuild_saved";
        case WorkspacePanelItem::kSelectionTileGroup:
            return "selection_tile";
        case WorkspacePanelItem::kSelectionTileInfo:
            return "selection_tile_info";
        case WorkspacePanelItem::kSelectionVoxelGroup:
            return "selection_voxel";
        case WorkspacePanelItem::kSelectionVoxelInfo:
            return "selection_voxel_info";
        case WorkspacePanelItem::kSelectionChunkGroup:
            return "selection_chunk";
        case WorkspacePanelItem::kSelectionChunkInfo:
            return "selection_chunk_info";
        case WorkspacePanelItem::kSelectionActionsGroup:
            return "selection_actions";
        case WorkspacePanelItem::kSelectionInspect:
            return "selection_inspect";
        case WorkspacePanelItem::kSelectionFocus:
            return "selection_focus";
        case WorkspacePanelItem::kSelectionCopyInfo:
            return "selection_copy_info";
        case WorkspacePanelItem::kPackageMetadataGroup:
            return "package_metadata";
        case WorkspacePanelItem::kMapPackage:
            return "map_package";
        case WorkspacePanelItem::kMapValidate:
            return "map_validate";
        case WorkspacePanelItem::kPackageRuntimeGridsGroup:
            return "package_runtime_grids";
        case WorkspacePanelItem::kPackageHeightGrid:
            return "package_height_grid";
        case WorkspacePanelItem::kPackageCollisionGrid:
            return "package_collision_grid";
        case WorkspacePanelItem::kPackageMovementCostGrid:
            return "package_movement_cost_grid";
        case WorkspacePanelItem::kPackageWorldDataGroup:
            return "package_world_data";
        case WorkspacePanelItem::kPackageObjects:
            return "package_objects";
        case WorkspacePanelItem::kPackageMarkers:
            return "package_markers";
        case WorkspacePanelItem::kPackageRoutes:
            return "package_routes";
        case WorkspacePanelItem::kPackageGameplayZones:
            return "package_gameplay_zones";
        case WorkspacePanelItem::kDebugRuntimeMap:
            return "debug_runtime_map";
        case WorkspacePanelItem::kDebugChunkGrid:
            return "debug_chunk_grid";
        case WorkspacePanelItem::kDebugVoxelWorld:
            return "debug_voxel_world";
        case WorkspacePanelItem::kDebugFaceVisibility:
            return "debug_face_visibility";
        case WorkspacePanelItem::kDebugChunkMesh:
            return "debug_chunk_mesh";
        case WorkspacePanelItem::kDebugCamera:
            return "debug_camera";
        case WorkspacePanelItem::kDebugMemory:
            return "debug_memory";
        case WorkspacePanelItem::kDebugFps:
            return "debug_fps";
        case WorkspacePanelItem::kDebugLogs:
            return "debug_logs";
        case WorkspacePanelItem::kSettingsLanguage:
            return "settings_language";
        case WorkspacePanelItem::kSettingsCamera:
            return "settings_camera";
        case WorkspacePanelItem::kSettingsRender:
            return "settings_render";
    }
    return "unknown";
}

std::vector<WorkspacePanelItemState> BuildWorkspacePanelItems(const WorkspaceState& workspace)
{
    if (workspace.selected_panel_tab != WorkspacePanelTab::kMenu) {
        return {};
    }

    std::vector<WorkspacePanelItemState> items{
        Group(Item::kMenuModeGroup, 0),
        Radio(Item::kMode2DMap, 1, true, !workspace.show_3d_preview),
        Radio(Item::kMode3DWorld, 1, workspace.chunk_meshes.IsValid(), workspace.show_3d_preview),
    };

    if (workspace.show_3d_preview) {
        items.push_back(Group(Item::k3DCameraGroup, 0));
        items.push_back(Action(Item::kViewFitMap, 1, workspace.chunk_meshes.IsValid()));
        items.push_back(Action(Item::kViewResetView, 1, workspace.chunk_meshes.IsValid()));

        items.push_back(Group(Item::k3DRenderGroup, 0));
        items.push_back(Checkbox(Item::kRenderChunkBounds, 1, workspace.chunk_grid.IsValid(), workspace.show_3d_chunk_bounds));
        items.push_back(Checkbox(Item::kRenderWorldGrid, 1, workspace.chunk_meshes.IsValid(), workspace.show_3d_world_grid));
        items.push_back(Checkbox(Item::kRenderCollision, 1, workspace.runtime_map.info.collision_loaded, workspace.show_3d_collision_overlay));
        items.push_back(Checkbox(Item::kRenderHeight, 1, workspace.runtime_map.info.elevation_loaded, workspace.show_3d_height_overlay));

        items.push_back(Group(Item::k3DColorModeGroup, 0));
        items.push_back(Radio(Item::k3DColorMaterial, 1, workspace.chunk_meshes.IsValid(), workspace.color_mode == WorkspaceColorMode::kMaterial));
        items.push_back(Radio(Item::k3DColorGeographic, 1, workspace.chunk_meshes.IsValid(), workspace.color_mode == WorkspaceColorMode::kGeographic));
        items.push_back(Radio(Item::k3DColorChunkId, 1, workspace.chunk_meshes.IsValid(), workspace.color_mode == WorkspaceColorMode::kChunkId));
        items.push_back(Radio(Item::k3DColorFaceType, 1, workspace.chunk_meshes.IsValid(), workspace.color_mode == WorkspaceColorMode::kFaceType));

        items.push_back(Group(Item::k3DVisibilityGroup, 0));
        items.push_back(Radio(Item::k3DVisibilityAllChunks, 1, workspace.chunk_meshes.IsValid(), workspace.visibility_mode == WorkspaceVisibilityMode::kAllChunks));
        items.push_back(Radio(Item::k3DVisibilityRadiusFade, 1, workspace.chunk_meshes.IsValid(), workspace.visibility_mode == WorkspaceVisibilityMode::kRadiusFade));
        items.push_back(Radio(Item::k3DVisibilityHardCull, 1, workspace.chunk_meshes.IsValid(), workspace.visibility_mode == WorkspaceVisibilityMode::kHardCull));
        items.push_back(Radio(Item::k3DVisibilityFrustumCull, 1, workspace.chunk_meshes.IsValid(), workspace.visibility_mode == WorkspaceVisibilityMode::kFrustumCull));
        items.push_back(Action(Item::k3DVisibilityRadiusMinus, 1, workspace.chunk_meshes.IsValid()));
        items.push_back(Action(Item::k3DVisibilityRadiusPlus, 1, workspace.chunk_meshes.IsValid()));
        items.push_back(Action(Item::k3DVisibilityFadeMinus, 1, workspace.chunk_meshes.IsValid()));
        items.push_back(Action(Item::k3DVisibilityFadePlus, 1, workspace.chunk_meshes.IsValid()));
        items.push_back(Checkbox(Item::k3DShowHiddenBounds, 1, workspace.chunk_meshes.IsValid(), workspace.show_3d_hidden_chunk_bounds));

        const bool transition_features_enabled = workspace.transition_features.IsValid()
            && !workspace.transition_features.features.empty();
        items.push_back(Group(Item::k3DTransitionGroup, 0));
        items.push_back(Checkbox(Item::k3DShowTransitions, 1, transition_features_enabled, workspace.show_transition_overlay));
        items.push_back(Checkbox(Item::k3DTransitionRamps, 1, transition_features_enabled, workspace.show_transition_ramps));
        items.push_back(Checkbox(Item::k3DTransitionStairs, 1, transition_features_enabled, workspace.show_transition_stairs));
        items.push_back(Checkbox(Item::k3DTransitionBridges, 1, transition_features_enabled, workspace.show_transition_bridges));
        items.push_back(Checkbox(Item::k3DTransitionDrops, 1, transition_features_enabled, workspace.show_transition_drops));

        const bool movement_probe_enabled = workspace.selected_tile.IsValid() && workspace.movement_probe.IsValid();
        items.push_back(Group(Item::k3DMovementGroup, 0));
        items.push_back(Checkbox(Item::k3DShowMovementProbe, 1, movement_probe_enabled, workspace.show_movement_probe));

        const bool path_can_run = workspace.runtime_map.IsValid() && workspace.transition_features.IsValid()
            && workspace.has_path_start && workspace.has_path_goal;
        const bool path_available = workspace.path_probe.IsValid();
        items.push_back(Group(Item::k3DPathGroup, 0));
        items.push_back(Radio(Item::k3DPathProfileShortest, 1, true, workspace.path_profile == PathProfile::kShortest));
        items.push_back(Radio(Item::k3DPathProfileSafe, 1, true, workspace.path_profile == PathProfile::kSafe));
        items.push_back(Action(Item::k3DSetSelectedAsPathStart, 1, workspace.selected_tile.IsValid()));
        items.push_back(Action(Item::k3DSetSelectedAsPathGoal, 1, workspace.selected_tile.IsValid()));
        items.push_back(Action(Item::k3DRunPathProbe, 1, path_can_run));
        items.push_back(Action(Item::k3DClearPathProbe, 1, path_available || workspace.has_path_start || workspace.has_path_goal));
        items.push_back(Checkbox(Item::k3DShowPath, 1, path_available, workspace.show_path_overlay));
        items.push_back(Checkbox(Item::k3DShowPathVisited, 1, path_available, workspace.show_path_visited));

        const bool validation_report_available = workspace.passability_validation.IsValid();
        const bool validation_enabled = validation_report_available && !workspace.passability_validation.issues.empty();
        const bool validation_can_run = workspace.runtime_map.IsValid() && workspace.transition_features.IsValid()
            && workspace.validation_mode != WorkspaceValidationMode::kOff;
        const bool validation_can_clear = validation_report_available
            || workspace.passability_validation_status == WorkspaceValidationStatus::kDone;
        items.push_back(Group(Item::k3DValidationGroup, 0));
        items.push_back(Radio(
            Item::k3DValidationModeOff,
            1,
            true,
            workspace.validation_mode == WorkspaceValidationMode::kOff));
        items.push_back(Radio(
            Item::k3DValidationModeManual,
            1,
            true,
            workspace.validation_mode == WorkspaceValidationMode::kManual));
        items.push_back(Radio(
            Item::k3DValidationModeOnLoad,
            1,
            true,
            workspace.validation_mode == WorkspaceValidationMode::kOnLoad));
        items.push_back(Action(Item::k3DRunPassabilityValidation, 1, validation_can_run));
        items.push_back(Action(Item::k3DClearPassabilityValidation, 1, validation_can_clear));
        items.push_back(Checkbox(Item::k3DShowPassabilityIssues, 1, validation_enabled, workspace.show_passability_issues));
        items.push_back(Checkbox(Item::k3DValidationInvalidTransitions, 1, validation_enabled, workspace.show_passability_invalid_transitions));
        items.push_back(Checkbox(Item::k3DValidationBlockedTransitions, 1, validation_enabled, workspace.show_passability_blocked_transitions));
        items.push_back(Checkbox(Item::k3DValidationSuspiciousDrops, 1, validation_enabled, workspace.show_passability_suspicious_drops));
        items.push_back(Checkbox(Item::k3DValidationIsolatedTiles, 1, validation_enabled, workspace.show_passability_isolated_tiles));

        const bool terrain_passes_enabled = workspace.mesh_mode == ChunkMeshBuildMode::kTerrainSurface
            && workspace.terrain_chunk_meshes.IsValid();
        items.push_back(Group(Item::k3DTerrainPassGroup, 0));
        items.push_back(Checkbox(Item::k3DTerrainPassTops, 1, terrain_passes_enabled, workspace.show_terrain_tops));
        items.push_back(Checkbox(Item::k3DTerrainPassWalls, 1, terrain_passes_enabled, workspace.show_terrain_walls));
        items.push_back(Checkbox(Item::k3DTerrainPassCliffs, 1, terrain_passes_enabled, workspace.show_terrain_cliffs));

        items.push_back(Group(Item::k3DMeshGroup, 0));
        items.push_back(Radio(Item::k3DMeshSimple, 1, workspace.simple_chunk_meshes.IsValid(), workspace.mesh_mode == ChunkMeshBuildMode::kSimpleFaces));
        items.push_back(Radio(Item::k3DMeshGreedy, 1, workspace.greedy_chunk_meshes.IsValid(), workspace.mesh_mode == ChunkMeshBuildMode::kGreedyFaces));
        items.push_back(Radio(Item::k3DMeshTerrainSurface, 1, workspace.terrain_chunk_meshes.IsValid(), workspace.mesh_mode == ChunkMeshBuildMode::kTerrainSurface));

        items.push_back(Group(Item::k3DChunkSizeGroup, 0));
        items.push_back(Radio(Item::k3DChunkSize16, 1, workspace.runtime_map.IsValid(), workspace.chunk_size_tiles == 16));
        items.push_back(Radio(Item::k3DChunkSize32, 1, workspace.runtime_map.IsValid(), workspace.chunk_size_tiles == 32));
        items.push_back(Action(Item::k3DDirtyRebuildProbe, 1, workspace.chunk_mesh_cache.IsValid() && workspace.mesh_mode != ChunkMeshBuildMode::kTerrainSurface));
        return items;
    }

    items.push_back(Group(Item::k2DNavigationGroup, 0));
    items.push_back(Action(Item::k2DFitView, 1, false));
    items.push_back(Action(Item::k2DResetView, 1, false));
    items.push_back(Action(Item::k2DZoomIn, 1, false));
    items.push_back(Action(Item::k2DZoomOut, 1, false));

    items.push_back(Group(Item::k2DBaseLayerGroup, 0));
    items.push_back(Checkbox(Item::kLayerTerrain, 1, workspace.runtime_map.info.terrain_loaded, workspace.show_terrain_layer));
    items.push_back(Checkbox(Item::kLayerElevation, 1, workspace.runtime_map.info.elevation_loaded, workspace.show_elevation_layer));
    items.push_back(Checkbox(Item::kLayerCollision, 1, workspace.runtime_map.info.collision_loaded, workspace.show_collision_layer));

    items.push_back(Group(Item::k2DOverlayGroup, 0));
    items.push_back(Checkbox(Item::kLayerGrid, 1, true, workspace.show_grid_layer));
    items.push_back(Checkbox(Item::k2DChunks, 1, false, false));
    items.push_back(Checkbox(Item::k2DStartGoal, 1, false, false));
    items.push_back(Checkbox(Item::k2DObjects, 1, false, false));
    items.push_back(Checkbox(Item::k2DPlaces, 1, false, false));
    items.push_back(Checkbox(Item::k2DMarkers, 1, false, false));
    items.push_back(Checkbox(Item::k2DRoutes, 1, false, false));
    return items;
}

}  // namespace vox3d
