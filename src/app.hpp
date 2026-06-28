#pragma once

#include "app_config.hpp"
#include "confirm_dialog.hpp"
#include "logger.hpp"
#include "menu.hpp"
#include "process_metrics.hpp"
#include "vox3d/render_raylib/chunk_mesh_preview.hpp"
#include "vox3d/render_raylib/free_fly_camera.hpp"
#include "ui_fonts.hpp"
#include "ui_labels.hpp"
#include "ui_layout.hpp"
#include "window_config.hpp"
#include "workspace.hpp"

#include <raylib.h>

#include <string>
#include <string_view>
#include <vector>

namespace vox3d {

/**
 * @brief High-level application screen state.
 */
enum class AppScreen {
    kMainMenu,
    kWorkspace,
    kSettingsPlaceholder,
};

/**
 * @brief Owns the application lifecycle and main loop state.
 */
class App {
public:
    /**
     * @brief Creates the application with the provided runtime configuration.
     *
     * @param config Runtime configuration copied into the application.
     * @param logger Logger used for application diagnostics. The logger must outlive the app.
     * @param labels Localized UI labels copied into the application.
     */
    App(AppConfig config, Logger& logger, UiLabels labels);

    /**
     * @brief Initializes window, resources, and initial UI state.
     *
     * @return True if initialization succeeded, false otherwise.
     */
    [[nodiscard]] bool Initialize();

    /**
     * @brief Runs the main application loop until shutdown is requested.
     *
     * @return Process exit code.
     */
    [[nodiscard]] int Run();

    /**
     * @brief Releases resources and closes the window.
     */
    void Shutdown();

private:
    void HandleInput(float dt);
    void HandleMainMenuInput();
    void HandleScreenInput(float dt);
    void HandlePlaceholderInput();
    void HandleWorkspaceInput(float dt);
    void HandleDialogInput();
    void Update(float dt);
    void Draw();
    void RebuildLayout();
    void ActivateSelectedMenuItem();
    void ActivatePlaceholderAction();
    void SelectPreviousWorkspaceTool();
    void SelectNextWorkspaceTool();
    void ToggleWorkspaceTool(WorkspaceTool tool);
    void ActivateWorkspacePanelItem(WorkspacePanelItem item);
    void ToggleTransitionOverlay(std::string_view reason);
    void SelectTileAtMouse(Vector2 mouse, std::string_view reason);
    void ScrollWorkspaceMenu(int delta_rows, std::string_view reason);
    void SetMeshBuildMode(ChunkMeshBuildMode mode, std::string_view reason);
    void SetColorMode(WorkspaceColorMode mode, std::string_view reason);
    void CycleColorMode(std::string_view reason);
    void SetVisibilityMode(WorkspaceVisibilityMode mode, std::string_view reason);
    void CycleVisibilityMode(std::string_view reason);
    void AdjustVisibilityRadius(int delta, std::string_view reason);
    void AdjustVisibilityFadeRing(int delta, std::string_view reason);
    void UpdateVisibilityStats();
    void SetChunkSize(int chunk_size, std::string_view reason);
    void RebuildChunkPipeline(int chunk_size, std::string_view reason);
    void UploadActiveChunkMesh(std::string_view reason);
    void RefreshMeshOptimizationStats();
    void RefreshChunkSizeComparison(
        int before_chunk_size,
        const ChunkGridInfo& before_grid_info,
        const MeshOptimizationStats& before_stats,
        bool had_before_stats);
    void SetActiveMeshCacheFromMode();
    void RunDirtyRebuildProbe(std::string_view reason);
    void FitPreviewCameraToViewport(std::string_view reason);
    void SetCurrentScreen(AppScreen screen, std::string_view reason);
    void RequestExitConfirmation(bool from_window_close = false);
    void CancelExitConfirmation();
    void AcceptExitConfirmation();
    void LoadUiFonts();
    void RefreshProcessMemoryInfo();
    void UnloadUiFonts();
    void UnloadPreviewResources();
    void LogSelectedItemChanged() const;
    [[nodiscard]] UiFontSet UiFonts() const;
    [[nodiscard]] AppScreen CurrentScreen() const;

    AppConfig config_;
    Logger& logger_;
    UiLabels labels_;
    WindowConfig window_config_;
    MainMenu main_menu_;
    ConfirmDialogState dialog_;
    AppScreen screen_ = AppScreen::kWorkspace;
    PlaceholderAction placeholder_selected_action_ = PlaceholderAction::kMainMenu;
    WorkspaceState workspace_;
    RaylibChunkMeshPreview chunk_mesh_preview_;
    FreeFlyCameraController preview_camera_;
    std::string hovered_item_ = "none";
    UiLayoutCache layout_cache_{};
    bool layout_dirty_ = true;
    bool running_ = false;
    bool window_initialized_ = false;
    bool title_font_loaded_ = false;
    bool text_font_loaded_ = false;
    bool window_close_request_armed_ = true;
    bool exit_requested_from_window_ = false;
    bool dialog_input_blocked_until_next_frame_ = false;
    bool suppress_window_close_request_this_frame_ = false;
    ProcessMemoryInfo process_memory_{};
    float process_memory_sample_timer_ = 0.0F;
    Font title_font_{};
    Font text_font_{};
};

/**
 * @brief Converts an application screen to a stable lowercase name.
 *
 * @param screen Application screen.
 * @return String representation.
 */
[[nodiscard]] std::string_view ToString(AppScreen screen);

}  // namespace vox3d
