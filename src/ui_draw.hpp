#pragma once

#include "app.hpp"
#include "confirm_dialog.hpp"
#include "menu.hpp"
#include "process_metrics.hpp"
#include "ui_fonts.hpp"
#include "ui_labels.hpp"
#include "ui_layout.hpp"
#include "window_config.hpp"
#include "workspace.hpp"

#include <raylib.h>

#include <string>

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
 * @param fonts Fonts used for workspace text.
 * @param labels Localized labels used for workspace controls.
 * @param layout Cached UI layout.
 */
void DrawWorkspace(const WorkspaceState& workspace, const UiFontSet& fonts, const UiLabels& labels, const UiLayoutCache& layout);

/**
 * @brief Draws the FPS counter.
 *
 * @param fonts Fonts used for compact UI text.
 * @param labels Localized labels used for the FPS and memory prefixes.
 * @param layout Cached UI layout.
 * @param memory Latest cached process memory snapshot.
 */
void DrawFpsCounter(
    const UiFontSet& fonts,
    const UiLabels& labels,
    const UiLayoutCache& layout,
    const ProcessMemoryInfo& memory);

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
