#include "app.hpp"

#include "ui_draw.hpp"
#include "vox3d/chunk/chunk_grid.hpp"
#include "vox3d/map/map_package.hpp"
#include "vox3d/map/runtime_map.hpp"
#include "vox3d/mesh/chunk_mesh_builder.hpp"
#include "vox3d/mesh/face_visibility.hpp"
#include "vox3d/voxel/voxel_world.hpp"

#include <raylib.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace vox3d {
namespace {

[[nodiscard]] std::vector<int> BuildFontCodepoints()
{
    std::vector<int> codepoints;
    codepoints.reserve(256);
    for (int c = 32; c <= 126; ++c) {
        codepoints.push_back(c);
    }
    for (int c = 0x0400; c <= 0x04FF; ++c) {
        codepoints.push_back(c);
    }
    codepoints.push_back(0x2116);
    return codepoints;
}

[[nodiscard]] std::string BuildMode()
{
#ifdef NDEBUG
    return "release";
#else
    return "debug";
#endif
}

[[nodiscard]] std::string CurrentWorkingDirectory()
{
    std::error_code error;
    const auto path = std::filesystem::current_path(error);
    if (error) {
        return "<unavailable>";
    }
    return path.string();
}

[[nodiscard]] bool IsKeyPressedAny(std::initializer_list<int> keys)
{
    return std::any_of(keys.begin(), keys.end(), [](int key) { return IsKeyPressed(key); });
}

[[nodiscard]] bool PointInRect(Vector2 point, Rectangle rect)
{
    return CheckCollisionPointRec(point, rect);
}

[[nodiscard]] std::string Lowercase(std::string_view value)
{
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

[[nodiscard]] int RaylibTraceLogLevel(std::string_view value)
{
    const std::string normalized = Lowercase(value);
    if (normalized == "trace") {
        return LOG_TRACE;
    }
    if (normalized == "debug") {
        return LOG_DEBUG;
    }
    if (normalized == "info") {
        return LOG_INFO;
    }
    if (normalized == "warn" || normalized == "warning") {
        return LOG_WARNING;
    }
    if (normalized == "error") {
        return LOG_ERROR;
    }
    if (normalized == "fatal") {
        return LOG_FATAL;
    }
    return LOG_WARNING;
}

}  // namespace

App::App(AppConfig config, Logger& logger, UiLabels labels)
    : config_(std::move(config)), logger_(logger), labels_(std::move(labels)), main_menu_(labels_)
{
}

bool App::Initialize()
{
    logger_.Info("app", "started version=" + config_.version + " mode=" + BuildMode());
    logger_.Info("app", "working_directory=" + CurrentWorkingDirectory());
    logger_.Info(
        "log",
        "level=" + std::string(ToString(config_.log_level)) + " color="
            + std::string(config_.no_color || !config_.log_color ? "disabled" : "auto")
            + " raylib_level=" + config_.raylib_log_level);

    unsigned int raylib_flags = 0;
    if (config_.window_vsync) {
        raylib_flags |= FLAG_VSYNC_HINT;
    }
    if (config_.window_resizable) {
        raylib_flags |= FLAG_WINDOW_RESIZABLE;
    }
    if (raylib_flags != 0) {
        SetConfigFlags(raylib_flags);
    }
    SetTraceLogLevel(RaylibTraceLogLevel(config_.raylib_log_level));
    SetExitKey(KEY_NULL);

    InitWindow(config_.base_width, config_.base_height, config_.app_name.c_str());
    if (!IsWindowReady()) {
        logger_.Fatal(
            "window",
            "failed to initialize requested=" + std::to_string(config_.base_width) + "x"
                + std::to_string(config_.base_height) + " title=\"" + config_.app_name + "\"");
        return false;
    }

    window_initialized_ = true;

    const int monitor_index = GetCurrentMonitor();
    const Vector2 monitor_position = GetMonitorPosition(monitor_index);
    int monitor_width = GetMonitorWidth(monitor_index);
    int monitor_height = GetMonitorHeight(monitor_index);
    if (monitor_width <= 0 || monitor_height <= 0) {
        logger_.Warn("window", "monitor size unavailable after InitWindow, using conservative fallback 1920x1080");
        monitor_width = 1920;
        monitor_height = 1080;
    }

    window_config_ = CalculateWindowConfig(
        static_cast<int>(monitor_position.x),
        static_cast<int>(monitor_position.y),
        monitor_width,
        monitor_height,
        config_);
    window_config_.monitor_index = monitor_index;

    if (GetScreenWidth() != window_config_.window_width || GetScreenHeight() != window_config_.window_height) {
        SetWindowSize(window_config_.window_width, window_config_.window_height);
    }
    SetWindowPosition(window_config_.window_x, window_config_.window_y);
    SetTargetFPS(config_.target_fps);

    workspace_.map = LoadMapPackageInfo(config_.map_package_path);
    logger_.Info("map", ToLogString(workspace_.map));
    for (const auto& warning : workspace_.map.warnings) {
        logger_.Warn("map", warning);
    }
    workspace_.runtime_map = BuildRuntimeMap(workspace_.map);
    logger_.Info("runtime_map", ToLogString(workspace_.runtime_map));
    for (const auto& warning : workspace_.runtime_map.diagnostics.warnings) {
        logger_.Warn("runtime_map", warning);
    }
    workspace_.chunk_grid = BuildChunkGrid(workspace_.runtime_map);
    logger_.Info("chunk_grid", ToLogString(workspace_.chunk_grid));
    for (const auto& warning : workspace_.chunk_grid.diagnostics.warnings) {
        logger_.Warn("chunk_grid", warning);
    }
    workspace_.voxel_world = BuildVoxelWorld(workspace_.runtime_map, workspace_.chunk_grid);
    logger_.Info("voxel_world", ToLogString(workspace_.voxel_world));
    for (const auto& warning : workspace_.voxel_world.diagnostics.warnings) {
        logger_.Warn("voxel_world", warning);
    }
    workspace_.face_visibility = BuildFaceVisibility(workspace_.voxel_world);
    logger_.Info("face_visibility", ToLogString(workspace_.face_visibility));
    for (const auto& warning : workspace_.face_visibility.diagnostics.warnings) {
        logger_.Warn("face_visibility", warning);
    }
    workspace_.chunk_meshes = BuildChunkMeshes(workspace_.voxel_world, workspace_.chunk_grid);
    logger_.Info("chunk_mesh", ToLogString(workspace_.chunk_meshes));
    for (const auto& warning : workspace_.chunk_meshes.diagnostics.warnings) {
        logger_.Warn("chunk_mesh", warning);
    }
    main_menu_.SetItemEnabled(MenuItemId::kLoadGame, workspace_.map.loaded);

    LoadUiFonts();
    RefreshProcessMemoryInfo();
    RebuildLayout();

    {
        std::ostringstream out;
        out << "monitor=" << window_config_.monitor_width << 'x' << window_config_.monitor_height << " window="
            << window_config_.window_width << 'x' << window_config_.window_height << " pos=" << window_config_.window_x
            << ',' << window_config_.window_y << " ui_scale=" << window_config_.ui_scale;
        logger_.Info("window", out.str());
    }
    {
        std::ostringstream out;
        out << main_menu_.State();
        logger_.Debug("menu", out.str());
    }
    logger_.Info("app", "opened screen=workspace");

    return true;
}

int App::Run()
{
    if (!window_initialized_) {
        logger_.Fatal("app", "Run called before successful Initialize");
        return 1;
    }

    running_ = true;
    while (running_) {
        const bool close_requested = WindowShouldClose();
        if (close_requested && window_close_request_armed_) {
            logger_.Info("window", "close requested");
            RequestExitConfirmation(true);
            window_close_request_armed_ = false;
        } else if (!close_requested) {
            window_close_request_armed_ = true;
        }

        const float dt = GetFrameTime();
        HandleInput(dt);
        Update(dt);
        Draw();
    }

    logger_.Info("app", "shutting down");
    return 0;
}

void App::Shutdown()
{
    UnloadUiFonts();
    if (window_initialized_) {
        CloseWindow();
        window_initialized_ = false;
    }
}

void App::HandleInput(float dt)
{
    (void)dt;
    hovered_item_ = labels_.debug_none;

    if (dialog_.type != ModalDialog::kNone) {
        if (dialog_input_blocked_until_next_frame_) {
            dialog_input_blocked_until_next_frame_ = false;
            return;
        }
        HandleDialogInput();
        return;
    }

    if (screen_ == AppScreen::kMainMenu) {
        HandleMainMenuInput();
        return;
    }

    HandleScreenInput();
}

void App::HandleMainMenuInput()
{
    if (IsKeyPressedAny({KEY_UP, KEY_W})) {
        if (main_menu_.SelectPrevious()) {
            LogSelectedItemChanged();
        }
    }
    if (IsKeyPressedAny({KEY_DOWN, KEY_S})) {
        if (main_menu_.SelectNext()) {
            LogSelectedItemChanged();
        }
    }
    if (IsKeyPressed(KEY_ENTER)) {
        ActivateSelectedMenuItem();
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        RequestExitConfirmation();
    }

    if (layout_dirty_) {
        RebuildLayout();
    }

    const Vector2 mouse = GetMousePosition();
    for (const auto& item_bounds : layout_cache_.main_menu.items) {
        const auto& item = main_menu_.State().items[static_cast<std::size_t>(item_bounds.index)];
        if (!PointInRect(mouse, item_bounds.bounds)) {
            continue;
        }

        hovered_item_ = std::string(ToString(item.id));
        if (item.enabled) {
            if (main_menu_.SelectByIndex(item_bounds.index)) {
                LogSelectedItemChanged();
            }
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                ActivateSelectedMenuItem();
            }
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            logger_.Debug("menu", "disabled item activation ignored id=" + std::string(ToString(item.id)));
        }
        break;
    }
}

