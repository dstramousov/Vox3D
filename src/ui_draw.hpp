#pragma once

#include "app.hpp"
#include "confirm_dialog.hpp"
#include "menu.hpp"
#include "process_metrics.hpp"
#include "vox3d/render_raylib/chunk_mesh_preview.hpp"
#include "vox3d/render_raylib/free_fly_camera.hpp"
#include "vox3d/render_raylib/map_2d_view.hpp"
#include "ui_fonts.hpp"
#include "ui_labels.hpp"
#include "ui_layout.hpp"
#include "window_config.hpp"
#include "workspace.hpp"

#include <raylib.h>

#include <string>
#include <string_view>

namespace vox3d {

/**
 * @brief Draws the main menu screen.
 *
 * @param menu Menu state to draw.
 * @param fonts Fonts used for the decorative title and UI text.
 * @param labels Localized labels used for main title/subtitle.
 * @param layout Cached main menu layout.
 */
void DrawMainMenu(const MenuState& menu, const UiFontSet& fonts, const UiLabels& labels, const UiLayoutCache& layout);

/**
 * @brief Draws a simple placeholder screen.
 *
 * @param title Main placeholder title.
 * @param selected_action Currently selected placeholder action.
 * @param fonts Fonts used for placeholder text.
 * @param labels Localized labels used for action buttons and hints.
 * @param layout Cached UI layout.
 */
void DrawPlaceholderScreen(
    const std::string& title,
    PlaceholderAction selected_action,
    const UiFontSet& fonts,
    const UiLabels& labels,
    const UiLayoutCache& layout);

/**
 * @brief Draws the main workspace screen.
 *
 * @param workspace Workspace state to draw.
 * @param map_2d_view Optional interactive 2D map renderer.
 * @param mesh_preview Optional uploaded 3D mesh preview renderer.
 * @param preview_camera Optional camera used by the 3D preview renderer.
 * @param camera_status Latest camera diagnostics shown in the workspace panel.
 * @param fonts Fonts used for workspace text.
 * @param labels Localized labels used for workspace controls.
 * @param layout Cached UI layout.
 */
void DrawWorkspace(
    const WorkspaceState& workspace,
    const Map2DView* map_2d_view,
    const RaylibChunkMeshPreview* mesh_preview,
    const Camera3D* preview_camera,
    FreeFlyCameraStatus camera_status,
    const UiFontSet& fonts,
    const UiLabels& labels,
    const UiLayoutCache& layout);

/**
 * @brief Returns the maximum scroll offset for the selection information overlay.
 *
 * @param workspace Workspace state used to build the overlay content.
 * @param layout Current UI layout and window metrics.
 * @return Maximum first visible row index, never negative.
 */
[[nodiscard]] int SelectionInfoOverlayMaxScrollRows(
    const WorkspaceState& workspace,
    const UiLayoutCache& layout);

/**
 * @brief Draws a modal overlay with information about the selected tile.
 *
 * The overlay is read-only and does not modify the workspace selection.
 *
 * @param workspace Workspace state containing the current selection.
 * @param first_visible_row Requested first visible content row.
 * @param fonts Fonts used for overlay text.
 * @param layout Current UI layout and window metrics.
 */
void DrawSelectionInfoOverlay(
    const WorkspaceState& workspace,
    int first_visible_row,
    const UiFontSet& fonts,
    const UiLayoutCache& layout);

/**
 * @brief Returns the maximum scroll offset for the keyboard help overlay.
 *
 * @param layout Current UI layout and window metrics.
 * @return Maximum first visible row index, never negative.
 */
[[nodiscard]] int HelpOverlayMaxScrollRows(const UiLayoutCache& layout);

/**
 * @brief Draws the 2D and 3D keyboard help overlay.
 *
 * The overlay keeps mode-specific controls in separate sections and uses the
 * same geometry and visual style as the selection information overlay.
 *
 * @param first_visible_row Requested first visible content row.
 * @param fonts Fonts used for overlay text.
 * @param layout Current UI layout and window metrics.
 */
void DrawHelpOverlay(
    int first_visible_row,
    const UiFontSet& fonts,
    const UiLayoutCache& layout);

/**
 * @brief Returns the maximum scroll offset for the mode-specific statistics overlay.
 *
 * @param workspace Workspace state used to build statistics.
 * @param map_2d_view Optional interactive 2D view.
 * @param camera_status Latest 3D camera diagnostics.
 * @param layout Current UI layout and window metrics.
 * @return Maximum scroll offset in logical rows, never negative.
 */
[[nodiscard]] int StatsOverlayMaxScrollRows(
    const WorkspaceState& workspace,
    const Map2DView* map_2d_view,
    FreeFlyCameraStatus camera_status,
    const UiLayoutCache& layout);

/**
 * @brief Draws a live table-style statistics overlay for the active 2D or 3D mode.
 *
 * @param workspace Workspace state containing map and renderer diagnostics.
 * @param map_2d_view Optional interactive 2D view.
 * @param camera_status Latest 3D camera diagnostics.
 * @param first_visible_row Requested logical scroll row.
 * @param fonts Fonts used for overlay text.
 * @param layout Current UI layout and window metrics.
 */
void DrawStatsOverlay(
    const WorkspaceState& workspace,
    const Map2DView* map_2d_view,
    FreeFlyCameraStatus camera_status,
    int first_visible_row,
    const UiFontSet& fonts,
    const UiLayoutCache& layout);

/**
 * @brief Draws the FPS counter.
 *
 * @param fonts Fonts used for compact UI text.
 * @param labels Localized labels used for the FPS and memory prefixes.
 * @param layout Cached UI layout.
 * @param memory Latest cached process memory snapshot.
 * @param version Application version shown in the status bar.
 */
void DrawFpsCounter(
    const UiFontSet& fonts,
    const UiLabels& labels,
    const UiLayoutCache& layout,
    const ProcessMemoryInfo& memory,
    std::string_view version);

/**
 * @brief Draws optional debug information.
 *
 * @param config Application configuration.
 * @param window Window configuration.
 * @param screen Current application screen.
 * @param dialog Current modal dialog.
 * @param menu Menu state.
 * @param workspace Workspace state.
 * @param hovered_item Stable name of the item or action currently under the mouse.
 * @param labels Localized labels used for debug field names.
 * @param fonts Fonts used for compact UI text.
 * @param layout Cached UI layout.
 */
void DrawDebugOverlay(
    const UiFontSet& fonts,
    const AppConfig& config,
    const WindowConfig& window,
    AppScreen screen,
    ModalDialog dialog,
    const MenuState& menu,
    const WorkspaceState& workspace,
    std::string_view hovered_item,
    const UiLabels& labels,
    const UiLayoutCache& layout);

/**
 * @brief Draws the exit confirmation dialog over the current screen.
 *
 * @param state Dialog state.
 * @param fonts Fonts used for dialog text.
 * @param labels Localized labels used for dialog text and buttons.
 * @param layout Cached UI layout.
 */
void DrawExitDialog(const ConfirmDialogState& state, const UiFontSet& fonts, const UiLabels& labels, const UiLayoutCache& layout);

}  // namespace vox3d
