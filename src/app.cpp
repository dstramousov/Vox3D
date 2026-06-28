#include "app.hpp"

#include "ui_draw.hpp"
#include "vox3d/chunk/chunk_grid.hpp"
#include "vox3d/map/map_package.hpp"
#include "vox3d/map/runtime_map.hpp"
#include "vox3d/mesh/chunk_mesh_builder.hpp"
#include "vox3d/mesh/chunk_mesh_cache.hpp"
#include "vox3d/mesh/face_visibility.hpp"
#include "vox3d/mesh/terrain_mesh_builder.hpp"
#include "vox3d/transition/transition_feature.hpp"
#include "vox3d/validation/passability_validator.hpp"
#include "vox3d/voxel/voxel_world.hpp"

#include <raylib.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace vox3d {
namespace {

constexpr int kChunkSize16 = 16;
constexpr int kChunkSize32 = 32;

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

void ToggleOverlayFlag(
    bool& flag,
    std::string_view name,
    std::uint64_t primitive_count,
    Logger& logger,
    bool& layout_dirty)
{
    flag = !flag;
    layout_dirty = true;

    std::ostringstream out;
    out << name << '=' << (flag ? "on" : "off");
    out << " debug_primitives=" << primitive_count;
    out << " delta=" << (flag ? "+" : "-") << primitive_count;
    logger.Info("render3d", out.str());
}

[[nodiscard]] std::uint64_t WorldGridLineCount(const ChunkMeshBuildResult& mesh)
{
    if (!mesh.IsValid()) {
        return 0;
    }
    const int step = std::max(4, mesh.info.chunk_size_x > 0 ? mesh.info.chunk_size_x : 16);
    const auto x_lines = static_cast<std::uint64_t>((mesh.info.map_width + step) / step);
    const auto y_lines = static_cast<std::uint64_t>((mesh.info.map_height + step) / step);
    return x_lines + y_lines;
}

[[nodiscard]] std::uint64_t HeightMarkerCount(const RuntimeMap& map)
{
    if (!map.height.IsValid()) {
        return 0;
    }
    const int sample_step = std::max(1, std::max(map.info.width, map.info.height) / 96);
    const auto x_count = static_cast<std::uint64_t>((map.info.width + sample_step - 1) / sample_step);
    const auto y_count = static_cast<std::uint64_t>((map.info.height + sample_step - 1) / sample_step);
    return x_count * y_count;
}

[[nodiscard]] std::uint64_t OverlayPrimitiveCount(const WorkspaceState& workspace, WorkspacePanelItem item)
{
    switch (item) {
        case WorkspacePanelItem::kRenderChunkBounds:
            return workspace.chunk_grid.IsValid() ? static_cast<std::uint64_t>(workspace.chunk_grid.info.total_chunks) * 4ULL : 0;
        case WorkspacePanelItem::kRenderWorldGrid:
            return WorldGridLineCount(workspace.chunk_meshes);
        case WorkspacePanelItem::kRenderCollision:
            return workspace.runtime_map.info.collision_loaded
                ? static_cast<std::uint64_t>(workspace.runtime_map.info.blocked_cells)
                : 0;
        case WorkspacePanelItem::kRenderHeight:
            return HeightMarkerCount(workspace.runtime_map);
        default:
            return 0;
    }
}

[[nodiscard]] std::string PercentText(double ratio)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << ratio * 100.0 << '%';
    return out.str();
}

[[nodiscard]] int ToggleChunkSize(int chunk_size)
{
    return chunk_size == kChunkSize16 ? kChunkSize32 : kChunkSize16;
}

[[nodiscard]] WorkspaceColorMode NextColorMode(WorkspaceColorMode mode)
{
    switch (mode) {
        case WorkspaceColorMode::kMaterial:
            return WorkspaceColorMode::kGeographic;
        case WorkspaceColorMode::kGeographic:
            return WorkspaceColorMode::kChunkId;
        case WorkspaceColorMode::kChunkId:
            return WorkspaceColorMode::kFaceType;
        case WorkspaceColorMode::kFaceType:
            return WorkspaceColorMode::kMaterial;
    }
    return WorkspaceColorMode::kMaterial;
}

[[nodiscard]] RaylibChunkMeshColorMode ToRaylibColorMode(WorkspaceColorMode mode)
{
    switch (mode) {
        case WorkspaceColorMode::kMaterial:
            return RaylibChunkMeshColorMode::kMaterial;
        case WorkspaceColorMode::kGeographic:
            return RaylibChunkMeshColorMode::kGeographic;
        case WorkspaceColorMode::kChunkId:
            return RaylibChunkMeshColorMode::kChunkId;
        case WorkspaceColorMode::kFaceType:
            return RaylibChunkMeshColorMode::kFaceType;
    }
    return RaylibChunkMeshColorMode::kMaterial;
}

[[nodiscard]] WorkspaceVisibilityMode NextVisibilityMode(WorkspaceVisibilityMode mode)
{
    switch (mode) {
        case WorkspaceVisibilityMode::kAllChunks:
            return WorkspaceVisibilityMode::kRadiusFade;
        case WorkspaceVisibilityMode::kRadiusFade:
            return WorkspaceVisibilityMode::kHardCull;
        case WorkspaceVisibilityMode::kHardCull:
            return WorkspaceVisibilityMode::kFrustumCull;
        case WorkspaceVisibilityMode::kFrustumCull:
            return WorkspaceVisibilityMode::kAllChunks;
    }
    return WorkspaceVisibilityMode::kAllChunks;
}

[[nodiscard]] RaylibChunkVisibilityMode ToRaylibVisibilityMode(WorkspaceVisibilityMode mode)
{
    switch (mode) {
        case WorkspaceVisibilityMode::kAllChunks:
            return RaylibChunkVisibilityMode::kAllChunks;
        case WorkspaceVisibilityMode::kRadiusFade:
            return RaylibChunkVisibilityMode::kRadiusFade;
        case WorkspaceVisibilityMode::kHardCull:
            return RaylibChunkVisibilityMode::kHardCull;
        case WorkspaceVisibilityMode::kFrustumCull:
            return RaylibChunkVisibilityMode::kFrustumCull;
    }
    return RaylibChunkVisibilityMode::kAllChunks;
}

[[nodiscard]] float CurrentRenderAspectRatio()
{
    const int width = std::max(1, GetScreenWidth());
    const int height = std::max(1, GetScreenHeight());
    return static_cast<float>(width) / static_cast<float>(height);
}

[[nodiscard]] RaylibChunkVisibilityOptions BuildRaylibVisibilityOptions(
    const WorkspaceState& workspace,
    Rectangle /*viewport*/)
{
    return RaylibChunkVisibilityOptions{
        ToRaylibVisibilityMode(workspace.visibility_mode),
        workspace.visibility_radius_chunks,
        workspace.visibility_fade_ring_chunks,
        workspace.show_3d_hidden_chunk_bounds,
        CurrentRenderAspectRatio(),
    };
}

[[nodiscard]] RaylibTerrainPassOptions BuildRaylibTerrainPassOptions(const WorkspaceState& workspace)
{
    return RaylibTerrainPassOptions{
        workspace.show_terrain_tops,
        workspace.show_terrain_walls,
        workspace.show_terrain_cliffs,
    };
}