void App::HandleScreenInput()
{
    if (screen_ == AppScreen::kWorkspace) {
        HandleWorkspaceInput();
        return;
    }
    if (screen_ == AppScreen::kSettingsPlaceholder) {
        HandlePlaceholderInput();
    }
}

void App::HandleWorkspaceInput()
{
    if (IsKeyPressed(KEY_ESCAPE)) {
        RequestExitConfirmation();
        return;
    }

    if (IsKeyPressedAny({KEY_UP, KEY_W, KEY_LEFT, KEY_A})) {
        SelectPreviousWorkspaceTool();
    }
    if (IsKeyPressedAny({KEY_DOWN, KEY_S, KEY_RIGHT, KEY_D, KEY_TAB})) {
        SelectNextWorkspaceTool();
    }
    if (IsKeyPressed(KEY_ENTER)) {
        ToggleWorkspaceTool(workspace_.selected_tool);
    }

    if (layout_dirty_) {
        RebuildLayout();
    }

    const Vector2 mouse = GetMousePosition();
    for (const auto& tool_bounds : layout_cache_.workspace.tools) {
        if (!PointInRect(mouse, tool_bounds.bounds)) {
            continue;
        }

        hovered_item_ = "workspace_" + std::string(ToString(tool_bounds.tool));
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ToggleWorkspaceTool(tool_bounds.tool);
        }
        return;
    }

    for (const auto& item_bounds : layout_cache_.workspace.panel_items) {
        if (!PointInRect(mouse, item_bounds.bounds)) {
            continue;
        }

        hovered_item_ = "workspace_" + std::string(ToString(item_bounds.item));
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ActivateWorkspacePanelItem(item_bounds.item);
        }
        return;
    }
}

