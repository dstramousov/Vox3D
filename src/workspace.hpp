#pragma once

#include "vox3d/chunk/chunk_grid.hpp"
#include "vox3d/map/map_package.hpp"
#include "vox3d/map/runtime_map.hpp"
#include "vox3d/mesh/face_visibility.hpp"
#include "vox3d/mesh/mesh_data.hpp"
#include "vox3d/voxel/voxel_world.hpp"

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
 * @brief Clickable item shown inside an expanded workspace tree section.
 */
enum class WorkspacePanelItem {
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
    kRenderChunkBounds,
    kRenderWorldGrid,
    kRenderCollision,
    kRenderHeight,
    k3DMeshGroup,
    k3DVisibleFaces,
    k3DCulledFaces,
    k3DChunkMeshes,
    k3DDirtyChunks,

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
 * @brief Runtime state for the main workspace screen.
 */
struct WorkspaceState {
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
    MapPackageInfo map;
    RuntimeMap runtime_map;
    ChunkGrid chunk_grid;
    VoxelWorld voxel_world;
    FaceVisibilityResult face_visibility;
    ChunkMeshBuildResult chunk_meshes;
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
 * @brief Builds the visible tree rows for the currently selected workspace section.
 *
 * @param workspace Workspace state.
 * @return Rows shown under the selected workspace section.
 */
[[nodiscard]] std::vector<WorkspacePanelItemState> BuildWorkspacePanelItems(const WorkspaceState& workspace);

}  // namespace vox3d
