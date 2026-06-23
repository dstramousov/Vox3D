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

[[nodiscard]] WorkspacePanelItemState Value(Item item, int depth, bool enabled)
{
    return {item, Kind::kValue, depth, enabled, enabled};
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
        case WorkspacePanelItem::k3DDrawModels:
            return "3d_draw_models";
        case WorkspacePanelItem::k3DVisibleFaces:
            return "3d_visible_faces";
        case WorkspacePanelItem::k3DCulledFaces:
            return "3d_culled_faces";
        case WorkspacePanelItem::k3DGreedySaved:
            return "3d_greedy_saved";
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
    switch (workspace.selected_tool) {
        case WorkspaceTool::kMode:
            return {
                Radio(Item::kMode2DMap, 1, true, !workspace.show_3d_preview),
                Radio(Item::kMode3DWorld, 1, workspace.chunk_meshes.IsValid(), workspace.show_3d_preview),
            };
        case WorkspaceTool::kMap2D:
            return {
                Group(Item::k2DNavigationGroup, 1),
                Action(Item::k2DFitView, 2, false),
                Action(Item::k2DResetView, 2, false),
                Action(Item::k2DZoomIn, 2, false),
                Action(Item::k2DZoomOut, 2, false),
                Group(Item::k2DBaseLayerGroup, 1),
                Checkbox(Item::kLayerTerrain, 2, workspace.runtime_map.info.terrain_loaded, workspace.show_terrain_layer),
                Checkbox(Item::kLayerElevation, 2, workspace.runtime_map.info.elevation_loaded, workspace.show_elevation_layer),
                Checkbox(Item::kLayerCollision, 2, workspace.runtime_map.info.collision_loaded, workspace.show_collision_layer),
                Group(Item::k2DOverlayGroup, 1),
                Checkbox(Item::kLayerGrid, 2, true, workspace.show_grid_layer),
                Checkbox(Item::k2DChunks, 2, false, false),
                Checkbox(Item::k2DStartGoal, 2, false, false),
                Checkbox(Item::k2DObjects, 2, false, false),
                Checkbox(Item::k2DPlaces, 2, false, false),
                Checkbox(Item::k2DMarkers, 2, false, false),
                Checkbox(Item::k2DRoutes, 2, false, false),
                Checkbox(Item::k2DWorldGraph, 2, false, false),
                Checkbox(Item::k2DGameplayZones, 2, false, false),
                Checkbox(Item::k2DElevationFeatures, 2, false, false),
                Checkbox(Item::k2DElevationTransitions, 2, false, false),
            };
        case WorkspaceTool::kWorld3D:
            return {
                Group(Item::k3DCameraGroup, 1),
                Action(Item::kViewFitMap, 2, workspace.show_3d_preview && workspace.chunk_meshes.IsValid()),
                Action(Item::kViewResetView, 2, workspace.show_3d_preview && workspace.chunk_meshes.IsValid()),
                Action(Item::k3DCaptureMouse, 2, false),
                Action(Item::k3DReleaseMouse, 2, false),
                Group(Item::k3DRenderGroup, 1),
                Checkbox(Item::kRenderTerrainMesh, 2, workspace.chunk_meshes.IsValid(), workspace.show_3d_preview),
                Checkbox(Item::kRenderChunkBounds, 2, workspace.show_3d_preview && workspace.chunk_grid.IsValid(), workspace.show_3d_chunk_bounds),
                Checkbox(Item::kRenderWorldGrid, 2, workspace.show_3d_preview && workspace.chunk_meshes.IsValid(), workspace.show_3d_world_grid),
                Checkbox(Item::kRenderCollision, 2, workspace.show_3d_preview && workspace.runtime_map.info.collision_loaded, workspace.show_3d_collision_overlay),
                Checkbox(Item::kRenderHeight, 2, workspace.show_3d_preview && workspace.runtime_map.info.elevation_loaded, workspace.show_3d_height_overlay),
                Group(Item::k3DMeshGroup, 1),
                Group(Item::k3DChunkSizeGroup, 2),
                Radio(Item::k3DChunkSize16, 3, workspace.runtime_map.IsValid(), workspace.chunk_size_tiles == 16),
                Radio(Item::k3DChunkSize32, 3, workspace.runtime_map.IsValid(), workspace.chunk_size_tiles == 32),
                Value(Item::k3DChunkSizeProfit, 3, workspace.chunk_size_comparison.available),
                Radio(Item::k3DMeshSimple, 2, workspace.simple_chunk_meshes.IsValid(), workspace.mesh_mode == ChunkMeshBuildMode::kSimpleFaces),
                Radio(Item::k3DMeshGreedy, 2, workspace.greedy_chunk_meshes.IsValid(), workspace.mesh_mode == ChunkMeshBuildMode::kGreedyFaces),
                Value(Item::k3DDrawModels, 2, workspace.mesh_stats.draw_models > 0),
                Value(Item::k3DVisibleFaces, 2, workspace.face_visibility.IsValid()),
                Value(Item::k3DCulledFaces, 2, workspace.face_visibility.IsValid()),
                Value(Item::k3DGreedySaved, 2, workspace.greedy_chunk_meshes.IsValid()),
                Value(Item::k3DTotalSaved, 2, workspace.chunk_meshes.IsValid()),
                Value(Item::k3DChunkMeshes, 2, workspace.chunk_meshes.IsValid()),
                Action(Item::k3DDirtyRebuildProbe, 2, workspace.chunk_mesh_cache.IsValid()),
                Value(Item::k3DDirtyChunks, 2, workspace.chunk_mesh_cache.IsValid()),
                Value(Item::k3DRebuiltChunks, 2, workspace.last_mesh_rebuild.attempted),
                Value(Item::k3DRebuildSaved, 2, workspace.last_mesh_rebuild.attempted),
            };
        case WorkspaceTool::kSelection:
            return {
                Group(Item::kSelectionTileGroup, 1),
                Value(Item::kSelectionTileInfo, 2, false),
                Group(Item::kSelectionVoxelGroup, 1),
                Value(Item::kSelectionVoxelInfo, 2, false),
                Group(Item::kSelectionChunkGroup, 1),
                Value(Item::kSelectionChunkInfo, 2, false),
                Group(Item::kSelectionActionsGroup, 1),
                Action(Item::kSelectionInspect, 2, false),
                Action(Item::kSelectionFocus, 2, false),
                Action(Item::kSelectionCopyInfo, 2, false),
            };
        case WorkspaceTool::kPackageData:
            return {
                Group(Item::kPackageMetadataGroup, 1),
                Value(Item::kMapPackage, 2, workspace.map.loaded),
                Value(Item::kMapValidate, 2, workspace.runtime_map.HasCoreGrids()),
                Group(Item::kPackageRuntimeGridsGroup, 1),
                Value(Item::kPackageHeightGrid, 2, workspace.runtime_map.info.elevation_loaded),
                Value(Item::kPackageCollisionGrid, 2, workspace.runtime_map.info.collision_loaded),
                Value(Item::kPackageMovementCostGrid, 2, false),
                Group(Item::kPackageWorldDataGroup, 1),
                Value(Item::kPackageObjects, 2, workspace.map.object_count.value_or(0) > 0),
                Value(Item::kPackageMarkers, 2, workspace.map.marker_count.value_or(0) > 0),
                Value(Item::kPackageRoutes, 2, false),
                Value(Item::kPackageGameplayZones, 2, false),
            };
        case WorkspaceTool::kDebug:
            return {
                Value(Item::kDebugRuntimeMap, 1, workspace.runtime_map.IsValid()),
                Value(Item::kDebugChunkGrid, 1, workspace.chunk_grid.IsValid()),
                Value(Item::kDebugVoxelWorld, 1, workspace.voxel_world.IsValid()),
                Value(Item::kDebugFaceVisibility, 1, workspace.face_visibility.IsValid()),
                Value(Item::kDebugChunkMesh, 1, workspace.chunk_meshes.IsValid()),
                Value(Item::kDebugCamera, 1, workspace.show_3d_preview),
                Value(Item::kDebugMemory, 1, true),
                Value(Item::kDebugFps, 1, true),
                Value(Item::kDebugLogs, 1, false),
            };
        case WorkspaceTool::kSettings:
            return {
                Action(Item::kSettingsLanguage, 1, false),
                Action(Item::kSettingsCamera, 1, false),
                Action(Item::kSettingsRender, 1, false),
            };
    }
    return {};
}

}  // namespace vox3d