void App::HandlePlaceholderInput()
{
    if (IsKeyPressed(KEY_ESCAPE)) {
        SetCurrentScreen(AppScreen::kMainMenu, "back");
        return;
    }

    if (IsKeyPressedAny({KEY_LEFT, KEY_A, KEY_RIGHT, KEY_D, KEY_TAB})) {
        placeholder_selected_action_ = placeholder_selected_action_ == PlaceholderAction::kMainMenu
            ? PlaceholderAction::kExit
            : PlaceholderAction::kMainMenu;
        logger_.Debug("placeholder", "selected action=" + std::string(ToString(placeholder_selected_action_)));
    }

    if (layout_dirty_) {
        RebuildLayout();
    }

    const Vector2 mouse = GetMousePosition();
    for (const auto& action_bounds : layout_cache_.placeholder.actions) {
        if (!PointInRect(mouse, action_bounds.bounds)) {
            continue;
        }

        hovered_item_ = "placeholder_" + std::string(ToString(action_bounds.action));
        if (placeholder_selected_action_ != action_bounds.action) {
            placeholder_selected_action_ = action_bounds.action;
            logger_.Debug("placeholder", "selected action=" + std::string(ToString(placeholder_selected_action_)));
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ActivatePlaceholderAction();
            return;
        }
        break;
    }

    if (IsKeyPressed(KEY_ENTER)) {
        ActivatePlaceholderAction();
    }
}

