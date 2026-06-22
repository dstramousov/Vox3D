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
        case WorkspacePanelItem::kRenderWire:
            return "render_wire";
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
                {WorkspacePanelItem::kMapValidate, false, false},
            };
        case WorkspaceTool::kView:
            return {
                {WorkspacePanelItem::kView2DMap, true, true},
                {WorkspacePanelItem::kView3DPreview, false, false},
                {WorkspacePanelItem::kViewFitMap, false, false},
                {WorkspacePanelItem::kViewResetView, false, false},
            };
        case WorkspaceTool::kLayers:
            return {
                {WorkspacePanelItem::kLayerTerrain, workspace.map.terrain_available, workspace.show_terrain_layer},
                {WorkspacePanelItem::kLayerElevation, workspace.map.elevation_available, workspace.show_elevation_layer},
                {WorkspacePanelItem::kLayerCollision, workspace.map.collision_available, workspace.show_collision_layer},
                {WorkspacePanelItem::kLayerGrid, true, workspace.show_grid_layer},
            };
        case WorkspaceTool::kObjects:
            return {
                {WorkspacePanelItem::kMapOverview, false, false},
                {WorkspacePanelItem::kMapPackage, false, false},
            };
        case WorkspaceTool::kRender:
            return {
                {WorkspacePanelItem::kRenderOverview, true, true},
                {WorkspacePanelItem::kRenderWire, false, false},
                {WorkspacePanelItem::kRenderHeight, false, false},
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
