#pragma once

#include "vox3d/mesh/mesh_data.hpp"

#include <raylib.h>

#include <string>

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
};

/**
 * @brief Raylib-backed free-fly camera controller for the editor 3D preview.
 *
 * The controller owns camera position, orientation, and velocity smoothing. It
 * reads raylib keyboard/mouse state directly and never mutates core map, voxel,
 * chunk, or mesh data.
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
     * @param config Movement and mouse-look configuration.
     */
    explicit FreeFlyCameraController(FreeFlyCameraConfig config);

    /**
     * @brief Frames the generated mesh map and stores that frame as the reset pose.
     *
     * @param build_result Mesh build result used for map dimensions and level range.
     */
    void FitToMap(const ChunkMeshBuildResult& build_result);

    /**
     * @brief Restores the last stored reset pose and clears camera velocity.
     */
    void ResetView();

    /**
     * @brief Updates camera velocity, position, and mouse look from raylib input.
     *
     * Mouse look is active while the right mouse button is held over the 3D viewport.
     * Movement uses WASD for planar/forward movement and Q/E for vertical movement.
     *
     * @param dt Frame delta time in seconds.
     * @param viewport Screen-space rectangle used for mouse-look activation.
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
    void SetPose(Vector3 position, Vector3 target);

    FreeFlyCameraConfig config_{};
    Camera3D camera_{};
    Vector3 velocity_{};
    Vector3 reset_position_{};
    Vector3 reset_target_{};
    float yaw_ = 0.0F;
    float pitch_ = 0.0F;
    float reset_yaw_ = 0.0F;
    float reset_pitch_ = 0.0F;
    float current_speed_ = 0.0F;
    bool initialized_ = false;
    bool cursor_captured_ = false;
    bool mouse_look_active_ = false;
};

/**
 * @brief Builds a compact stable log string for free-fly camera diagnostics.
 *
 * @param status Camera runtime status.
 * @return Compact human-readable summary.
 */
[[nodiscard]] std::string ToLogString(const FreeFlyCameraStatus& status);

}  // namespace vox3d