void App::HandleDialogInput()
{
    if (IsKeyPressedAny({KEY_LEFT, KEY_A})) {
        dialog_.selected_choice = DialogChoice::kYes;
        logger_.Debug("dialog", "selected choice=yes");
    }
    if (IsKeyPressedAny({KEY_RIGHT, KEY_D})) {
        dialog_.selected_choice = DialogChoice::kNo;
        logger_.Debug("dialog", "selected choice=no");
    }
    if (IsKeyPressed(KEY_TAB)) {
        dialog_.selected_choice = dialog_.selected_choice == DialogChoice::kYes ? DialogChoice::kNo : DialogChoice::kYes;
        logger_.Debug("dialog", "selected choice=" + std::string(ToString(dialog_.selected_choice)));
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        CancelExitConfirmation();
        return;
    }

    if (layout_dirty_) {
        RebuildLayout();
    }

    const Vector2 mouse = GetMousePosition();
    const DialogButtonBounds& buttons = layout_cache_.exit_dialog.buttons;
    if (PointInRect(mouse, buttons.yes)) {
        hovered_item_ = "dialog_yes";
        if (dialog_.selected_choice != DialogChoice::kYes) {
            dialog_.selected_choice = DialogChoice::kYes;
            logger_.Debug("dialog", "selected choice=yes");
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            AcceptExitConfirmation();
            return;
        }
    } else if (PointInRect(mouse, buttons.no)) {
        hovered_item_ = "dialog_no";
        if (dialog_.selected_choice != DialogChoice::kNo) {
            dialog_.selected_choice = DialogChoice::kNo;
            logger_.Debug("dialog", "selected choice=no");
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            CancelExitConfirmation();
            return;
        }
    }

    if (IsKeyPressed(KEY_ENTER)) {
        if (dialog_.selected_choice == DialogChoice::kYes) {
            AcceptExitConfirmation();
        } else {
            CancelExitConfirmation();
        }
    }
}

void App::Update(float dt)
{
    process_memory_sample_timer_ += dt;
    if (process_memory_sample_timer_ >= 0.50F) {
        RefreshProcessMemoryInfo();
        process_memory_sample_timer_ = 0.0F;
    }

    if (!config_.window_resizable || !IsWindowResized()) {
        return;
    }

    window_config_.window_width = GetScreenWidth();
    window_config_.window_height = GetScreenHeight();
    window_config_.ui_scale = CalculateUiScale(window_config_.window_width, window_config_.window_height, config_);
    layout_dirty_ = true;
    logger_.Debug(
        "window",
        "resized window=" + std::to_string(window_config_.window_width) + "x"
            + std::to_string(window_config_.window_height) + " ui_scale=" + std::to_string(window_config_.ui_scale));
}

