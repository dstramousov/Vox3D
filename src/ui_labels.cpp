#include "ui_labels.hpp"

#include <cctype>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>

namespace vox3d {
namespace {

class FlatStringJsonParser {
public:
    explicit FlatStringJsonParser(std::string_view input)
        : input_(input)
    {
    }

    [[nodiscard]] std::optional<std::map<std::string, std::string>> Parse(std::string& error)
    {
        SkipWhitespace();
        if (Take() != '{') {
            error = "expected object";
            return std::nullopt;
        }

        std::map<std::string, std::string> result;
        SkipWhitespace();
        if (Peek() == '}') {
            Take();
            return result;
        }

        while (!IsEnd()) {
            SkipWhitespace();
            auto key = ParseString(error);
            if (!key.has_value()) {
                return std::nullopt;
            }

            SkipWhitespace();
            if (Take() != ':') {
                error = "expected ':' after object key";
                return std::nullopt;
            }

            SkipWhitespace();
            auto value = ParseString(error);
            if (!value.has_value()) {
                return std::nullopt;
            }
            result.emplace(std::move(*key), std::move(*value));

            SkipWhitespace();
            const char next = Take();
            if (next == '}') {
                SkipWhitespace();
                if (!IsEnd()) {
                    error = "unexpected trailing characters";
                    return std::nullopt;
                }
                return result;
            }
            if (next != ',') {
                error = "expected ',' or '}' in object";
                return std::nullopt;
            }
        }

        error = "unterminated object";
        return std::nullopt;
    }

private:
    [[nodiscard]] bool IsEnd() const
    {
        return position_ >= input_.size();
    }

    [[nodiscard]] char Peek() const
    {
        return IsEnd() ? '\0' : input_[position_];
    }

    char Take()
    {
        return IsEnd() ? '\0' : input_[position_++];
    }

    void SkipWhitespace()
    {
        while (!IsEnd() && std::isspace(static_cast<unsigned char>(Peek())) != 0) {
            ++position_;
        }
    }

    [[nodiscard]] std::optional<std::string> ParseString(std::string& error)
    {
        if (Take() != '"') {
            error = "expected string";
            return std::nullopt;
        }

        std::string result;
        while (!IsEnd()) {
            const char c = Take();
            if (c == '"') {
                return result;
            }
            if (c != '\\') {
                result.push_back(c);
                continue;
            }

            if (IsEnd()) {
                error = "unterminated escape sequence";
                return std::nullopt;
            }
            const char escaped = Take();
            switch (escaped) {
                case '"':
                case '\\':
                case '/':
                    result.push_back(escaped);
                    break;
                case 'b':
                    result.push_back('\b');
                    break;
                case 'f':
                    result.push_back('\f');
                    break;
                case 'n':
                    result.push_back('\n');
                    break;
                case 'r':
                    result.push_back('\r');
                    break;
                case 't':
                    result.push_back('\t');
                    break;
                default:
                    error = "unsupported escape sequence";
                    return std::nullopt;
            }
        }

        error = "unterminated string";
        return std::nullopt;
    }

