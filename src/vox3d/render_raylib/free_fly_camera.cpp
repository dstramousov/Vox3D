#include "vox3d/render_raylib/free_fly_camera.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace vox3d {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kHalfPi = kPi * 0.5F;

[[nodiscard]] float Length(Vector3 value)
{
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

[[nodiscard]] Vector3 Add(Vector3 lhs, Vector3 rhs)
{
    return Vector3{lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

[[nodiscard]] Vector3 Subtract(Vector3 lhs, Vector3 rhs)
{
    return Vector3{lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

[[nodiscard]] Vector3 Scale(Vector3 value, float scale)
{
    return Vector3{value.x * scale, value.y * scale, value.z * scale};
}

[[nodiscard]] Vector3 Normalize(Vector3 value)
{
    const float length = Length(value);
    if (length <= 0.000001F) {
        return Vector3{0.0F, 0.0F, 0.0F};
    }
    return Scale(value, 1.0F / length);
}

[[nodiscard]] Vector3 Lerp(Vector3 from, Vector3 to, float amount)
{
    return Vector3{
        from.x + (to.x - from.x) * amount,
        from.y + (to.y - from.y) * amount,
        from.z + (to.z - from.z) * amount,
    };
}

[[nodiscard]] float LerpFactor(float rate, float dt)
{
    if (rate <= 0.0F || dt <= 0.0F) {
        return 0.0F;
    }
    return std::clamp(1.0F - std::exp(-rate * dt), 0.0F, 1.0F);
}

[[nodiscard]] Vector3 ForwardFromAngles(float yaw, float pitch)
{
    const float cp = std::cos(pitch);
    return Normalize(Vector3{
        cp * std::sin(yaw),
        std::sin(pitch),
        cp * std::cos(yaw),
    });
}

[[nodiscard]] Vector3 RightFromYaw(float yaw)
{
    return Normalize(Vector3{std::cos(yaw), 0.0F, -std::sin(yaw)});
}

[[nodiscard]] Vector3 BuildInitialPosition(const ChunkMeshBuildResult& build_result)
{
    const float map_width = static_cast<float>(std::max(1, build_result.info.map_width));
    const float map_height = static_cast<float>(std::max(1, build_result.info.map_height));
    const float span = std::max(map_width, map_height);
    const float max_level = build_result.info.levels.has_value() ? static_cast<float>(build_result.info.levels->max) : 12.0F;
    return Vector3{span * 0.56F, span * 0.72F + std::max(18.0F, max_level), span * 0.70F};
}

[[nodiscard]] Vector3 BuildInitialTarget(const ChunkMeshBuildResult& build_result)
{
    const float min_level = build_result.info.levels.has_value() ? static_cast<float>(build_result.info.levels->min) : 0.0F;
    const float max_level = build_result.info.levels.has_value() ? static_cast<float>(build_result.info.levels->max) : 12.0F;
    return Vector3{0.0F, (min_level + max_level) * 0.40F, 0.0F};
}

[[nodiscard]] float CurrentMoveSpeed(const FreeFlyCameraConfig& config)
{
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
        return config.fast_speed;
    }
    if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
        return config.slow_speed;
    }
    return config.normal_speed;
}

}  // namespace

FreeFlyCameraController::FreeFlyCameraController(FreeFlyCameraConfig config)
    : config_(config)
{
}

void FreeFlyCameraController::FitToMap(const ChunkMeshBuildResult& build_result)
{
    SetPose(BuildInitialPosition(build_result), BuildInitialTarget(build_result));
    reset_position_ = camera_.position;
    reset_target_ = camera_.target;
    reset_yaw_ = yaw_;
    reset_pitch_ = pitch_;
    velocity_ = Vector3{};
}

void FreeFlyCameraController::ResetView()
{
    if (!initialized_) {
        return;
    }
    camera_.position = reset_position_;
    camera_.target = reset_target_;
    camera_.up = Vector3{0.0F, 1.0F, 0.0F};
    camera_.fovy = 45.0F;
    camera_.projection = CAMERA_PERSPECTIVE;
    yaw_ = reset_yaw_;
    pitch_ = reset_pitch_;
    velocity_ = Vector3{};
    current_speed_ = 0.0F;
    ApplyOrientation();
}

void FreeFlyCameraController::Update(float dt, Rectangle viewport, bool enabled)
{
    if (!initialized_ || dt <= 0.0F || !enabled || !IsWindowFocused()) {
        ReleaseMouse();
        velocity_ = Lerp(velocity_, Vector3{}, LerpFactor(config_.damping, dt));
        mouse_look_active_ = false;
        return;
    }

    const Vector2 mouse = GetMousePosition();
    const bool mouse_inside_viewport = CheckCollisionPointRec(mouse, viewport);
    const bool wants_capture = IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && (cursor_captured_ || mouse_inside_viewport);
    if (wants_capture && !cursor_captured_) {
        DisableCursor();
        cursor_captured_ = true;
    } else if (!wants_capture && cursor_captured_) {
        ReleaseMouse();
    }

    mouse_look_active_ = cursor_captured_;
    if (mouse_look_active_) {
        const Vector2 delta = GetMouseDelta();
        yaw_ -= delta.x * config_.mouse_sensitivity;
        pitch_ -= delta.y * config_.mouse_sensitivity;
        pitch_ = std::clamp(pitch_, std::max(config_.min_pitch_radians, -kHalfPi + 0.001F), std::min(config_.max_pitch_radians, kHalfPi - 0.001F));
    }

    const Vector3 forward = ForwardFromAngles(yaw_, pitch_);
    const Vector3 right = RightFromYaw(yaw_);
    constexpr Vector3 kWorldUp{0.0F, 1.0F, 0.0F};

    Vector3 direction{};
    if (IsKeyDown(KEY_W)) {
        direction = Add(direction, forward);
    }
    if (IsKeyDown(KEY_S)) {
        direction = Subtract(direction, forward);
    }
    if (IsKeyDown(KEY_D)) {
        direction = Add(direction, right);
    }
    if (IsKeyDown(KEY_A)) {
        direction = Subtract(direction, right);
    }
    if (IsKeyDown(KEY_E)) {
        direction = Add(direction, kWorldUp);
    }
    if (IsKeyDown(KEY_Q)) {
        direction = Subtract(direction, kWorldUp);
    }

    const bool moving = Length(direction) > 0.000001F;
    current_speed_ = moving ? CurrentMoveSpeed(config_) : 0.0F;
    const Vector3 target_velocity = moving ? Scale(Normalize(direction), current_speed_) : Vector3{};
    const float response = moving ? config_.acceleration : config_.damping;
    velocity_ = Lerp(velocity_, target_velocity, LerpFactor(response, dt));
    camera_.position = Add(camera_.position, Scale(velocity_, dt));
    ApplyOrientation();
}

void FreeFlyCameraController::ReleaseMouse()
{
    if (cursor_captured_) {
        EnableCursor();
        cursor_captured_ = false;
    }
    mouse_look_active_ = false;
}

bool FreeFlyCameraController::IsInitialized() const
{
    return initialized_;
}

bool FreeFlyCameraController::IsCursorCaptured() const
{
    return cursor_captured_;
}

const Camera3D& FreeFlyCameraController::Camera() const
{
    return camera_;
}

FreeFlyCameraStatus FreeFlyCameraController::Status() const
{
    return FreeFlyCameraStatus{
        initialized_,
        cursor_captured_,
        mouse_look_active_,
        current_speed_,
    };
}

void FreeFlyCameraController::ApplyOrientation()
{
    camera_.target = Add(camera_.position, ForwardFromAngles(yaw_, pitch_));
    camera_.up = Vector3{0.0F, 1.0F, 0.0F};
    camera_.fovy = 45.0F;
    camera_.projection = CAMERA_PERSPECTIVE;
}

void FreeFlyCameraController::SetPose(Vector3 position, Vector3 target)
{
    const Vector3 forward = Normalize(Subtract(target, position));
    yaw_ = std::atan2(forward.x, forward.z);
    pitch_ = std::asin(std::clamp(forward.y, -1.0F, 1.0F));
    camera_.position = position;
    camera_.target = target;
    camera_.up = Vector3{0.0F, 1.0F, 0.0F};
    camera_.fovy = 45.0F;
    camera_.projection = CAMERA_PERSPECTIVE;
    initialized_ = true;
    current_speed_ = 0.0F;
    ApplyOrientation();
}

std::string ToLogString(const FreeFlyCameraStatus& status)
{
    std::ostringstream out;
    out << "status=" << (status.initialized ? "ready" : "unavailable");
    out << " captured=" << (status.cursor_captured ? "yes" : "no");
    out << " look=" << (status.mouse_look_active ? "yes" : "no");
    out << " speed=" << status.speed;
    return out.str();
}

}  // namespace vox3d