void App::Draw()
{
    if (layout_dirty_) {
        RebuildLayout();
    }

    BeginDrawing();
    ClearBackground(Color{18, 20, 28, 255});

    switch (screen_) {
        case AppScreen::kMainMenu:
            DrawMainMenu(main_menu_.State(), UiFonts(), labels_, layout_cache_);
            break;
        case AppScreen::kWorkspace:
            DrawWorkspace(workspace_, UiFonts(), labels_, layout_cache_);
            break;
        case AppScreen::kSettingsPlaceholder:
            DrawPlaceholderScreen(labels_.placeholder_settings_title, placeholder_selected_action_, UiFonts(), labels_, layout_cache_);
            break;
    }

    DrawFpsCounter(UiFonts(), labels_, layout_cache_, process_memory_);
    if (config_.debug_ui) {
        DrawDebugOverlay(UiFonts(), config_, window_config_, screen_, dialog_.type, main_menu_.State(), workspace_, hovered_item_, labels_, layout_cache_);
    }
    if (dialog_.type == ModalDialog::kExitConfirmation) {
        DrawExitDialog(dialog_, UiFonts(), labels_, layout_cache_);
    }

    EndDrawing();
}

void App::RebuildLayout()
{
    layout_cache_ = RebuildUiLayout(main_menu_.State(), UiFonts(), window_config_, config_, labels_, workspace_);
    layout_dirty_ = false;
}

void App::ActivateSelectedMenuItem()
{
    const MenuItem* item = main_menu_.SelectedItem();
    if (item == nullptr) {
        logger_.Debug("menu", "activation ignored because no enabled item is selected");
        return;
    }

    logger_.Info(
        "menu",
        "item activated id=" + std::string(ToString(item->id)) + " title=\"" + item->title + "\"");

    switch (item->id) {
        case MenuItemId::kNewGame:
            SetCurrentScreen(AppScreen::kWorkspace, "menu_workspace");
            break;
        case MenuItemId::kLoadGame:
            if (workspace_.map.loaded) {
                SetCurrentScreen(AppScreen::kWorkspace, "menu_load_map");
            } else {
                logger_.Debug("menu", "disabled item activation ignored id=load_game");
            }
            break;
        case MenuItemId::kSettings:
            SetCurrentScreen(AppScreen::kSettingsPlaceholder, "menu_settings");
            break;
        case MenuItemId::kExit:
            RequestExitConfirmation();
            break;
    }
}

void App::ActivatePlaceholderAction()
{
    logger_.Info("placeholder", "action activated id=" + std::string(ToString(placeholder_selected_action_)));
    switch (placeholder_selected_action_) {
        case PlaceholderAction::kMainMenu:
            SetCurrentScreen(AppScreen::kMainMenu, "placeholder_action");
            break;
        case PlaceholderAction::kExit:
            RequestExitConfirmation();
            break;
    }
}


void App::SelectPreviousWorkspaceTool()
{
    switch (workspace_.selected_tool) {
        case WorkspaceTool::kMap:
            workspace_.selected_tool = WorkspaceTool::kSettings;
            break;
        case WorkspaceTool::kView:
            workspace_.selected_tool = WorkspaceTool::kMap;
            break;
        case WorkspaceTool::kLayers:
            workspace_.selected_tool = WorkspaceTool::kView;
            break;
        case WorkspaceTool::kObjects:
            workspace_.selected_tool = WorkspaceTool::kLayers;
            break;
        case WorkspaceTool::kRender:
            workspace_.selected_tool = WorkspaceTool::kObjects;
            break;
        case WorkspaceTool::kDebug:
            workspace_.selected_tool = WorkspaceTool::kRender;
            break;
        case WorkspaceTool::kSettings:
            workspace_.selected_tool = WorkspaceTool::kDebug;
            break;
    }
    workspace_.selected_tool_expanded = true;
    layout_dirty_ = true;
    logger_.Debug("workspace", "selected tool=" + std::string(ToString(workspace_.selected_tool)));
}

void App::SelectNextWorkspaceTool()
{
    switch (workspace_.selected_tool) {
        case WorkspaceTool::kMap:
            workspace_.selected_tool = WorkspaceTool::kView;
            break;
        case WorkspaceTool::kView:
            workspace_.selected_tool = WorkspaceTool::kLayers;
            break;
        case WorkspaceTool::kLayers:
            workspace_.selected_tool = WorkspaceTool::kObjects;
            break;
        case WorkspaceTool::kObjects:
            workspace_.selected_tool = WorkspaceTool::kRender;
            break;
        case WorkspaceTool::kRender:
            workspace_.selected_tool = WorkspaceTool::kDebug;
            break;
        case WorkspaceTool::kDebug:
            workspace_.selected_tool = WorkspaceTool::kSettings;
            break;
        case WorkspaceTool::kSettings:
            workspace_.selected_tool = WorkspaceTool::kMap;
            break;
    }
    workspace_.selected_tool_expanded = true;
    layout_dirty_ = true;
    logger_.Debug("workspace", "selected tool=" + std::string(ToString(workspace_.selected_tool)));
}