[[nodiscard]] WorkspaceVisibilityStats ToWorkspaceVisibilityStats(const RaylibChunkVisibilityStats& stats)
{
    WorkspaceVisibilityStats result;
    switch (stats.mode) {
        case RaylibChunkVisibilityMode::kAllChunks:
            result.mode = WorkspaceVisibilityMode::kAllChunks;
            break;
        case RaylibChunkVisibilityMode::kRadiusFade:
            result.mode = WorkspaceVisibilityMode::kRadiusFade;
            break;
        case RaylibChunkVisibilityMode::kHardCull:
            result.mode = WorkspaceVisibilityMode::kHardCull;
            break;
        case RaylibChunkVisibilityMode::kFrustumCull:
            result.mode = WorkspaceVisibilityMode::kFrustumCull;
            break;
    }
    result.radius_chunks = stats.radius_chunks;
    result.fade_ring_chunks = stats.fade_ring_chunks;
    result.resident_chunks = stats.resident_chunks;
    result.resident_models = stats.resident_models;
    result.visible_chunks = stats.visible_chunks;
    result.fade_chunks = stats.fade_chunks;
    result.hidden_chunks = stats.hidden_chunks;
    result.drawn_models = stats.drawn_models;
    result.culled_models = stats.culled_models;
    result.total_faces = stats.total_faces;
    result.drawn_faces = stats.drawn_faces;
    result.culled_faces = stats.culled_faces;
    return result;
}

[[nodiscard]] bool IsSupportedChunkSize(int chunk_size)
{
    return chunk_size == kChunkSize16 || chunk_size == kChunkSize32;
}

[[nodiscard]] std::string ChunkComparisonLogString(const WorkspaceChunkSizeComparison& comparison)
{
    if (!comparison.available) {
        return "status=unavailable";
    }

    std::ostringstream out;
    out << "before=" << comparison.before_chunk_size;
    out << " after=" << comparison.after_chunk_size;
    out << " chunks=" << comparison.before_total_chunks << "->" << comparison.after_total_chunks;
    out << " draw_models=" << comparison.before_draw_models << "->" << comparison.after_draw_models;
    out << " draw_delta=" << PercentText(comparison.DrawModelDeltaRatio());
    out << " faces=" << comparison.before_active_faces << "->" << comparison.after_active_faces;
    out << " face_delta=" << PercentText(comparison.FaceDeltaRatio());
    return out.str();
}

[[nodiscard]] ChunkMeshRebuildReport FullCacheBuildReport(const ChunkMeshCache& cache)
{
    ChunkMeshRebuildReport report;
    report.mode = cache.info.mode;
    report.attempted = true;
    report.valid = cache.IsValid();
    report.total_chunks = cache.info.total_chunks;
    report.dirty_chunks = static_cast<std::uint64_t>(cache.info.total_chunks);
    report.rebuilt_chunks = static_cast<std::uint64_t>(cache.info.total_chunks);
    report.reused_chunks = 0;
    report.old_faces = 0;
    report.new_faces = cache.info.faces;
    report.old_vertices = 0;
    report.new_vertices = cache.info.vertices;
    report.old_indices = 0;
    report.new_indices = cache.info.indices;
    return report;
}

[[nodiscard]] TileCoord CenterTile(const RuntimeMap& map)
{
    return TileCoord{map.info.width / 2, map.info.height / 2};
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
    SetExitKey(KEY_NULL);
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
    RebuildChunkPipeline(workspace_.chunk_size_tiles, "initial");
    main_menu_.SetItemEnabled(MenuItemId::kLoadGame, workspace_.map.loaded);

    LoadUiFonts();
    RefreshProcessMemoryInfo();
    RebuildLayout();
    if (chunk_mesh_preview_.IsUploaded()) {
        FitPreviewCameraToViewport("initial");
    }

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
        suppress_window_close_request_this_frame_ = false;

        const float dt = GetFrameTime();
        HandleInput(dt);
        Update(dt);

        const bool close_requested = WindowShouldClose();
        if (close_requested && suppress_window_close_request_this_frame_) {
            logger_.Debug("window", "close request suppressed after mouse release");
            window_close_request_armed_ = false;
        } else if (close_requested && window_close_request_armed_) {
            logger_.Info("window", "close requested");
            RequestExitConfirmation(true);
            window_close_request_armed_ = false;
        } else if (!close_requested) {
            window_close_request_armed_ = true;
        }

        Draw();
    }

    logger_.Info("app", "shutting down");
    return 0;
}

void App::Shutdown()
{
    preview_camera_.ReleaseMouse();
    UnloadPreviewResources();
    UnloadUiFonts();
    if (window_initialized_) {
        CloseWindow();
        window_initialized_ = false;
    }
}

