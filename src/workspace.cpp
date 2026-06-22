#include "workspace.hpp"

namespace vox3d {

std::string_view ToString(WorkspaceTool tool)
{
    switch (tool) {
        case WorkspaceTool::kMap:
            return "map";
        case WorkspaceTool::kView:
            return "view";
        case WorkspaceTool::kLayers:
            return "layers";
        case WorkspaceTool::kObjects:
            return "objects";
        case WorkspaceTool::kRender:
            return "render";
        case WorkspaceTool::kDebug:
            return "debug";
        case WorkspaceTool::kSettings:
            return "settings";
    }
    return "unknown";
}

std::string_view ToString(WorkspacePanelItem item)
{
    switch (item) {
        case WorkspacePanelItem::kMapOverview:
            return "map_overview";
        case WorkspacePanelItem::kMapPackage:
            return "map_package";
        case WorkspacePanelItem::kMapValidate:
            return "map_validate";
        case WorkspacePanelItem::kView2DMap:
            return "view_2d_map";
        case WorkspacePanelItem::kView3DPreview:
            return "view_3d_preview";
        case WorkspacePanelItem::kViewFitMap:
            return "view_fit_map";
        case WorkspacePanelItem::kViewResetView:
            return "view_reset_view";
        case WorkspacePanelItem::kLayerTerrain:
            return "layer_terrain";
        case WorkspacePanelItem::kLayerElevation:
            return "layer_elevation";
        case WorkspacePanelItem::kLayerCollision:
            return "layer_collision";
        case WorkspacePanelItem::kLayerGrid:
            return "layer_grid";
        case WorkspacePanelItem::kRenderOverview:
            return "render_overview";
        case WorkspacePanelItem::kRenderChunkBounds:
            return "render_chunk_bounds";
        case WorkspacePanelItem::kRenderWorldGrid:
            return "render_world_grid";
        case WorkspacePanelItem::kRenderCollision:
            return "render_collision";
        case WorkspacePanelItem::kRenderHeight:
            return "render_height";
        case WorkspacePanelItem::kDebugMemory:
            return "debug_memory";
        case WorkspacePanelItem::kDebugFps:
            return "debug_fps";
        case WorkspacePanelItem::kDebugLogs:
            return "debug_logs";
        case WorkspacePanelItem::kSettingsLanguage:
            return "settings_language";
    }
    return "unknown";
}

std::vector<WorkspacePanelItemState> BuildWorkspacePanelItems(const WorkspaceState& workspace)
{
    switch (workspace.selected_tool) {
        case WorkspaceTool::kMap:
            return {
                {WorkspacePanelItem::kMapOverview, workspace.map.overview.IsValid(), workspace.map.overview.IsValid()},
                {WorkspacePanelItem::kMapPackage, workspace.map.loaded, workspace.map.loaded},
                {WorkspacePanelItem::kMapValidate, workspace.runtime_map.HasCoreGrids(), workspace.runtime_map.HasCoreGrids()},
            };
        case WorkspaceTool::kView:
            return {
                {WorkspacePanelItem::kView2DMap, true, !workspace.show_3d_preview},
                {WorkspacePanelItem::kView3DPreview, workspace.chunk_meshes.IsValid(), workspace.show_3d_preview},
                {WorkspacePanelItem::kViewFitMap, workspace.show_3d_preview && workspace.chunk_meshes.IsValid(), false},
                {WorkspacePanelItem::kViewResetView, workspace.show_3d_preview && workspace.chunk_meshes.IsValid(), false},
            };
        case WorkspaceTool::kLayers:
            return {
                {WorkspacePanelItem::kLayerTerrain, workspace.runtime_map.info.terrain_loaded, workspace.show_terrain_layer},
                {WorkspacePanelItem::kLayerElevation, workspace.runtime_map.info.elevation_loaded, workspace.show_elevation_layer},
                {WorkspacePanelItem::kLayerCollision, workspace.runtime_map.info.collision_loaded, workspace.show_collision_layer},
                {WorkspacePanelItem::kLayerGrid, true, workspace.show_grid_layer},
            };
        case WorkspaceTool::kObjects:
            return {
                {WorkspacePanelItem::kMapOverview, false, false},
                {WorkspacePanelItem::kMapPackage, false, false},
            };
        case WorkspaceTool::kRender:
            return {
                {WorkspacePanelItem::kRenderOverview, workspace.show_3d_preview && workspace.chunk_meshes.IsValid(), workspace.show_3d_preview},
                {WorkspacePanelItem::kRenderChunkBounds, workspace.show_3d_preview && workspace.chunk_grid.IsValid(), workspace.show_3d_chunk_bounds},
                {WorkspacePanelItem::kRenderWorldGrid, workspace.show_3d_preview && workspace.chunk_meshes.IsValid(), workspace.show_3d_world_grid},
                {WorkspacePanelItem::kRenderCollision, workspace.show_3d_preview && workspace.runtime_map.info.collision_loaded, workspace.show_3d_collision_overlay},
                {WorkspacePanelItem::kRenderHeight, workspace.show_3d_preview && workspace.runtime_map.info.elevation_loaded, workspace.show_3d_height_overlay},
            };
        case WorkspaceTool::kDebug:
            return {
                {WorkspacePanelItem::kDebugMemory, true, true},
                {WorkspacePanelItem::kDebugFps, true, true},
                {WorkspacePanelItem::kDebugLogs, false, false},
            };
        case WorkspaceTool::kSettings:
            return {
                {WorkspacePanelItem::kSettingsLanguage, false, false},
            };
    }
    return {};
}

}  // namespace vox3d
