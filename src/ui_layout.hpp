#pragma once

#include "app_config.hpp"
#include "confirm_dialog.hpp"
#include "menu.hpp"
#include "ui_fonts.hpp"
#include "ui_labels.hpp"
#include "window_config.hpp"
#include "workspace.hpp"

#include <raylib.h>

#include <string>
#include <string_view>
#include <vector>

namespace vox3d {

/**
 * @brief Resolved UI sizes derived from the current window and application config.
 */
struct UiMetrics {
    int window_width = 0;
    int window_height = 0;
    float scale = 1.0F;
    float font_scale = 1.0F;
    float text_spacing = 2.0F;

    float title_font_size = 20.0F;
    float subtitle_font_size = 13.0F;
    float menu_font_size = 15.0F;
    float placeholder_title_font_size = 18.0F;
    float placeholder_hint_font_size = 14.0F;
    float fps_font_size = 18.0F;
    float debug_font_size = 12.0F;
    float dialog_title_font_size = 18.0F;
    float dialog_text_font_size = 14.0F;
    float dialog_button_font_size = 16.0F;

    float screen_padding = 16.0F;
    float menu_item_gap = 13.0F;
    float menu_item_padding_x = 18.0F;
    float menu_item_padding_y = 8.0F;
    float debug_line_gap = 15.0F;
    float dialog_padding = 40.0F;
    float dialog_button_width = 150.0F;
    float dialog_button_height = 50.0F;
    float dialog_button_gap = 28.0F;
    float modal_border_width = 2.0F;

    float workspace_panel_width = 210.0F;
    float workspace_status_height = 38.0F;
    float workspace_border_width = 2.0F;
    float workspace_tool_gap = 6.0F;
    float workspace_tool_font_size = 18.0F;
    float workspace_status_font_size = 16.0F;
    float workspace_panel_padding = 10.0F;
};

/**
 * @brief Rectangle associated with a menu item index.
 */
struct MenuItemBounds {
    int index = 0;
    Rectangle bounds{};
    Vector2 text_position{};
};

/**
 * @brief Cached layout for the main menu screen.
 */
struct MainMenuLayout {
    std::vector<MenuItemBounds> items;
    float title_y = 0.0F;
    float subtitle_y = 0.0F;
};

/**
 * @brief Hit boxes for confirmation dialog buttons.
 */
struct DialogButtonBounds {
    Rectangle yes{};
    Rectangle no{};
};

/**
 * @brief Action selectable on temporary placeholder screens.
 */
enum class PlaceholderAction {
    kMainMenu,
    kExit,
};

/**
 * @brief Hit box and text position for a placeholder screen action.
 */
struct PlaceholderActionBounds {
    PlaceholderAction action = PlaceholderAction::kMainMenu;
    Rectangle bounds{};
    Vector2 text_position{};
};

/**
 * @brief Cached layout for temporary placeholder screens.
 */
struct PlaceholderLayout {
    std::vector<PlaceholderActionBounds> actions;
    float title_y = 0.0F;
    float hint_y = 0.0F;
};

/**
 * @brief Hit box and text position for a workspace accordion section header.
 */
struct WorkspaceToolBounds {
    WorkspaceTool tool = WorkspaceTool::kMode;
    Rectangle bounds{};
    Vector2 text_position{};
};

/**
 * @brief Hit box and text position for a workspace right-panel tab.
 */
struct WorkspacePanelTabBounds {
    WorkspacePanelTab tab = WorkspacePanelTab::kMenu;
    Rectangle bounds{};
    Vector2 text_position{};
};

/**
 * @brief Hit box and text position for a workspace accordion subitem.
 */
struct WorkspacePanelItemBounds {
    WorkspacePanelItem item = WorkspacePanelItem::kMode2DMap;
    WorkspacePanelItemKind kind = WorkspacePanelItemKind::kAction;
    int depth = 1;
    Rectangle bounds{};
    Vector2 text_position{};
    bool enabled = false;
    bool checked = false;
};

/**
 * @brief Cached layout for the main workspace screen.
 */
struct WorkspaceLayout {
    Rectangle viewport{};
    Rectangle tool_panel{};
    Rectangle status_bar{};
    Rectangle tool_header{};
    Rectangle tool_menu{};
    Rectangle tool_info{};
    Rectangle map_summary{};
    Rectangle map_overview{};
    std::vector<WorkspaceToolBounds> tools;
    std::vector<WorkspacePanelTabBounds> panel_tabs;
    std::vector<WorkspacePanelItemBounds> panel_items;
    int panel_total_rows = 0;
    int panel_first_visible_row = 0;
    int panel_visible_rows = 0;
    bool panel_can_scroll_up = false;
    bool panel_can_scroll_down = false;
};

/**
 * @brief Cached layout for a confirmation dialog.
 */
struct ConfirmDialogLayout {
    Rectangle panel{};
    DialogButtonBounds buttons{};
    float title_y = 0.0F;
    float message_y = 0.0F;
    std::vector<std::string> message_lines;
};

/**
 * @brief Cached screen layout used by input hit-testing and rendering.
 */
struct UiLayoutCache {
    UiMetrics metrics{};
    MainMenuLayout main_menu{};
    PlaceholderLayout placeholder{};
    WorkspaceLayout workspace{};
    ConfirmDialogLayout exit_dialog{};
};

/**
 * @brief Converts a placeholder action to a stable lowercase name.
 *
 * @param action Placeholder action.
 * @return String representation.
 */
[[nodiscard]] std::string_view ToString(PlaceholderAction action);

/**
 * @brief Calculates resolved UI metrics from window size and app configuration.
 *
 * @param window Window configuration.
 * @param config Application configuration.
 * @return Resolved UI metrics for the current frame size.
 */
[[nodiscard]] UiMetrics CalculateUiMetrics(const WindowConfig& window, const AppConfig& config);

/**
 * @brief Rebuilds cached UI layout for the current window, menu, and font.
 *
 * This function performs text measurement and word wrapping. It should be called only
 * when the window size, menu content, font, or relevant configuration changes.
 *
 * @param menu Menu state to layout.
 * @param fonts UI fonts used for text measurement and cached layout.
 * @param window Window configuration.
 * @param config Application configuration.
 * @param labels Localized labels used for measured dialog text.
 * @param workspace Workspace state used for accordion layout.
 * @return Cached UI layout.
 */
[[nodiscard]] UiLayoutCache RebuildUiLayout(
    const MenuState& menu,
    const UiFontSet& fonts,
    const WindowConfig& window,
    const AppConfig& config,
    const UiLabels& labels,
    const WorkspaceState& workspace);

}  // namespace vox3d