void App::HandleInput(float dt)
{
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

    HandleScreenInput(dt);
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

void App::HandleScreenInput(float dt)
{
    if (screen_ == AppScreen::kWorkspace) {
        HandleWorkspaceInput(dt);
        return;
    }
    if (screen_ == AppScreen::kSettingsPlaceholder) {
        HandlePlaceholderInput();
    }
}

void App::HandleWorkspaceInput(float dt)
{
    const bool release_capture_by_escape = IsKeyPressed(KEY_ESCAPE);
    const bool release_capture_by_hotkey = IsKeyPressed(KEY_F2);
    if (workspace_.show_3d_preview && preview_camera_.IsCursorCaptured()
        && (release_capture_by_escape || release_capture_by_hotkey)) {
        preview_camera_.ReleaseMouse();
        if (release_capture_by_escape) {
            suppress_window_close_request_this_frame_ = true;
        }
        logger_.Debug(
            "camera3d",
            std::string("mouse capture released by ") + (release_capture_by_escape ? "escape" : "f2"));
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        RequestExitConfirmation();
        return;
    }

    if (IsKeyPressed(KEY_F3) && chunk_mesh_preview_.IsUploaded()) {
        workspace_.show_3d_preview = !workspace_.show_3d_preview;
        if (workspace_.show_3d_preview) {
            if (!preview_camera_.IsInitialized()) {
                FitPreviewCameraToViewport("hotkey");
            }
        } else {
            preview_camera_.ReleaseMouse();
        }
        layout_dirty_ = true;
        logger_.Info("workspace", std::string("preview mode=") + (workspace_.show_3d_preview ? "3d" : "2d"));
    }

    if (layout_dirty_) {
        RebuildLayout();
    }

    const Vector2 panel_mouse = GetMousePosition();
    const bool mouse_over_workspace_menu = workspace_.selected_panel_tab == WorkspacePanelTab::kMenu
        && PointInRect(panel_mouse, layout_cache_.workspace.tool_menu);
    if (!preview_camera_.IsCursorCaptured() && mouse_over_workspace_menu) {
        const float wheel = GetMouseWheelMove();
        if (wheel > 0.0001F) {
            ScrollWorkspaceMenu(-3, "wheel");
        } else if (wheel < -0.0001F) {
            ScrollWorkspaceMenu(3, "wheel");
        }
    }
    if (workspace_.selected_panel_tab == WorkspacePanelTab::kMenu) {
        if (IsKeyPressed(KEY_PAGE_UP)) {
            ScrollWorkspaceMenu(-6, "page_up");
        }
        if (IsKeyPressed(KEY_PAGE_DOWN)) {
            ScrollWorkspaceMenu(6, "page_down");
        }
        if (IsKeyPressed(KEY_HOME)) {
            ScrollWorkspaceMenu(-1000000, "home");
        }
        if (IsKeyPressed(KEY_END)) {
            ScrollWorkspaceMenu(1000000, "end");
        }
    }

    const bool camera_mode = workspace_.show_3d_preview && chunk_mesh_preview_.IsUploaded() && preview_camera_.IsInitialized();
    if (camera_mode) {
        if (IsKeyPressed(KEY_R)) {
            preview_camera_.ResetView();
            logger_.Info("camera3d", "reset view " + ToLogString(preview_camera_.Status()));
        }
        if (IsKeyPressed(KEY_F)) {
            FitPreviewCameraToViewport("hotkey");
        }
        if (IsKeyPressed(KEY_F4)) {
            ToggleOverlayFlag(
                workspace_.show_3d_chunk_bounds,
                "chunk_bounds",
                OverlayPrimitiveCount(workspace_, WorkspacePanelItem::kRenderChunkBounds),
                logger_,
                layout_dirty_);
        }
        if (IsKeyPressed(KEY_F5)) {
            ToggleOverlayFlag(
                workspace_.show_3d_world_grid,
                "world_grid",
                OverlayPrimitiveCount(workspace_, WorkspacePanelItem::kRenderWorldGrid),
                logger_,
                layout_dirty_);
        }
        if (IsKeyPressed(KEY_F6)) {
            ToggleOverlayFlag(
                workspace_.show_3d_collision_overlay,
                "collision_overlay",
                OverlayPrimitiveCount(workspace_, WorkspacePanelItem::kRenderCollision),
                logger_,
                layout_dirty_);
        }
        if (IsKeyPressed(KEY_F7)) {
            ToggleOverlayFlag(
                workspace_.show_3d_height_overlay,
                "height_overlay",
                OverlayPrimitiveCount(workspace_, WorkspacePanelItem::kRenderHeight),
                logger_,
                layout_dirty_);
        }
        if (IsKeyPressed(KEY_F8)) {
            ChunkMeshBuildMode next_mode = ChunkMeshBuildMode::kSimpleFaces;
            if (workspace_.mesh_mode == ChunkMeshBuildMode::kSimpleFaces) {
                next_mode = ChunkMeshBuildMode::kGreedyFaces;
            } else if (workspace_.mesh_mode == ChunkMeshBuildMode::kGreedyFaces) {
                next_mode = ChunkMeshBuildMode::kTerrainSurface;
            }
            SetMeshBuildMode(next_mode, "hotkey");
        }
        if (IsKeyPressed(KEY_F9)) {
            SetChunkSize(ToggleChunkSize(workspace_.chunk_size_tiles), "hotkey");
        }
        if (IsKeyPressed(KEY_F10)) {
            RunDirtyRebuildProbe("hotkey");
        }
        if (IsKeyPressed(KEY_F11)) {
            CycleColorMode("hotkey");
        }
        if (IsKeyPressed(KEY_F12)) {
            CycleVisibilityMode("hotkey");
        }
        if (IsKeyPressed(KEY_T)) {
            ToggleTransitionOverlay("hotkey");
        }
        if (IsKeyPressed(KEY_M)) {
            ToggleMovementProbeOverlay("hotkey");
        }
        if (IsKeyPressed(KEY_V)) {
            TogglePassabilityValidationOverlay("hotkey");
        }
        const Vector2 pick_mouse = GetMousePosition();
        if (!preview_camera_.IsCursorCaptured() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
            && PointInRect(pick_mouse, layout_cache_.workspace.map_overview)) {
            SelectTileAtMouse(pick_mouse, "mouse");
        }
        preview_camera_.Update(dt, layout_cache_.workspace.map_overview, true);
    } else {
        preview_camera_.Update(dt, layout_cache_.workspace.map_overview, false);
    }

    if (camera_mode) {
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_LEFT)) {
            SelectPreviousWorkspaceTool();
        }
        if (IsKeyPressedAny({KEY_DOWN, KEY_RIGHT, KEY_TAB})) {
            SelectNextWorkspaceTool();
        }
    } else {
        if (IsKeyPressedAny({KEY_UP, KEY_W, KEY_LEFT, KEY_A})) {
            SelectPreviousWorkspaceTool();
        }
        if (IsKeyPressedAny({KEY_DOWN, KEY_S, KEY_RIGHT, KEY_D, KEY_TAB})) {
            SelectNextWorkspaceTool();
        }
    }
    if (IsKeyPressed(KEY_ENTER) && workspace_.selected_panel_tab != WorkspacePanelTab::kMenu) {
        workspace_.selected_panel_tab = WorkspacePanelTab::kMenu;
        layout_dirty_ = true;
        logger_.Debug("workspace", "panel tab=" + std::string(ToString(workspace_.selected_panel_tab)));
    }

    if (layout_dirty_) {
        RebuildLayout();
    }

    const Vector2 mouse = GetMousePosition();
    for (const auto& tab_bounds : layout_cache_.workspace.panel_tabs) {
        if (!PointInRect(mouse, tab_bounds.bounds)) {
            continue;
        }

        hovered_item_ = "workspace_tab_" + std::string(ToString(tab_bounds.tab));
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && workspace_.selected_panel_tab != tab_bounds.tab) {
            workspace_.selected_panel_tab = tab_bounds.tab;
            layout_dirty_ = true;
            logger_.Debug("workspace", "panel tab=" + std::string(ToString(workspace_.selected_panel_tab)));
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

    UpdateVisibilityStats();

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
            DrawWorkspace(
                workspace_,
                &chunk_mesh_preview_,
                &preview_camera_.Camera(),
                preview_camera_.Status(),
                UiFonts(),
                labels_,
                layout_cache_);
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

void App::RefreshMeshOptimizationStats()
{
    workspace_.mesh_stats = MeshOptimizationStats{};
    workspace_.mesh_stats.active_mode = workspace_.mesh_mode;

    if (workspace_.face_visibility.IsValid()) {
        workspace_.mesh_stats.solid_blocks = workspace_.face_visibility.info.solid_blocks;
        workspace_.mesh_stats.naive_faces = workspace_.face_visibility.info.naive_faces;
        workspace_.mesh_stats.culled_faces = workspace_.face_visibility.info.culled_faces;
    }
    if (workspace_.simple_chunk_meshes.IsValid()) {
        workspace_.mesh_stats.simple_faces = workspace_.simple_chunk_meshes.info.visible_faces;
    }
    if (workspace_.greedy_chunk_meshes.IsValid()) {
        workspace_.mesh_stats.greedy_faces = workspace_.greedy_chunk_meshes.info.visible_faces;
    }
    if (workspace_.terrain_chunk_meshes.IsValid()) {
        workspace_.mesh_stats.terrain_faces = workspace_.terrain_chunk_meshes.info.visible_faces;
        workspace_.mesh_stats.terrain_raw_top_faces = workspace_.terrain_chunk_meshes.info.terrain_raw_top_faces;
        workspace_.mesh_stats.terrain_raw_wall_faces = workspace_.terrain_chunk_meshes.info.terrain_raw_wall_faces;
        workspace_.mesh_stats.terrain_top_faces = workspace_.terrain_chunk_meshes.info.terrain_top_faces;
        workspace_.mesh_stats.terrain_wall_faces = workspace_.terrain_chunk_meshes.info.terrain_wall_faces;
        workspace_.mesh_stats.terrain_cliff_faces = workspace_.terrain_chunk_meshes.info.terrain_cliff_faces;
    }
    if (workspace_.chunk_meshes.IsValid()) {
        workspace_.mesh_stats.active_faces = workspace_.chunk_meshes.info.visible_faces;
        workspace_.mesh_stats.active_vertices = workspace_.chunk_meshes.info.vertices;
        workspace_.mesh_stats.active_indices = workspace_.chunk_meshes.info.indices;
        workspace_.mesh_stats.mesh_chunks = workspace_.chunk_meshes.info.non_empty_chunks;
    }

    const RaylibChunkMeshPreviewStats& upload = chunk_mesh_preview_.Stats();
    workspace_.mesh_stats.draw_models = upload.models;
    workspace_.mesh_stats.skipped_chunks = upload.skipped_chunks;
}

void App::RefreshChunkSizeComparison(
    int before_chunk_size,
    const ChunkGridInfo& before_grid_info,
    const MeshOptimizationStats& before_stats,
    bool had_before_stats)
{
    workspace_.chunk_size_comparison = WorkspaceChunkSizeComparison{};
    if (!had_before_stats || !workspace_.chunk_grid.IsValid() || !workspace_.chunk_meshes.IsValid()) {
        return;
    }

    workspace_.chunk_size_comparison.available = true;
    workspace_.chunk_size_comparison.before_chunk_size = before_chunk_size;
    workspace_.chunk_size_comparison.after_chunk_size = workspace_.chunk_size_tiles;
    workspace_.chunk_size_comparison.before_total_chunks = before_grid_info.total_chunks;
    workspace_.chunk_size_comparison.after_total_chunks = workspace_.chunk_grid.info.total_chunks;
    workspace_.chunk_size_comparison.before_draw_models = before_stats.draw_models;
    workspace_.chunk_size_comparison.after_draw_models = workspace_.mesh_stats.draw_models;
    workspace_.chunk_size_comparison.before_active_faces = before_stats.active_faces;
    workspace_.chunk_size_comparison.after_active_faces = workspace_.mesh_stats.active_faces;
}

void App::RebuildChunkPipeline(int chunk_size, std::string_view reason)
{
    if (!IsSupportedChunkSize(chunk_size)) {
        logger_.Warn("chunk_pipeline", "unsupported chunk size=" + std::to_string(chunk_size));
        return;
    }

    const bool had_before_stats = workspace_.chunk_grid.IsValid() && workspace_.chunk_meshes.IsValid();
    const int before_chunk_size = workspace_.chunk_size_tiles;
    const ChunkGridInfo before_grid_info = workspace_.chunk_grid.info;
    const MeshOptimizationStats before_stats = workspace_.mesh_stats;

    ChunkGrid next_chunk_grid = BuildChunkGrid(workspace_.runtime_map, ChunkGridOptions{chunk_size, chunk_size});
    logger_.Info("chunk_grid", "reason=" + std::string(reason) + " " + ToLogString(next_chunk_grid));
    for (const auto& warning : next_chunk_grid.diagnostics.warnings) {
        logger_.Warn("chunk_grid", warning);
    }

    VoxelWorld next_voxel_world = BuildVoxelWorld(workspace_.runtime_map, next_chunk_grid);
    logger_.Info("voxel_world", "reason=" + std::string(reason) + " " + ToLogString(next_voxel_world));
    for (const auto& warning : next_voxel_world.diagnostics.warnings) {
        logger_.Warn("voxel_world", warning);
    }

    FaceVisibilityResult next_face_visibility = BuildFaceVisibility(next_voxel_world);
    logger_.Info("face_visibility", "reason=" + std::string(reason) + " " + ToLogString(next_face_visibility));
    for (const auto& warning : next_face_visibility.diagnostics.warnings) {
        logger_.Warn("face_visibility", warning);
    }

    ChunkMeshCache next_simple_cache = BuildChunkMeshCache(
        next_voxel_world,
        next_chunk_grid,
        ChunkMeshBuildMode::kSimpleFaces);
    logger_.Info("chunk_mesh_cache", "reason=" + std::string(reason) + " " + ToLogString(next_simple_cache));
    for (const auto& warning : next_simple_cache.diagnostics.warnings) {
        logger_.Warn("chunk_mesh_cache", warning);
    }

    ChunkMeshCache next_greedy_cache = BuildChunkMeshCache(
        next_voxel_world,
        next_chunk_grid,
        ChunkMeshBuildMode::kGreedyFaces);
    logger_.Info("chunk_mesh_cache", "reason=" + std::string(reason) + " " + ToLogString(next_greedy_cache));
    for (const auto& warning : next_greedy_cache.diagnostics.warnings) {
        logger_.Warn("chunk_mesh_cache", warning);
    }

    ChunkMeshBuildResult next_terrain_meshes = BuildTerrainChunkMeshes(workspace_.runtime_map, next_chunk_grid);
    logger_.Info("terrain_mesh", "reason=" + std::string(reason) + " " + ToLogString(next_terrain_meshes));
    for (const auto& warning : next_terrain_meshes.diagnostics.warnings) {
        logger_.Warn("terrain_mesh", warning);
    }

    TransitionFeatureSet next_transition_features = BuildTransitionFeatures(workspace_.runtime_map);
    logger_.Info("transitions", "reason=" + std::string(reason) + " " + ToLogString(next_transition_features));
    for (const auto& warning : next_transition_features.diagnostics.warnings) {
        logger_.Warn("transitions", warning);
    }

    workspace_.chunk_size_tiles = chunk_size;
    workspace_.chunk_grid = std::move(next_chunk_grid);
    workspace_.voxel_world = std::move(next_voxel_world);
    workspace_.face_visibility = std::move(next_face_visibility);
    workspace_.simple_chunk_mesh_cache = std::move(next_simple_cache);
    workspace_.greedy_chunk_mesh_cache = std::move(next_greedy_cache);
    workspace_.terrain_chunk_meshes = std::move(next_terrain_meshes);
    workspace_.transition_features = std::move(next_transition_features);
    if (workspace_.validation_mode == WorkspaceValidationMode::kOff) {
        ClearPassabilityValidation("chunk_pipeline_disabled");
    } else if (workspace_.passability_validation_dirty) {
        if (workspace_.validation_mode == WorkspaceValidationMode::kOnLoad) {
            RunPassabilityValidation(reason);
        } else {
            workspace_.passability_validation = PassabilityValidationReport{};
            workspace_.passability_validation_status = WorkspaceValidationStatus::kNotRun;
            workspace_.passability_validation_last_run_ms = 0.0;
            workspace_.show_passability_issues = false;
            logger_.Info(
                "passability",
                "status=not_run mode=" + std::string(ToString(workspace_.validation_mode))
                    + " reason=" + std::string(reason));
        }
    }
    if (workspace_.selected_tile.IsValid()) {
        workspace_.selected_tile = InspectTile(
            workspace_.runtime_map,
            workspace_.chunk_grid,
            workspace_.transition_features,
            workspace_.selected_tile.tile);
        workspace_.movement_probe = BuildMovementProbe(
            workspace_.runtime_map,
            workspace_.transition_features,
            workspace_.selected_tile.tile);
    } else {
        workspace_.movement_probe = MovementProbeResult{};
    }
    workspace_.simple_chunk_meshes = ToChunkMeshBuildResult(workspace_.simple_chunk_mesh_cache);
    workspace_.greedy_chunk_meshes = ToChunkMeshBuildResult(workspace_.greedy_chunk_mesh_cache);
    SetActiveMeshCacheFromMode();
    workspace_.last_mesh_rebuild = workspace_.mesh_mode == ChunkMeshBuildMode::kTerrainSurface
        ? ChunkMeshRebuildReport{}
        : FullCacheBuildReport(workspace_.chunk_mesh_cache);

    UploadActiveChunkMesh(reason);
    RefreshChunkSizeComparison(before_chunk_size, before_grid_info, before_stats, had_before_stats);
    if (workspace_.chunk_size_comparison.available) {
        logger_.Info("chunk_profit", ChunkComparisonLogString(workspace_.chunk_size_comparison));
    }

    layout_dirty_ = true;
}

void App::SetChunkSize(int chunk_size, std::string_view reason)
{
    if (!IsSupportedChunkSize(chunk_size)) {
        logger_.Warn("chunk_pipeline", "chunk size switch ignored unsupported size=" + std::to_string(chunk_size));
        return;
    }
    if (workspace_.chunk_size_tiles == chunk_size && workspace_.chunk_grid.IsValid()) {
        logger_.Debug("chunk_pipeline", "chunk size unchanged size=" + std::to_string(chunk_size));
        return;
    }

    RebuildChunkPipeline(chunk_size, reason);
}

void App::SetActiveMeshCacheFromMode()
{
    if (workspace_.mesh_mode == ChunkMeshBuildMode::kTerrainSurface) {
        workspace_.chunk_mesh_cache = ChunkMeshCache{};
        workspace_.chunk_meshes = workspace_.terrain_chunk_meshes;
    } else if (workspace_.mesh_mode == ChunkMeshBuildMode::kGreedyFaces) {
        workspace_.chunk_mesh_cache = workspace_.greedy_chunk_mesh_cache;
        workspace_.chunk_meshes = workspace_.greedy_chunk_meshes;
    } else {
        workspace_.chunk_mesh_cache = workspace_.simple_chunk_mesh_cache;
        workspace_.chunk_meshes = workspace_.simple_chunk_meshes;
    }
}

void App::RunDirtyRebuildProbe(std::string_view reason)
{
    if (!workspace_.runtime_map.IsValid() || !workspace_.chunk_grid.IsValid()) {
        logger_.Warn("dirty_rebuild", "probe ignored because runtime map or chunk grid is invalid");
        return;
    }
    if (workspace_.mesh_mode == ChunkMeshBuildMode::kTerrainSurface) {
        logger_.Warn("dirty_rebuild", "probe ignored because terrain surface meshes do not use voxel chunk cache yet");
        return;
    }

    ChunkMeshCache* active_cache = workspace_.mesh_mode == ChunkMeshBuildMode::kGreedyFaces
        ? &workspace_.greedy_chunk_mesh_cache
        : &workspace_.simple_chunk_mesh_cache;
    if (!active_cache->IsValid()) {
        logger_.Warn("dirty_rebuild", "probe ignored because active cache is invalid");
        return;
    }

    const TileCoord tile = CenterTile(workspace_.runtime_map);
    const std::uint64_t marked = active_cache->MarkTileAndBorderChunksDirty(tile, workspace_.chunk_grid);
    workspace_.last_mesh_rebuild = RebuildDirtyChunkMeshes(workspace_.voxel_world, workspace_.chunk_grid, active_cache);

    if (workspace_.mesh_mode == ChunkMeshBuildMode::kGreedyFaces) {
        workspace_.greedy_chunk_meshes = ToChunkMeshBuildResult(workspace_.greedy_chunk_mesh_cache);
    } else {
        workspace_.simple_chunk_meshes = ToChunkMeshBuildResult(workspace_.simple_chunk_mesh_cache);
    }
    SetActiveMeshCacheFromMode();
    UploadActiveChunkMesh(reason);

    std::ostringstream out;
    out << "reason=" << reason;
    out << " probe_tile=" << tile.x << ',' << tile.y;
    out << " newly_marked=" << marked;
    out << ' ' << ToLogString(workspace_.last_mesh_rebuild);
    logger_.Info("dirty_rebuild", out.str());
    for (const auto& warning : workspace_.last_mesh_rebuild.diagnostics.warnings) {
        logger_.Warn("dirty_rebuild", warning);
    }

    layout_dirty_ = true;
}

void App::UploadActiveChunkMesh(std::string_view reason)
{
    const bool uploaded = chunk_mesh_preview_.Upload(workspace_.chunk_meshes, ToRaylibColorMode(workspace_.color_mode));
    RefreshMeshOptimizationStats();
    UpdateVisibilityStats();
    logger_.Info(
        "render3d",
        "upload reason=" + std::string(reason) + " color=" + std::string(ToString(workspace_.color_mode)) + " "
            + ToLogString(chunk_mesh_preview_.Stats()));
    logger_.Info("mesh_stats", ToLogString(workspace_.mesh_stats));
    if (!uploaded) {
        logger_.Warn("render3d", "3D preview mesh upload failed or produced no drawable chunks");
    }
}

void App::SetMeshBuildMode(ChunkMeshBuildMode mode, std::string_view reason)
{
    if (workspace_.mesh_mode == mode && workspace_.chunk_meshes.IsValid()) {
        logger_.Debug("mesh_stats", "mesh mode unchanged mode=" + std::string(ToString(mode)));
        return;
    }

    const ChunkMeshBuildResult& selected = mode == ChunkMeshBuildMode::kTerrainSurface
        ? workspace_.terrain_chunk_meshes
        : (mode == ChunkMeshBuildMode::kGreedyFaces ? workspace_.greedy_chunk_meshes : workspace_.simple_chunk_meshes);
    if (!selected.IsValid()) {
        logger_.Warn("mesh_stats", "mesh mode switch ignored invalid mode=" + std::string(ToString(mode)));
        return;
    }

    workspace_.mesh_mode = mode;
    SetActiveMeshCacheFromMode();
    UploadActiveChunkMesh(reason);
    layout_dirty_ = true;
}

void App::SetColorMode(WorkspaceColorMode mode, std::string_view reason)
{
    if (workspace_.color_mode == mode && chunk_mesh_preview_.IsUploaded()) {
        logger_.Debug("render3d", "color mode unchanged mode=" + std::string(ToString(mode)));
        return;
    }

    workspace_.color_mode = mode;
    if (workspace_.chunk_meshes.IsValid()) {
        UploadActiveChunkMesh(reason);
    }
    layout_dirty_ = true;
    logger_.Info("render3d", "color mode=" + std::string(ToString(mode)) + " reason=" + std::string(reason));
}

void App::CycleColorMode(std::string_view reason)
{
    SetColorMode(NextColorMode(workspace_.color_mode), reason);
}

void App::SetVisibilityMode(WorkspaceVisibilityMode mode, std::string_view reason)
{
    if (workspace_.visibility_mode == mode) {
        logger_.Debug("visibility", "mode unchanged mode=" + std::string(ToString(mode)));
        return;
    }

    workspace_.visibility_mode = mode;
    UpdateVisibilityStats();
    layout_dirty_ = true;
    logger_.Info("visibility", "reason=" + std::string(reason) + " " + ToLogString(chunk_mesh_preview_.CalculateVisibilityStats(
        workspace_.chunk_meshes,
        preview_camera_.Camera(),
        BuildRaylibVisibilityOptions(workspace_, layout_cache_.workspace.map_overview),
        BuildRaylibTerrainPassOptions(workspace_))));
}

void App::CycleVisibilityMode(std::string_view reason)
{
    SetVisibilityMode(NextVisibilityMode(workspace_.visibility_mode), reason);
}

void App::AdjustVisibilityRadius(int delta, std::string_view reason)
{
    constexpr int kMinRadius = 0;
    constexpr int kMaxRadius = 12;
    const int next_radius = std::clamp(workspace_.visibility_radius_chunks + delta, kMinRadius, kMaxRadius);
    if (next_radius == workspace_.visibility_radius_chunks) {
        logger_.Debug("visibility", "radius unchanged radius=" + std::to_string(workspace_.visibility_radius_chunks));
        return;
    }

    workspace_.visibility_radius_chunks = next_radius;
    UpdateVisibilityStats();
    layout_dirty_ = true;
    logger_.Info("visibility", "reason=" + std::string(reason) + " radius=" + std::to_string(next_radius) + " "
        + ToLogString(chunk_mesh_preview_.CalculateVisibilityStats(
            workspace_.chunk_meshes,
            preview_camera_.Camera(),
            BuildRaylibVisibilityOptions(workspace_, layout_cache_.workspace.map_overview),
        BuildRaylibTerrainPassOptions(workspace_))));
}

void App::AdjustVisibilityFadeRing(int delta, std::string_view reason)
{
    constexpr int kMinFadeRing = 0;
    constexpr int kMaxFadeRing = 4;
    const int next_fade_ring = std::clamp(workspace_.visibility_fade_ring_chunks + delta, kMinFadeRing, kMaxFadeRing);
    if (next_fade_ring == workspace_.visibility_fade_ring_chunks) {
        logger_.Debug("visibility", "fade ring unchanged fade_ring=" + std::to_string(workspace_.visibility_fade_ring_chunks));
        return;
    }

    workspace_.visibility_fade_ring_chunks = next_fade_ring;
    UpdateVisibilityStats();
    layout_dirty_ = true;
    logger_.Info("visibility", "reason=" + std::string(reason) + " fade_ring=" + std::to_string(next_fade_ring) + " "
        + ToLogString(chunk_mesh_preview_.CalculateVisibilityStats(
            workspace_.chunk_meshes,
            preview_camera_.Camera(),
            BuildRaylibVisibilityOptions(workspace_, layout_cache_.workspace.map_overview),
        BuildRaylibTerrainPassOptions(workspace_))));
}

void App::UpdateVisibilityStats()
{
    if (!chunk_mesh_preview_.IsUploaded() || !workspace_.chunk_meshes.IsValid()) {
        workspace_.visibility_stats = WorkspaceVisibilityStats{};
        workspace_.visibility_stats.mode = workspace_.visibility_mode;
        workspace_.visibility_stats.radius_chunks = workspace_.visibility_radius_chunks;
        workspace_.visibility_stats.fade_ring_chunks = workspace_.visibility_fade_ring_chunks;
        return;
    }

    workspace_.visibility_stats = ToWorkspaceVisibilityStats(chunk_mesh_preview_.CalculateVisibilityStats(
        workspace_.chunk_meshes,
        preview_camera_.Camera(),
        BuildRaylibVisibilityOptions(workspace_, layout_cache_.workspace.map_overview),
        BuildRaylibTerrainPassOptions(workspace_)));
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
    switch (workspace_.selected_panel_tab) {
        case WorkspacePanelTab::kMenu:
            workspace_.selected_panel_tab = WorkspacePanelTab::kHelp;
            break;
        case WorkspacePanelTab::kStats:
            workspace_.selected_panel_tab = WorkspacePanelTab::kMenu;
            break;
        case WorkspacePanelTab::kInspect:
            workspace_.selected_panel_tab = WorkspacePanelTab::kStats;
            break;
        case WorkspacePanelTab::kHelp:
            workspace_.selected_panel_tab = WorkspacePanelTab::kInspect;
            break;
    }
    layout_dirty_ = true;
    logger_.Debug("workspace", "panel tab=" + std::string(ToString(workspace_.selected_panel_tab)));
}

void App::SelectNextWorkspaceTool()
{
    switch (workspace_.selected_panel_tab) {
        case WorkspacePanelTab::kMenu:
            workspace_.selected_panel_tab = WorkspacePanelTab::kStats;
            break;
        case WorkspacePanelTab::kStats:
            workspace_.selected_panel_tab = WorkspacePanelTab::kInspect;
            break;
        case WorkspacePanelTab::kInspect:
            workspace_.selected_panel_tab = WorkspacePanelTab::kHelp;
            break;
        case WorkspacePanelTab::kHelp:
            workspace_.selected_panel_tab = WorkspacePanelTab::kMenu;
            break;
    }
    layout_dirty_ = true;
    logger_.Debug("workspace", "panel tab=" + std::string(ToString(workspace_.selected_panel_tab)));
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

void App::ToggleTransitionOverlay(std::string_view reason)
{
    if (!workspace_.transition_features.IsValid() || workspace_.transition_features.features.empty()) {
        logger_.Debug("transitions", "overlay toggle ignored because no transition features are available");
        return;
    }

    workspace_.show_transition_overlay = !workspace_.show_transition_overlay;
    layout_dirty_ = true;
    logger_.Info("transitions", std::string("overlay=")
        + (workspace_.show_transition_overlay ? "on" : "off")
        + " reason=" + std::string(reason));
}

void App::ToggleMovementProbeOverlay(std::string_view reason)
{
    if (!workspace_.selected_tile.IsValid() || !workspace_.movement_probe.IsValid()) {
        logger_.Debug("movement", "probe overlay toggle ignored because no tile is selected");
        return;
    }

    workspace_.show_movement_probe = !workspace_.show_movement_probe;
    layout_dirty_ = true;
    logger_.Info("movement", std::string("probe_overlay=")
        + (workspace_.show_movement_probe ? "on" : "off")
        + " reason=" + std::string(reason));
}

void App::SetValidationMode(WorkspaceValidationMode mode, std::string_view reason)
{
    if (workspace_.validation_mode == mode) {
        logger_.Debug("passability", "validation mode unchanged mode=" + std::string(ToString(mode)));
        return;
    }

    workspace_.validation_mode = mode;
    if (mode == WorkspaceValidationMode::kOff) {
        ClearPassabilityValidation(reason);
    } else if (workspace_.passability_validation_status == WorkspaceValidationStatus::kDisabled) {
        workspace_.passability_validation_status = WorkspaceValidationStatus::kNotRun;
        workspace_.passability_validation_dirty = true;
    }

    logger_.Info(
        "passability",
        "mode=" + std::string(ToString(workspace_.validation_mode)) + " reason=" + std::string(reason));

    if (mode == WorkspaceValidationMode::kOnLoad && workspace_.passability_validation_dirty
        && workspace_.runtime_map.IsValid() && workspace_.transition_features.IsValid()) {
        RunPassabilityValidation("mode_on_load");
    }

    layout_dirty_ = true;
}

void App::RunPassabilityValidation(std::string_view reason)
{
    if (workspace_.validation_mode == WorkspaceValidationMode::kOff) {
        logger_.Debug("passability", "validation run ignored because validation mode is off");
        return;
    }
    if (!workspace_.runtime_map.IsValid() || !workspace_.transition_features.IsValid()) {
        logger_.Warn("passability", "validation run ignored because map or transition features are unavailable");
        return;
    }

    const auto start = std::chrono::steady_clock::now();
    PassabilityValidationReport next_passability_validation = ValidatePassability(
        workspace_.runtime_map,
        workspace_.transition_features);
    const auto finish = std::chrono::steady_clock::now();
    const std::chrono::duration<double, std::milli> elapsed = finish - start;

    workspace_.passability_validation_last_run_ms = elapsed.count();
    workspace_.passability_validation_status = WorkspaceValidationStatus::kDone;
    workspace_.passability_validation_dirty = false;
    workspace_.passability_validation = std::move(next_passability_validation);

    std::ostringstream out;
    out << "reason=" << reason << ' ' << ToLogString(workspace_.passability_validation)
        << " duration_ms=" << std::fixed << std::setprecision(2)
        << workspace_.passability_validation_last_run_ms;
    logger_.Info("passability", out.str());
    for (const auto& warning : workspace_.passability_validation.diagnostics.warnings) {
        logger_.Warn("passability", warning);
    }

    layout_dirty_ = true;
}

void App::ClearPassabilityValidation(std::string_view reason)
{
    workspace_.passability_validation = PassabilityValidationReport{};
    workspace_.passability_validation_last_run_ms = 0.0;
    workspace_.show_passability_issues = false;
    if (workspace_.validation_mode == WorkspaceValidationMode::kOff) {
        workspace_.passability_validation_status = WorkspaceValidationStatus::kDisabled;
        workspace_.passability_validation_dirty = false;
    } else {
        workspace_.passability_validation_status = WorkspaceValidationStatus::kNotRun;
        workspace_.passability_validation_dirty = workspace_.runtime_map.IsValid() && workspace_.transition_features.IsValid();
    }

    logger_.Info(
        "passability",
        "clear reason=" + std::string(reason)
            + " status=" + std::string(ToString(workspace_.passability_validation_status)));
    layout_dirty_ = true;
}

void App::TogglePassabilityValidationOverlay(std::string_view reason)
{
    if (!workspace_.passability_validation.IsValid() || workspace_.passability_validation.issues.empty()) {
        logger_.Debug("passability", "overlay toggle ignored because no validation issues are available");
        return;
    }

    workspace_.show_passability_issues = !workspace_.show_passability_issues;
    layout_dirty_ = true;
    logger_.Info("passability", std::string("overlay=")
        + (workspace_.show_passability_issues ? "on" : "off")
        + " reason=" + std::string(reason));
}

void App::SelectTileAtMouse(Vector2 mouse, std::string_view reason)
{
    if (!workspace_.show_3d_preview || !chunk_mesh_preview_.IsUploaded() || !workspace_.runtime_map.IsValid()) {
        return;
    }

    const auto picked_tile = chunk_mesh_preview_.PickTile(
        mouse,
        layout_cache_.workspace.map_overview,
        workspace_.runtime_map,
        preview_camera_.Camera());
    if (!picked_tile.has_value()) {
        workspace_.selected_tile = TileInspectResult{};
        workspace_.movement_probe = MovementProbeResult{};
        workspace_.selected_panel_tab = WorkspacePanelTab::kInspect;
        layout_dirty_ = true;
        logger_.Debug("inspect", "tile pick missed reason=" + std::string(reason));
        return;
    }

    workspace_.selected_tile = InspectTile(
        workspace_.runtime_map,
        workspace_.chunk_grid,
        workspace_.transition_features,
        *picked_tile);
    workspace_.movement_probe = BuildMovementProbe(
        workspace_.runtime_map,
        workspace_.transition_features,
        workspace_.selected_tile.tile);
    workspace_.selected_panel_tab = WorkspacePanelTab::kInspect;
    layout_dirty_ = true;
    logger_.Info("inspect", "reason=" + std::string(reason) + " " + ToLogString(workspace_.selected_tile));
    logger_.Info("movement", "reason=" + std::string(reason) + " " + ToLogString(workspace_.movement_probe));
}

void App::ScrollWorkspaceMenu(int delta_rows, std::string_view reason)
{
    if (workspace_.selected_panel_tab != WorkspacePanelTab::kMenu || delta_rows == 0) {
        return;
    }

    const WorkspaceLayout& workspace_layout = layout_cache_.workspace;
    const int max_scroll_rows = std::max(
        0,
        workspace_layout.panel_total_rows - workspace_layout.panel_visible_rows);
    const int next_scroll = std::clamp(
        workspace_.menu_scroll_rows + delta_rows,
        0,
        max_scroll_rows);
    if (next_scroll == workspace_.menu_scroll_rows) {
        return;
    }

    workspace_.menu_scroll_rows = next_scroll;
    layout_dirty_ = true;
    logger_.Debug("workspace", "menu_scroll=" + std::to_string(workspace_.menu_scroll_rows)
        + " reason=" + std::string(reason));
}

void App::ActivateWorkspacePanelItem(WorkspacePanelItem item)
{
    bool activatable = false;
    for (const WorkspacePanelItemState& state : BuildWorkspacePanelItems(workspace_)) {
        if (state.item != item) {
            continue;
        }
        activatable = state.enabled
            && (state.kind == WorkspacePanelItemKind::kAction
                || state.kind == WorkspacePanelItemKind::kCheckbox
                || state.kind == WorkspacePanelItemKind::kRadio);
        break;
    }

    if (!activatable) {
        logger_.Debug("workspace", "inactive panel item ignored id=" + std::string(ToString(item)));
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
        case WorkspacePanelItem::kMode2DMap:
            workspace_.show_3d_preview = false;
            preview_camera_.ReleaseMouse();
            logger_.Info("workspace", "preview mode=2d");
            break;
        case WorkspacePanelItem::kMode3DWorld:
        case WorkspacePanelItem::kRenderTerrainMesh:
            workspace_.show_3d_preview = chunk_mesh_preview_.IsUploaded();
            if (workspace_.show_3d_preview && !preview_camera_.IsInitialized()) {
                FitPreviewCameraToViewport("panel");
            }
            logger_.Info("workspace", std::string("preview mode=") + (workspace_.show_3d_preview ? "3d" : "3d_unavailable"));
            break;
        case WorkspacePanelItem::kViewFitMap:
            FitPreviewCameraToViewport("panel");
            break;
        case WorkspacePanelItem::kViewResetView:
            preview_camera_.ResetView();
            logger_.Info("camera3d", "reset view " + ToLogString(preview_camera_.Status()));
            break;
        case WorkspacePanelItem::kRenderChunkBounds:
            ToggleOverlayFlag(
                workspace_.show_3d_chunk_bounds,
                "chunk_bounds",
                OverlayPrimitiveCount(workspace_, item),
                logger_,
                layout_dirty_);
            break;
        case WorkspacePanelItem::kRenderWorldGrid:
            ToggleOverlayFlag(
                workspace_.show_3d_world_grid,
                "world_grid",
                OverlayPrimitiveCount(workspace_, item),
                logger_,
                layout_dirty_);
            break;
        case WorkspacePanelItem::kRenderCollision:
            ToggleOverlayFlag(
                workspace_.show_3d_collision_overlay,
                "collision_overlay",
                OverlayPrimitiveCount(workspace_, item),
                logger_,
                layout_dirty_);
            break;
        case WorkspacePanelItem::kRenderHeight:
            ToggleOverlayFlag(
                workspace_.show_3d_height_overlay,
                "height_overlay",
                OverlayPrimitiveCount(workspace_, item),
                logger_,
                layout_dirty_);
            break;
        case WorkspacePanelItem::k3DColorMaterial:
            SetColorMode(WorkspaceColorMode::kMaterial, "panel");
            break;
        case WorkspacePanelItem::k3DColorGeographic:
            SetColorMode(WorkspaceColorMode::kGeographic, "panel");
            break;
        case WorkspacePanelItem::k3DColorChunkId:
            SetColorMode(WorkspaceColorMode::kChunkId, "panel");
            break;
        case WorkspacePanelItem::k3DColorFaceType:
            SetColorMode(WorkspaceColorMode::kFaceType, "panel");
            break;
        case WorkspacePanelItem::k3DVisibilityAllChunks:
            SetVisibilityMode(WorkspaceVisibilityMode::kAllChunks, "panel");
            break;
        case WorkspacePanelItem::k3DVisibilityRadiusFade:
            SetVisibilityMode(WorkspaceVisibilityMode::kRadiusFade, "panel");
            break;
        case WorkspacePanelItem::k3DVisibilityHardCull:
            SetVisibilityMode(WorkspaceVisibilityMode::kHardCull, "panel");
            break;
        case WorkspacePanelItem::k3DVisibilityFrustumCull:
            SetVisibilityMode(WorkspaceVisibilityMode::kFrustumCull, "panel");
            break;
        case WorkspacePanelItem::k3DVisibilityRadiusMinus:
            AdjustVisibilityRadius(-1, "panel");
            break;
        case WorkspacePanelItem::k3DVisibilityRadiusPlus:
            AdjustVisibilityRadius(1, "panel");
            break;
        case WorkspacePanelItem::k3DVisibilityFadeMinus:
            AdjustVisibilityFadeRing(-1, "panel");
            break;
        case WorkspacePanelItem::k3DVisibilityFadePlus:
            AdjustVisibilityFadeRing(1, "panel");
            break;
        case WorkspacePanelItem::k3DShowHiddenBounds:
            workspace_.show_3d_hidden_chunk_bounds = !workspace_.show_3d_hidden_chunk_bounds;
            UpdateVisibilityStats();
            logger_.Info("visibility", std::string("hidden_bounds=")
                + (workspace_.show_3d_hidden_chunk_bounds ? "on" : "off"));
            break;
        case WorkspacePanelItem::k3DTerrainPassTops:
            workspace_.show_terrain_tops = !workspace_.show_terrain_tops;
            UpdateVisibilityStats();
            layout_dirty_ = true;
            logger_.Info("render3d", std::string("terrain_pass_tops=")
                + (workspace_.show_terrain_tops ? "on" : "off"));
            break;
        case WorkspacePanelItem::k3DTerrainPassWalls:
            workspace_.show_terrain_walls = !workspace_.show_terrain_walls;
            UpdateVisibilityStats();
            layout_dirty_ = true;
            logger_.Info("render3d", std::string("terrain_pass_walls=")
                + (workspace_.show_terrain_walls ? "on" : "off"));
            break;
        case WorkspacePanelItem::k3DTerrainPassCliffs:
            workspace_.show_terrain_cliffs = !workspace_.show_terrain_cliffs;
            UpdateVisibilityStats();
            layout_dirty_ = true;
            logger_.Info("render3d", std::string("terrain_pass_cliffs=")
                + (workspace_.show_terrain_cliffs ? "on" : "off"));
            break;
        case WorkspacePanelItem::k3DShowTransitions:
            ToggleTransitionOverlay("panel");
            break;
        case WorkspacePanelItem::k3DTransitionRamps:
            workspace_.show_transition_ramps = !workspace_.show_transition_ramps;
            layout_dirty_ = true;
            logger_.Info("transitions", std::string("ramps=")
                + (workspace_.show_transition_ramps ? "on" : "off"));
            break;
        case WorkspacePanelItem::k3DTransitionStairs:
            workspace_.show_transition_stairs = !workspace_.show_transition_stairs;
            layout_dirty_ = true;
            logger_.Info("transitions", std::string("stairs=")
                + (workspace_.show_transition_stairs ? "on" : "off"));
            break;
        case WorkspacePanelItem::k3DTransitionBridges:
            workspace_.show_transition_bridges = !workspace_.show_transition_bridges;
            layout_dirty_ = true;
            logger_.Info("transitions", std::string("bridges=")
                + (workspace_.show_transition_bridges ? "on" : "off"));
            break;
        case WorkspacePanelItem::k3DTransitionDrops:
            workspace_.show_transition_drops = !workspace_.show_transition_drops;
            layout_dirty_ = true;
            logger_.Info("transitions", std::string("drops=")
                + (workspace_.show_transition_drops ? "on" : "off"));
            break;
        case WorkspacePanelItem::k3DShowMovementProbe:
            ToggleMovementProbeOverlay("panel");
            break;
        case WorkspacePanelItem::k3DValidationModeOff:
            SetValidationMode(WorkspaceValidationMode::kOff, "panel");
            break;
        case WorkspacePanelItem::k3DValidationModeManual:
            SetValidationMode(WorkspaceValidationMode::kManual, "panel");
            break;
        case WorkspacePanelItem::k3DValidationModeOnLoad:
            SetValidationMode(WorkspaceValidationMode::kOnLoad, "panel");
            break;
        case WorkspacePanelItem::k3DRunPassabilityValidation:
            RunPassabilityValidation("panel");
            break;
        case WorkspacePanelItem::k3DClearPassabilityValidation:
            ClearPassabilityValidation("panel");
            break;
        case WorkspacePanelItem::k3DShowPassabilityIssues:
            TogglePassabilityValidationOverlay("panel");
            break;
        case WorkspacePanelItem::k3DValidationInvalidTransitions:
            workspace_.show_passability_invalid_transitions = !workspace_.show_passability_invalid_transitions;
            layout_dirty_ = true;
            logger_.Info("passability", std::string("invalid_transitions=")
                + (workspace_.show_passability_invalid_transitions ? "on" : "off"));
            break;
        case WorkspacePanelItem::k3DValidationBlockedTransitions:
            workspace_.show_passability_blocked_transitions = !workspace_.show_passability_blocked_transitions;
            layout_dirty_ = true;
            logger_.Info("passability", std::string("blocked_transitions=")
                + (workspace_.show_passability_blocked_transitions ? "on" : "off"));
            break;
        case WorkspacePanelItem::k3DValidationSuspiciousDrops:
            workspace_.show_passability_suspicious_drops = !workspace_.show_passability_suspicious_drops;
            layout_dirty_ = true;
            logger_.Info("passability", std::string("suspicious_drops=")
                + (workspace_.show_passability_suspicious_drops ? "on" : "off"));
            break;
        case WorkspacePanelItem::k3DValidationIsolatedTiles:
            workspace_.show_passability_isolated_tiles = !workspace_.show_passability_isolated_tiles;
            layout_dirty_ = true;
            logger_.Info("passability", std::string("isolated_tiles=")
                + (workspace_.show_passability_isolated_tiles ? "on" : "off"));
            break;
        case WorkspacePanelItem::k3DMeshSimple:
            SetMeshBuildMode(ChunkMeshBuildMode::kSimpleFaces, "panel");
            break;
        case WorkspacePanelItem::k3DChunkSize16:
            SetChunkSize(kChunkSize16, "panel");
            break;
        case WorkspacePanelItem::k3DChunkSize32:
            SetChunkSize(kChunkSize32, "panel");
            break;
        case WorkspacePanelItem::k3DMeshGreedy:
            SetMeshBuildMode(ChunkMeshBuildMode::kGreedyFaces, "panel");
            break;
        case WorkspacePanelItem::k3DMeshTerrainSurface:
            SetMeshBuildMode(ChunkMeshBuildMode::kTerrainSurface, "panel");
            break;
        case WorkspacePanelItem::k3DDirtyRebuildProbe:
            RunDirtyRebuildProbe("panel");
            break;
        default:
            break;
    }

    layout_dirty_ = true;
    logger_.Debug("workspace", "panel item activated id=" + std::string(ToString(item)));
}



void App::FitPreviewCameraToViewport(std::string_view reason)
{
    if (!chunk_mesh_preview_.IsUploaded() || !workspace_.chunk_meshes.IsValid()) {
        logger_.Debug("camera3d", "fit ignored reason=" + std::string(reason));
        return;
    }
    if (layout_dirty_) {
        RebuildLayout();
    }
    preview_camera_.FitToMap(workspace_.chunk_meshes, layout_cache_.workspace.map_overview);
    logger_.Info("camera3d", "fit map reason=" + std::string(reason) + " " + ToLogString(preview_camera_.Status()));
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
    preview_camera_.ReleaseMouse();
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


void App::UnloadPreviewResources()
{
    if (chunk_mesh_preview_.IsUploaded()) {
        chunk_mesh_preview_.Unload();
        logger_.Debug("render3d", "preview resources unloaded");
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
