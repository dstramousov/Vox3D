#pragma once

#include "vox3d/chunk/chunk_grid.hpp"
#include "vox3d/map/map_package.hpp"
#include "vox3d/map/runtime_map.hpp"
#include "vox3d/mesh/face_visibility.hpp"
#include "vox3d/voxel/voxel_world.hpp"

#include <string_view>
#include <vector>

namespace vox3d {

/**
 * @brief Top-level workspace section shown in the right-side accordion panel.
 */
enum class WorkspaceTool {
    kMap,
    kView,
    kLayers,
    kObjects,
    kRender,
    kDebug,
    kSettings,
};

/**
 * @brief Clickable item shown inside an expanded workspace accordion section.
 */
enum class WorkspacePanelItem {
    kMapOverview,
    kMapPackage,
    kMapValidate,
    kView2DMap,
    kView3DPreview,
    kViewFitMap,
    kViewResetView,
    kLayerTerrain,
    kLayerElevation,
    kLayerCollision,
    kLayerGrid,
    kRenderOverview,
    kRenderWire,
    kRenderHeight,
    kDebugMemory,
    kDebugFps,
    kDebugLogs,
    kSettingsLanguage,
};

/**
 * @brief Runtime state for one workspace accordion subitem.
 */
struct WorkspacePanelItemState {
    WorkspacePanelItem item = WorkspacePanelItem::kMapOverview;
    bool enabled = false;
    bool checked = false;
};

/**
 * @brief Runtime state for the main workspace screen.
 */
struct WorkspaceState {
    WorkspaceTool selected_tool = WorkspaceTool::kMap;
    bool selected_tool_expanded = true;
    bool show_terrain_layer = true;
    bool show_elevation_layer = false;
    bool show_collision_layer = false;
    bool show_grid_layer = false;
    MapPackageInfo map;
    RuntimeMap runtime_map;
    ChunkGrid chunk_grid;
    VoxelWorld voxel_world;
    FaceVisibilityResult face_visibility;
};

/**
 * @brief Converts a workspace tool identifier to a stable lowercase name.
 *
 * @param tool Workspace tool identifier.
 * @return String representation.
 */
[[nodiscard]] std::string_view ToString(WorkspaceTool tool);

/**
 * @brief Converts a workspace panel item identifier to a stable lowercase name.
 *
 * @param item Workspace panel item identifier.
 * @return String representation.
 */
[[nodiscard]] std::string_view ToString(WorkspacePanelItem item);

/**
 * @brief Builds the visible subitems for the currently selected accordion section.
 *
 * @param workspace Workspace state.
 * @return Subitems shown under the selected workspace section.
 */
[[nodiscard]] std::vector<WorkspacePanelItemState> BuildWorkspacePanelItems(const WorkspaceState& workspace);

}  // namespace vox3d
