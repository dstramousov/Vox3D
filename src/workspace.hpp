#pragma once

#include "map_package.hpp"

#include <string_view>

namespace vox3d {

/**
 * @brief Workspace tool currently selected in the right-side tool panel.
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
 * @brief Runtime state for the main workspace screen.
 */
struct WorkspaceState {
    WorkspaceTool selected_tool = WorkspaceTool::kMap;
    MapPackageInfo map;
};

/**
 * @brief Converts a workspace tool identifier to a stable lowercase name.
 *
 * @param tool Workspace tool identifier.
 * @return String representation.
 */
[[nodiscard]] std::string_view ToString(WorkspaceTool tool);

}  // namespace vox3d