    std::string_view input_;
    std::size_t position_ = 0;
};

void AssignLabel(
    const std::map<std::string, std::string>& values,
    std::string_view key,
    std::string& target,
    std::vector<std::string>& diagnostics)
{
    const auto found = values.find(std::string(key));
    if (found == values.end()) {
        diagnostics.push_back("labels: missing key=\"" + std::string(key) + "\", using fallback text");
        return;
    }
    target = found->second;
}

}  // namespace

UiLabels DefaultUiLabels(std::string_view language)
{
    UiLabels labels;
    labels.language = std::string(language.empty() ? std::string_view("en") : language);

    if (language == "uk") {
        labels.app_title = "VoX3D";
        labels.app_subtitle = "voxel runtime bootstrap";
        labels.menu_new_game = "Відкрити робочий екран";
        labels.menu_load_game = "Завантажити";
        labels.menu_settings = "Налаштування";
        labels.menu_exit = "Вийти";
        labels.placeholder_game_title = "Екран гри";
        labels.placeholder_settings_title = "Екран налаштувань";
        labels.placeholder_back_hint = "Esc повертає до головного меню";
        labels.placeholder_main_menu = "Головне меню";
        labels.placeholder_exit = "Вийти";
        labels.workspace_title = "MainRender";
        labels.workspace_viewport_title = "MainRender";
        labels.workspace_tool_panel_title = "VoX3D";
        labels.workspace_tool_map = "Мапа";
        labels.workspace_tool_view = "Вид";
        labels.workspace_tool_layers = "Шари";
        labels.workspace_tool_objects = "Об’єкти";
        labels.workspace_tool_render = "Рендер";
        labels.workspace_tool_debug = "Дебаг";
        labels.workspace_tool_settings = "Налаштування";
        labels.workspace_status_ready = "Готово";
        labels.workspace_status_map_loaded = "Мапу завантажено";
        labels.workspace_status_map_missing = "Мапу не знайдено";
        labels.workspace_status_map_not_configured = "Мапу не налаштовано";
        labels.workspace_status_metadata_unavailable = "Метадані недоступні";
        labels.workspace_status_escape_hint = "Esc: вихід";
        labels.workspace_map_label = "Мапа";
        labels.workspace_size_label = "Розмір";
        labels.workspace_levels_label = "Рівні";
        labels.workspace_tile_label = "Тайл";
        labels.workspace_terrain_label = "Терен";
        labels.workspace_elevation_label = "Висота";
        labels.workspace_collision_label = "Колізії";
        labels.workspace_overview_label = "Огляд";
        labels.workspace_source_label = "Джерело";
        labels.workspace_yes = "так";
        labels.workspace_no = "ні";
        labels.workspace_map_size_unknown = "розмір невідомий";
        labels.workspace_map_levels_unknown = "рівні невідомі";
        labels.workspace_map_tile_unknown = "тайл невідомий";
        labels.workspace_overview_unavailable = "огляд недоступний";
        labels.workspace_subitem_overview = "Огляд";
        labels.workspace_subitem_package = "Пакет";
        labels.workspace_subitem_validate = "Перевірка";
        labels.workspace_subitem_2d_map = "2D мапа";
        labels.workspace_subitem_3d_preview = "3D перегляд";
        labels.workspace_subitem_fit_view = "Вмістити";
        labels.workspace_subitem_reset_view = "Скинути вид";
        labels.workspace_subitem_grid = "Сітка";
        labels.workspace_subitem_chunk_bounds = "Межі чанків";
        labels.workspace_subitem_world_grid = "Сітка світу";
        labels.workspace_subitem_collision_overlay = "Колізія";
        labels.workspace_subitem_solid = "Суцільний";
        labels.workspace_subitem_height = "Висота";
        labels.workspace_subitem_memory = "Пам'ять";
        labels.workspace_subitem_logs = "Логи";
        labels.workspace_subitem_language = "Мова";
        labels.dialog_exit_title = "Вийти з програми?";
        labels.dialog_exit_message = "Незбережений прогрес може бути втрачено.";
        labels.dialog_yes = "Так";
        labels.dialog_no = "Ні";
        labels.fps_label = "FPS";
        labels.memory_label = "Пам'ять";
        labels.debug_version = "версія";
        labels.debug_screen = "екран";
        labels.debug_window = "вікно";
        labels.debug_ui_scale = "ui_scale";
        labels.debug_modal = "modal";
        labels.debug_selected = "вибрано";
        labels.debug_hovered = "наведено";
        labels.debug_workspace_tool = "інструмент";
        labels.debug_map_path = "шлях_мапи";
        labels.debug_map_loaded = "мапа_завантажена";
        labels.debug_none = "немає";
        return labels;
    }

    labels.language = "en";
    return labels;
}

bool LoadUiLabelsFromFile(
    const std::filesystem::path& labels_path,
    UiLabels& labels,
    std::vector<std::string>& diagnostics)
{
    std::ifstream file(labels_path);
    if (!file) {
        diagnostics.push_back("labels: file not found path=\"" + labels_path.string() + "\", using built-in defaults");
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();

    std::string parse_error;
    FlatStringJsonParser parser(buffer.str());
    std::optional<std::map<std::string, std::string>> values = parser.Parse(parse_error);
    if (!values.has_value()) {
        diagnostics.push_back("labels: parse failed path=\"" + labels_path.string() + "\" reason=\"" + parse_error + "\"");
        return false;
    }

    AssignLabel(*values, "app.title", labels.app_title, diagnostics);
    AssignLabel(*values, "app.subtitle", labels.app_subtitle, diagnostics);
    AssignLabel(*values, "menu.new_game", labels.menu_new_game, diagnostics);
    AssignLabel(*values, "menu.load_game", labels.menu_load_game, diagnostics);
    AssignLabel(*values, "menu.settings", labels.menu_settings, diagnostics);
    AssignLabel(*values, "menu.exit", labels.menu_exit, diagnostics);
    AssignLabel(*values, "placeholder.game_title", labels.placeholder_game_title, diagnostics);
    AssignLabel(*values, "placeholder.settings_title", labels.placeholder_settings_title, diagnostics);
    AssignLabel(*values, "placeholder.back_hint", labels.placeholder_back_hint, diagnostics);
    AssignLabel(*values, "placeholder.main_menu", labels.placeholder_main_menu, diagnostics);
    AssignLabel(*values, "placeholder.exit", labels.placeholder_exit, diagnostics);
    AssignLabel(*values, "workspace.title", labels.workspace_title, diagnostics);
    AssignLabel(*values, "workspace.viewport_title", labels.workspace_viewport_title, diagnostics);
    AssignLabel(*values, "workspace.tool_panel_title", labels.workspace_tool_panel_title, diagnostics);
    AssignLabel(*values, "workspace.tool.map", labels.workspace_tool_map, diagnostics);
    AssignLabel(*values, "workspace.tool.view", labels.workspace_tool_view, diagnostics);
    AssignLabel(*values, "workspace.tool.layers", labels.workspace_tool_layers, diagnostics);
    AssignLabel(*values, "workspace.tool.objects", labels.workspace_tool_objects, diagnostics);
    AssignLabel(*values, "workspace.tool.render", labels.workspace_tool_render, diagnostics);
    AssignLabel(*values, "workspace.tool.debug", labels.workspace_tool_debug, diagnostics);
    AssignLabel(*values, "workspace.tool.settings", labels.workspace_tool_settings, diagnostics);
    AssignLabel(*values, "workspace.status.ready", labels.workspace_status_ready, diagnostics);
    AssignLabel(*values, "workspace.status.map_loaded", labels.workspace_status_map_loaded, diagnostics);
    AssignLabel(*values, "workspace.status.map_missing", labels.workspace_status_map_missing, diagnostics);
    AssignLabel(*values, "workspace.status.map_not_configured", labels.workspace_status_map_not_configured, diagnostics);
    AssignLabel(*values, "workspace.status.metadata_unavailable", labels.workspace_status_metadata_unavailable, diagnostics);
    AssignLabel(*values, "workspace.status.escape_hint", labels.workspace_status_escape_hint, diagnostics);
    AssignLabel(*values, "workspace.map.label", labels.workspace_map_label, diagnostics);
    AssignLabel(*values, "workspace.size.label", labels.workspace_size_label, diagnostics);
    AssignLabel(*values, "workspace.levels.label", labels.workspace_levels_label, diagnostics);
    AssignLabel(*values, "workspace.tile.label", labels.workspace_tile_label, diagnostics);
    AssignLabel(*values, "workspace.terrain.label", labels.workspace_terrain_label, diagnostics);
    AssignLabel(*values, "workspace.elevation.label", labels.workspace_elevation_label, diagnostics);
    AssignLabel(*values, "workspace.collision.label", labels.workspace_collision_label, diagnostics);
    AssignLabel(*values, "workspace.overview.label", labels.workspace_overview_label, diagnostics);
    AssignLabel(*values, "workspace.source.label", labels.workspace_source_label, diagnostics);
    AssignLabel(*values, "workspace.yes", labels.workspace_yes, diagnostics);
    AssignLabel(*values, "workspace.no", labels.workspace_no, diagnostics);
    AssignLabel(*values, "workspace.map.size_unknown", labels.workspace_map_size_unknown, diagnostics);
    AssignLabel(*values, "workspace.map.levels_unknown", labels.workspace_map_levels_unknown, diagnostics);
    AssignLabel(*values, "workspace.map.tile_unknown", labels.workspace_map_tile_unknown, diagnostics);
    AssignLabel(*values, "workspace.overview.unavailable", labels.workspace_overview_unavailable, diagnostics);
    AssignLabel(*values, "workspace.subitem.overview", labels.workspace_subitem_overview, diagnostics);
    AssignLabel(*values, "workspace.subitem.package", labels.workspace_subitem_package, diagnostics);
    AssignLabel(*values, "workspace.subitem.validate", labels.workspace_subitem_validate, diagnostics);
    AssignLabel(*values, "workspace.subitem.2d_map", labels.workspace_subitem_2d_map, diagnostics);
    AssignLabel(*values, "workspace.subitem.3d_preview", labels.workspace_subitem_3d_preview, diagnostics);
    AssignLabel(*values, "workspace.subitem.fit_view", labels.workspace_subitem_fit_view, diagnostics);
    AssignLabel(*values, "workspace.subitem.reset_view", labels.workspace_subitem_reset_view, diagnostics);
    AssignLabel(*values, "workspace.subitem.grid", labels.workspace_subitem_grid, diagnostics);
    AssignLabel(*values, "workspace.subitem.chunk_bounds", labels.workspace_subitem_chunk_bounds, diagnostics);
    AssignLabel(*values, "workspace.subitem.world_grid", labels.workspace_subitem_world_grid, diagnostics);
    AssignLabel(*values, "workspace.subitem.collision_overlay", labels.workspace_subitem_collision_overlay, diagnostics);
    AssignLabel(*values, "workspace.subitem.solid", labels.workspace_subitem_solid, diagnostics);
    AssignLabel(*values, "workspace.subitem.height", labels.workspace_subitem_height, diagnostics);
    AssignLabel(*values, "workspace.subitem.memory", labels.workspace_subitem_memory, diagnostics);
    AssignLabel(*values, "workspace.subitem.logs", labels.workspace_subitem_logs, diagnostics);
    AssignLabel(*values, "workspace.subitem.language", labels.workspace_subitem_language, diagnostics);
    AssignLabel(*values, "dialog.exit_title", labels.dialog_exit_title, diagnostics);
    AssignLabel(*values, "dialog.exit_message", labels.dialog_exit_message, diagnostics);
    AssignLabel(*values, "dialog.yes", labels.dialog_yes, diagnostics);
    AssignLabel(*values, "dialog.no", labels.dialog_no, diagnostics);
    AssignLabel(*values, "fps.label", labels.fps_label, diagnostics);
    AssignLabel(*values, "memory.label", labels.memory_label, diagnostics);
    AssignLabel(*values, "debug.version", labels.debug_version, diagnostics);
    AssignLabel(*values, "debug.screen", labels.debug_screen, diagnostics);
    AssignLabel(*values, "debug.window", labels.debug_window, diagnostics);
    AssignLabel(*values, "debug.ui_scale", labels.debug_ui_scale, diagnostics);
    AssignLabel(*values, "debug.modal", labels.debug_modal, diagnostics);
    AssignLabel(*values, "debug.selected", labels.debug_selected, diagnostics);
    AssignLabel(*values, "debug.hovered", labels.debug_hovered, diagnostics);
    AssignLabel(*values, "debug.workspace_tool", labels.debug_workspace_tool, diagnostics);
    AssignLabel(*values, "debug.map_path", labels.debug_map_path, diagnostics);
    AssignLabel(*values, "debug.map_loaded", labels.debug_map_loaded, diagnostics);
    AssignLabel(*values, "debug.none", labels.debug_none, diagnostics);
    return true;
}

}  // namespace vox3d
