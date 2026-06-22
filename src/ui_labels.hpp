#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace vox3d {

/**
 * @brief User-facing UI text loaded for a selected language.
 */
struct UiLabels {
    std::string language = "en";

    std::string app_title = "VoX3D";
    std::string app_subtitle = "voxel runtime bootstrap";

    std::string menu_new_game = "Open Workspace";
    std::string menu_load_game = "Load";
    std::string menu_settings = "Settings";
    std::string menu_exit = "Exit";

    std::string placeholder_game_title = "Game screen placeholder";
    std::string placeholder_settings_title = "Settings placeholder";
    std::string placeholder_back_hint = "Esc returns to main menu";
    std::string placeholder_main_menu = "Main Menu";
    std::string placeholder_exit = "Exit";

    std::string workspace_title = "MainRender";
    std::string workspace_viewport_title = "MainRender";
    std::string workspace_tool_panel_title = "VoX3D";
    std::string workspace_tool_map = "Map";
    std::string workspace_tool_view = "View";
    std::string workspace_tool_layers = "Layers";
    std::string workspace_tool_objects = "Objects";
    std::string workspace_tool_render = "Render";
    std::string workspace_tool_debug = "Debug";
    std::string workspace_tool_settings = "Settings";
    std::string workspace_status_ready = "Ready";
    std::string workspace_status_map_loaded = "Map loaded";
    std::string workspace_status_map_missing = "Map missing";
    std::string workspace_status_map_not_configured = "Map not configured";
    std::string workspace_status_metadata_unavailable = "Metadata unavailable";
    std::string workspace_status_escape_hint = "Esc: exit";
    std::string workspace_map_label = "Map";
    std::string workspace_size_label = "Size";
    std::string workspace_levels_label = "Levels";
    std::string workspace_tile_label = "Tile";
    std::string workspace_terrain_label = "Terrain";
    std::string workspace_elevation_label = "Elevation";
    std::string workspace_collision_label = "Collision";
    std::string workspace_overview_label = "Overview";
    std::string workspace_source_label = "Source";
    std::string workspace_yes = "yes";
    std::string workspace_no = "no";
    std::string workspace_map_size_unknown = "size unknown";
    std::string workspace_map_levels_unknown = "levels unknown";
    std::string workspace_map_tile_unknown = "tile unknown";
    std::string workspace_overview_unavailable = "overview unavailable";

    std::string dialog_exit_title = "Exit program?";
    std::string dialog_exit_message = "Unsaved progress may be lost.";
    std::string dialog_yes = "Yes";
    std::string dialog_no = "No";

    std::string fps_label = "FPS";
    std::string memory_label = "MEM";

    std::string debug_version = "version";
    std::string debug_screen = "screen";
    std::string debug_window = "window";
    std::string debug_ui_scale = "ui_scale";
    std::string debug_modal = "modal";
    std::string debug_selected = "selected";
    std::string debug_hovered = "hovered";
    std::string debug_workspace_tool = "workspace_tool";
    std::string debug_map_path = "map_path";
    std::string debug_map_loaded = "map_loaded";
    std::string debug_none = "none";
};

/**
 * @brief Creates built-in labels for a supported language.
 *
 * English is used as the fallback when the requested language is unknown.
 *
 * @param language Requested language code.
 * @return Built-in labels for the requested language or English fallback.
 */
[[nodiscard]] UiLabels DefaultUiLabels(std::string_view language);

/**
 * @brief Loads UI labels from a flat JSON object.
 *
 * Missing fields keep the current values in @p labels. Invalid fields are ignored
 * individually and reported through diagnostics.
 *
 * @param labels_path Path to the language JSON file.
 * @param labels Labels object to update.
 * @param diagnostics Human-readable warnings collected during loading.
 * @return True if the language file was read and parsed successfully.
 */
[[nodiscard]] bool LoadUiLabelsFromFile(
    const std::filesystem::path& labels_path,
    UiLabels& labels,
    std::vector<std::string>& diagnostics);

}  // namespace vox3d