void App::ToggleWorkspaceTool(WorkspaceTool tool)
{
    if (workspace_.selected_tool == tool) {
        workspace_.selected_tool_expanded = !workspace_.selected_tool_expanded;
    } else {
        workspace_.selected_tool = tool;
        workspace_.selected_tool_expanded = true;
    }
    layout_dirty_ = true;
    logger_.Debug(
        "workspace",
        "tool clicked tool=" + std::string(ToString(workspace_.selected_tool))
            + " expanded=" + std::string(workspace_.selected_tool_expanded ? "true" : "false"));
}

void App::ActivateWorkspacePanelItem(WorkspacePanelItem item)
{
    bool enabled = false;
    for (const WorkspacePanelItemState& state : BuildWorkspacePanelItems(workspace_)) {
        if (state.item == item) {
            enabled = state.enabled;
            break;
        }
    }

    if (!enabled) {
        logger_.Debug("workspace", "disabled panel item ignored id=" + std::string(ToString(item)));
        return;
    }

    switch (item) {
        case WorkspacePanelItem::kLayerTerrain:
            workspace_.show_terrain_layer = !workspace_.show_terrain_layer;
            break;
        case WorkspacePanelItem::kLayerElevation:
            workspace_.show_elevation_layer = !workspace_.show_elevation_layer;
            break;
        case WorkspacePanelItem::kLayerCollision:
            workspace_.show_collision_layer = !workspace_.show_collision_layer;
            break;
        case WorkspacePanelItem::kLayerGrid:
            workspace_.show_grid_layer = !workspace_.show_grid_layer;
            break;
        case WorkspacePanelItem::kMapOverview:
        case WorkspacePanelItem::kMapPackage:
        case WorkspacePanelItem::kMapValidate:
        case WorkspacePanelItem::kView2DMap:
        case WorkspacePanelItem::kView3DPreview:
        case WorkspacePanelItem::kViewFitMap:
        case WorkspacePanelItem::kViewResetView:
        case WorkspacePanelItem::kRenderOverview:
        case WorkspacePanelItem::kRenderWire:
        case WorkspacePanelItem::kRenderHeight:
        case WorkspacePanelItem::kDebugMemory:
        case WorkspacePanelItem::kDebugFps:
        case WorkspacePanelItem::kDebugLogs:
        case WorkspacePanelItem::kSettingsLanguage:
            break;
    }

    layout_dirty_ = true;
    logger_.Debug("workspace", "panel item activated id=" + std::string(ToString(item)));
}

void App::SetCurrentScreen(AppScreen screen, std::string_view reason)
{
    if (screen_ == screen) {
        return;
    }
    screen_ = screen;
    if (screen_ == AppScreen::kSettingsPlaceholder) {
        placeholder_selected_action_ = PlaceholderAction::kMainMenu;
    }
    logger_.Info("app", "screen changed screen=" + std::string(ToString(screen_)) + " reason=" + std::string(reason));
}

void App::RequestExitConfirmation(bool from_window_close)
{
    if (dialog_.type == ModalDialog::kExitConfirmation) {
        return;
    }
    exit_requested_from_window_ = from_window_close;
    dialog_.type = ModalDialog::kExitConfirmation;
    dialog_.selected_choice = DialogChoice::kNo;
    dialog_input_blocked_until_next_frame_ = true;
    logger_.Debug("dialog", "opened type=exit_confirmation default=no");
}

void App::CancelExitConfirmation()
{
    if (dialog_.type == ModalDialog::kNone) {
        return;
    }
    const bool was_window_close_request = exit_requested_from_window_;
    dialog_.type = ModalDialog::kNone;
    dialog_.selected_choice = DialogChoice::kNo;
    exit_requested_from_window_ = false;
    dialog_input_blocked_until_next_frame_ = false;
    logger_.Info("app", "exit cancelled by user");
    if (was_window_close_request) {
        logger_.Info("window", "close cancelled by user");
    }
}

