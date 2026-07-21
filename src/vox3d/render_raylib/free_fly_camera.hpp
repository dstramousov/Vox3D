#pragma once

#include "vox3d/mesh/mesh_data.hpp"

#include <raylib.h>

#include <string>
#include <string_view>

namespace vox3d {

/**
 * @brief Tuning values for the free-fly debug camera controller.
 */
struct FreeFlyCameraConfig {
    float normal_speed = 36.0F;
    float fast_speed = 120.0F;
    float slow_speed = 9.0F;
    float acceleration = 14.0F;
    float damping = 18.0F;
    float mouse_sensitivity = 0.0030F;
    float wheel_dolly_speed = 140.0F;
    float fit_padding = 0.70F;
    float fovy_degrees = 45.0F;
    float min_pitch_radians = -1.553343F;
    float max_pitch_radians = 1.553343F;
};

/**
 * @brief Runtime status for the free-fly debug camera.
 */
struct FreeFlyCameraStatus {
    bool initialized = false;
    bool cursor_captured = false;
    bool mouse_look_active = false;
    float speed = 0.0F;
    float yaw_degrees = 0.0F;
    float pitch_degrees = 0.0F;
    Vector3 position{};
    Vector3 target{};
};

/**
 * @brief Raylib-backed free-fly camera controller for the editor 3D preview.
 *
 * The controller owns camera position, orientation, mouse capture, and velocity
 * smoothing. It reads raylib keyboard/mouse state directly and never mutates
 * core map, voxel, chunk, or mesh data.
 */
class FreeFlyCameraController {
public:
    /**
     * @brief Creates a camera controller with default movement tuning.
     */
    FreeFlyCameraController() = default;

    /**
     * @brief Creates a camera controller with explicit movement tuning.
     *
     * @param config Movement, fit-view, and mouse-look configuration.
     */
    explicit FreeFlyCameraController(FreeFlyCameraConfig config);

    /**
     * @brief Frames the generated mesh map and stores that frame as the reset pose.
     *
     * The camera distance is computed from the map bounds and the actual viewport
     * aspect ratio so the map starts with a tight centered frame.
     *
     * @param build_result Mesh build result used for map dimensions and level range.
     * @param viewport Screen-space viewport used to resolve aspect ratio.
     */
    void FitToMap(const ChunkMeshBuildResult& build_result, Rectangle viewport);

    /**
     * @brief Frames the generated mesh map using a square fallback viewport.
     *
     * @param build_result Mesh build result used for map dimensions and level range.
     */
    void FitToMap(const ChunkMeshBuildResult& build_result);

    /**
     * @brief Starts a one-shot cinematic fly-in from an overhead map frame.
     *
     * The final pose is stored as the reset pose. User driven camera updates are
     * ignored while the fly-in is active. The animation is intended for initial
     * application startup only.
     *
     * @param build_result Mesh build result used for map dimensions and level range.
     * @param viewport Screen-space viewport used to resolve aspect ratio.
     */
    void StartFlyInToMap(const ChunkMeshBuildResult& build_result, Rectangle viewport);

    /**
     * @brief Places the camera at a diagonal corner view of an already-built startup tile window.
     *
     * The pose is stored as the reset pose and disables any active startup fly-in.
     * Tile coordinates use the same 0-based map-space convention as mesh builders.
     *
     * @param build_result Mesh build result used for map dimensions and level range.
     * @param window_left Left tile coordinate of the startup window.
     * @param window_top Top tile coordinate of the startup window.
     * @param window_width Startup window width in tiles.
     * @param window_height Startup window height in tiles.
     * @param corner Corner name: nw, ne, sw, or se. Unknown values fall back to se.
     */
    void SetTileWindowCornerView(
        const ChunkMeshBuildResult& build_result,
        int window_left,
        int window_top,
        int window_width,
        int window_height,
        std::string_view corner);

    /**
     * @brief Restores the last stored reset pose and clears camera velocity.
     */
    void ResetView();

    /**
     * @brief Updates camera velocity, position, and mouse look from raylib input.
     *
     * Clicking inside the 3D viewport captures the mouse. While captured, mouse
     * movement controls view direction until the application releases the cursor.
     * Movement uses WASD for forward/strafe movement and Q/E for vertical movement.
     *
     * @param dt Frame delta time in seconds.
     * @param viewport Screen-space rectangle used for mouse capture activation.
     * @param enabled True when the 3D preview is currently active.
     */
    void Update(float dt, Rectangle viewport, bool enabled);

    /**
     * @brief Releases mouse capture if the controller currently owns it.
     */
    void ReleaseMouse();

    /**
     * @brief Returns true when the camera has a valid pose.
     *
     * @return True if FitToMap or ResetView initialized the camera.
     */
    [[nodiscard]] bool IsInitialized() const;

    /**
     * @brief Returns true when the mouse cursor is currently captured by raylib.
     *
     * @return True if mouse capture is active.
     */
    [[nodiscard]] bool IsCursorCaptured() const;

    /**
     * @brief Returns the current raylib camera.
     *
     * @return Current Camera3D state.
     */
    [[nodiscard]] const Camera3D& Camera() const;

    /**
     * @brief Returns the latest runtime camera status.
     *
     * @return Runtime camera status.
     */
    [[nodiscard]] FreeFlyCameraStatus Status() const;

private:
    void ApplyOrientation();
    void CaptureMouse();
    void SetPose(Vector3 position, Vector3 target);
    void StoreResetPose(Vector3 position, Vector3 target);

    FreeFlyCameraConfig config_{};
    Camera3D camera_{};
    Vector3 velocity_{};
    Vector3 reset_position_{};
    Vector3 reset_target_{};
    Vector3 fly_in_start_position_{};
    Vector3 fly_in_start_target_{};
    Vector3 fly_in_end_position_{};
    Vector3 fly_in_end_target_{};
    float yaw_ = 0.0F;
    float pitch_ = 0.0F;
    float reset_yaw_ = 0.0F;
    float reset_pitch_ = 0.0F;
    float current_speed_ = 0.0F;
    float wheel_velocity_ = 0.0F;
    float fly_in_elapsed_ = 0.0F;
    float fly_in_hold_elapsed_ = 0.0F;
    float fly_in_hold_duration_ = 0.45F;
    float fly_in_duration_ = 2.35F;
    bool fly_in_active_ = false;
    bool initialized_ = false;
    bool cursor_captured_ = false;
    bool mouse_look_active_ = false;
    bool discard_next_mouse_delta_ = false;
};

/**
 * @brief Builds a compact stable log string for free-fly camera diagnostics.
 *
 * @param status Camera runtime status.
 * @return Compact human-readable summary.
 */
[[nodiscard]] std::string ToLogString(const FreeFlyCameraStatus& status);

}  // namespace vox3d