void App::AcceptExitConfirmation()
{
    exit_requested_from_window_ = false;
    dialog_input_blocked_until_next_frame_ = false;
    logger_.Info("app", "exit accepted by user");
    running_ = false;
}

namespace {

[[nodiscard]] int FontAtlasSize(float base_size, float font_scale, int minimum, int maximum)
{
    const float resolved_scale = std::clamp(font_scale, 0.50F, 2.00F);
    return std::clamp(static_cast<int>(std::round(base_size * resolved_scale)), minimum, maximum);
}

[[nodiscard]] Font LoadConfiguredFont(
    const std::filesystem::path& path,
    int atlas_size,
    TextureFilter filter,
    std::string_view role,
    Logger& logger,
    bool& loaded)
{
    const std::string font_path = path.string();
    if (!FileExists(font_path.c_str())) {
        logger.Warn("assets", std::string(role) + " font not found path=\"" + font_path + "\", using default font");
        loaded = false;
        return GetFontDefault();
    }

    const std::vector<int> codepoints = BuildFontCodepoints();
    Font font = LoadFontEx(font_path.c_str(), atlas_size, const_cast<int*>(codepoints.data()), static_cast<int>(codepoints.size()));
    if (font.texture.id == 0) {
        logger.Warn("assets", std::string("failed to load ") + std::string(role) + " font path=\"" + font_path + "\", using default font");
        loaded = false;
        return GetFontDefault();
    }

    SetTextureFilter(font.texture, filter);
    loaded = true;
    logger.Info(
        "assets",
        std::string(role) + " font loaded path=\"" + font_path + "\" atlas_size=" + std::to_string(atlas_size));
    return font;
}

}  // namespace

void App::LoadUiFonts()
{
    title_font_ = LoadConfiguredFont(
        config_.ui_title_font_path,
        FontAtlasSize(38.0F, config_.ui_font_scale, 22, 72),
        TEXTURE_FILTER_BILINEAR,
        "title",
        logger_,
        title_font_loaded_);

    text_font_ = LoadConfiguredFont(
        config_.ui_text_font_path,
        FontAtlasSize(26.0F, config_.ui_font_scale, 16, 56),
        TEXTURE_FILTER_BILINEAR,
        "text",
        logger_,
        text_font_loaded_);
}

void App::UnloadUiFonts()
{
    if (title_font_loaded_) {
        UnloadFont(title_font_);
        title_font_loaded_ = false;
        title_font_ = Font{};
        logger_.Debug("assets", "title font unloaded");
    }
    if (text_font_loaded_) {
        UnloadFont(text_font_);
        text_font_loaded_ = false;
        text_font_ = Font{};
        logger_.Debug("assets", "text font unloaded");
    }
}

void App::RefreshProcessMemoryInfo()
{
    process_memory_ = QueryProcessMemoryInfo();
}

void App::LogSelectedItemChanged() const
{
    const MenuItem* item = main_menu_.SelectedItem();
    if (item == nullptr) {
        logger_.Debug("menu", "selected item changed selected=none");
        return;
    }
    logger_.Debug(
        "menu",
        "selected item changed index=" + std::to_string(main_menu_.State().selected_index) + " id="
            + std::string(ToString(item->id)));
}

UiFontSet App::UiFonts() const
{
    return UiFontSet{
        title_font_loaded_ ? title_font_ : GetFontDefault(),
        text_font_loaded_ ? text_font_ : GetFontDefault(),
    };
}

AppScreen App::CurrentScreen() const
{
    return screen_;
}

std::string_view ToString(AppScreen screen)
{
    switch (screen) {
        case AppScreen::kMainMenu:
            return "main_menu";
        case AppScreen::kWorkspace:
            return "main_render";
        case AppScreen::kSettingsPlaceholder:
            return "settings_placeholder";
    }
    return "unknown";
}

}  // namespace vox3d
