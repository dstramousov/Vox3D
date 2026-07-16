#include "vox3d/render_raylib/free_fly_camera.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace vox3d {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kHalfPi = kPi * 0.5F;
constexpr float kDegreesPerRadian = 180.0F / kPi;
constexpr float kDefaultFitYaw = -kPi * 0.25F;
constexpr float kDefaultFitPitch = -kPi / 3.0F;
constexpr float kOverviewYaw = -0.78F;
constexpr float kOverviewPitch = -0.29F;
constexpr float kMinimumFitDistance = 24.0F;

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


[[nodiscard]] float SmoothStep(float value)
{
    const float t = std::clamp(value, 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
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
    return Normalize(Vector3{-std::cos(yaw), 0.0F, std::sin(yaw)});
}

[[nodiscard]] float ToRadians(float degrees)
{
    return degrees * kPi / 180.0F;
}

[[nodiscard]] float SafeAspect(Rectangle viewport)
{
    if (viewport.width <= 1.0F || viewport.height <= 1.0F) {
        return 1.0F;
    }
    return std::clamp(viewport.width / viewport.height, 0.20F, 5.0F);
}

[[nodiscard]] Vector3 BuildMapCenter(const ChunkMeshBuildResult& build_result)
{
    const float min_level = build_result.info.levels.has_value()
        ? static_cast<float>(build_result.info.levels->min)
        : 0.0F;
    const float max_level = build_result.info.levels.has_value()
        ? static_cast<float>(build_result.info.levels->max + 1)
        : 12.0F;
    return Vector3{0.0F, (min_level + max_level) * 0.5F, 0.0F};
}

[[nodiscard]] float BuildFitDistance(
    const ChunkMeshBuildResult& build_result,
    Rectangle viewport,
    const FreeFlyCameraConfig& config)
{
    const float map_width = static_cast<float>(std::max(1, build_result.info.map_width));
    const float map_height = static_cast<float>(std::max(1, build_result.info.map_height));
    const float min_level = build_result.info.levels.has_value()
        ? static_cast<float>(build_result.info.levels->min)
        : 0.0F;
    const float max_level = build_result.info.levels.has_value()
        ? static_cast<float>(build_result.info.levels->max + 1)
        : 12.0F;
    const float half_width = map_width * 0.5F;
    const float half_depth = map_height * 0.5F;
    const float half_height = std::max(4.0F, (max_level - min_level) * 0.5F);
    const float radius = std::sqrt(
        half_width * half_width + half_depth * half_depth + half_height * half_height);

    const float vertical_fov = ToRadians(std::clamp(config.fovy_degrees, 15.0F, 120.0F));
    const float aspect = SafeAspect(viewport);
    const float horizontal_fov = 2.0F * std::atan(std::tan(vertical_fov * 0.5F) * aspect);
    const float limiting_fov = std::clamp(std::min(vertical_fov, horizontal_fov), 0.10F, kPi - 0.10F);
    const float fit_distance = radius / std::max(0.10F, std::sin(limiting_fov * 0.5F));
    return std::max(kMinimumFitDistance, fit_distance * std::clamp(config.fit_padding, 0.50F, 2.00F));
}

[[nodiscard]] Vector3 BuildOverviewTarget(const ChunkMeshBuildResult& build_result)
{
    Vector3 target = BuildMapCenter(build_result);
    const float map_width = static_cast<float>(std::max(1, build_result.info.map_width));
    const float map_height = static_cast<float>(std::max(1, build_result.info.map_height));
    target.x += map_width * 0.02F;
    target.z += map_height * 0.02F;
    return target;
}

[[nodiscard]] float BuildOverviewDistance(const ChunkMeshBuildResult& build_result)
{
    const float map_width = static_cast<float>(std::max(1, build_result.info.map_width));
    const float map_height = static_cast<float>(std::max(1, build_result.info.map_height));
    return std::max(kMinimumFitDistance, std::max(map_width, map_height) * 0.78F);
}

[[nodiscard]] Vector3 BuildOverviewPosition(const ChunkMeshBuildResult& build_result)
{
    const Vector3 target = BuildOverviewTarget(build_result);
    const Vector3 forward = ForwardFromAngles(kOverviewYaw, kOverviewPitch);
    return Subtract(target, Scale(forward, BuildOverviewDistance(build_result)));
}

[[nodiscard]] Vector3 BuildFitPosition(
    const ChunkMeshBuildResult& build_result,
    Rectangle /*viewport*/,
    const FreeFlyCameraConfig& /*config*/)
{
    return BuildOverviewPosition(build_result);
}

[[nodiscard]] Vector3 BuildFlyInStartPosition(
    const ChunkMeshBuildResult& build_result,
    Rectangle viewport,
    const FreeFlyCameraConfig& config)
{
    const Vector3 target = BuildMapCenter(build_result);
    const Vector3 forward = ForwardFromAngles(kDefaultFitYaw, kDefaultFitPitch);
    return Subtract(target, Scale(forward, BuildFitDistance(build_result, viewport, config) * 1.10F));
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

[[nodiscard]] bool IsAnyViewportCaptureButtonPressed()
{
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
        || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)
        || IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE);
}

[[nodiscard]] std::string FormatVector(Vector3 value)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(1);
    out << value.x << ',' << value.y << ',' << value.z;
    return out.str();
}

}  // namespace

FreeFlyCameraController::FreeFlyCameraController(FreeFlyCameraConfig config)
    : config_(config)
{
}

void FreeFlyCameraController::FitToMap(const ChunkMeshBuildResult& build_result, Rectangle viewport)
{
    const Vector3 target = BuildOverviewTarget(build_result);
    const Vector3 position = BuildFitPosition(build_result, viewport, config_);
    SetPose(position, target);
    StoreResetPose(position, target);
    fly_in_active_ = false;
    velocity_ = Vector3{};
    wheel_velocity_ = 0.0F;
}

void FreeFlyCameraController::FitToMap(const ChunkMeshBuildResult& build_result)
{
    FitToMap(build_result, Rectangle{0.0F, 0.0F, 1.0F, 1.0F});
}

void FreeFlyCameraController::StartFlyInToMap(const ChunkMeshBuildResult& build_result, Rectangle viewport)
{
    fly_in_start_target_ = BuildMapCenter(build_result);
    fly_in_start_position_ = BuildFlyInStartPosition(build_result, viewport, config_);
    fly_in_end_target_ = BuildOverviewTarget(build_result);
    fly_in_end_position_ = BuildOverviewPosition(build_result);
    fly_in_elapsed_ = 0.0F;
    fly_in_hold_elapsed_ = 0.0F;
    fly_in_duration_ = 2.35F;
    fly_in_active_ = true;
    velocity_ = Vector3{};
    wheel_velocity_ = 0.0F;
    StoreResetPose(fly_in_end_position_, fly_in_end_target_);
    SetPose(fly_in_start_position_, fly_in_start_target_);
}

void FreeFlyCameraController::ResetView()
{
    if (!initialized_) {
        return;
    }
    camera_.position = reset_position_;
    camera_.target = reset_target_;
    camera_.up = Vector3{0.0F, 1.0F, 0.0F};
    camera_.fovy = config_.fovy_degrees;
    camera_.projection = CAMERA_PERSPECTIVE;
    yaw_ = reset_yaw_;
    pitch_ = reset_pitch_;
    velocity_ = Vector3{};
    wheel_velocity_ = 0.0F;
    current_speed_ = 0.0F;
    ApplyOrientation();
}

void FreeFlyCameraController::Update(float dt, Rectangle viewport, bool enabled)
{
    if (fly_in_active_ && enabled && dt > 0.0F) {
        ReleaseMouse();
        if (IsAnyViewportCaptureButtonPressed() || std::abs(GetMouseWheelMove()) > 0.0001F
            || IsKeyPressed(KEY_W) || IsKeyPressed(KEY_A) || IsKeyPressed(KEY_S)
            || IsKeyPressed(KEY_D) || IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_E)) {
            fly_in_active_ = false;
            SetPose(fly_in_end_position_, fly_in_end_target_);
            return;
        }

        const float frame_dt = std::clamp(dt, 0.0F, 1.0F / 30.0F);
        if (fly_in_hold_elapsed_ < fly_in_hold_duration_) {
            fly_in_hold_elapsed_ += frame_dt;
            SetPose(fly_in_start_position_, fly_in_start_target_);
            return;
        }

        fly_in_elapsed_ += frame_dt;
        const float t = SmoothStep(fly_in_elapsed_ / std::max(0.001F, fly_in_duration_));
        SetPose(
            Lerp(fly_in_start_position_, fly_in_end_position_, t),
            Lerp(fly_in_start_target_, fly_in_end_target_, t));
        if (fly_in_elapsed_ >= fly_in_duration_) {
            fly_in_active_ = false;
            SetPose(fly_in_end_position_, fly_in_end_target_);
        }
        return;
    }

    if (!initialized_ || dt <= 0.0F || !enabled || !IsWindowFocused()) {
        ReleaseMouse();
        velocity_ = Lerp(velocity_, Vector3{}, LerpFactor(config_.damping, dt));
        wheel_velocity_ += (0.0F - wheel_velocity_) * LerpFactor(config_.damping, dt);
        mouse_look_active_ = false;
        return;
    }

    const Vector2 mouse = GetMousePosition();
    const bool mouse_inside_viewport = CheckCollisionPointRec(mouse, viewport);
    if (!cursor_captured_ && mouse_inside_viewport && IsAnyViewportCaptureButtonPressed()) {
        CaptureMouse();
    }

    mouse_look_active_ = cursor_captured_;
    if (mouse_look_active_) {
        const Vector2 delta = GetMouseDelta();
        if (discard_next_mouse_delta_) {
            discard_next_mouse_delta_ = false;
        } else {
            yaw_ -= delta.x * config_.mouse_sensitivity;
            pitch_ -= delta.y * config_.mouse_sensitivity;
            pitch_ = std::clamp(
                pitch_,
                std::max(config_.min_pitch_radians, -kHalfPi + 0.001F),
                std::min(config_.max_pitch_radians, kHalfPi - 0.001F));
        }
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

    const float wheel = GetMouseWheelMove();
    if ((cursor_captured_ || mouse_inside_viewport) && std::abs(wheel) > 0.0001F) {
        const float wheel_limit = std::max(config_.fast_speed * 2.0F, config_.wheel_dolly_speed);
        wheel_velocity_ = std::clamp(
            wheel_velocity_ + wheel * config_.wheel_dolly_speed,
            -wheel_limit,
            wheel_limit);
    }

    const bool moving = Length(direction) > 0.000001F;
    current_speed_ = moving ? CurrentMoveSpeed(config_) : 0.0F;
    const Vector3 target_velocity = moving ? Scale(Normalize(direction), current_speed_) : Vector3{};
    const float response = moving ? config_.acceleration : config_.damping;
    velocity_ = Lerp(velocity_, target_velocity, LerpFactor(response, dt));
    wheel_velocity_ += (0.0F - wheel_velocity_) * LerpFactor(config_.damping, dt);

    camera_.position = Add(camera_.position, Scale(velocity_, dt));
    camera_.position = Add(camera_.position, Scale(forward, wheel_velocity_ * dt));
    ApplyOrientation();
}

void FreeFlyCameraController::ReleaseMouse()
{
    if (cursor_captured_) {
        EnableCursor();
        cursor_captured_ = false;
    }
    mouse_look_active_ = false;
    discard_next_mouse_delta_ = false;
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
        yaw_ * kDegreesPerRadian,
        pitch_ * kDegreesPerRadian,
        camera_.position,
        camera_.target,
    };
}

void FreeFlyCameraController::ApplyOrientation()
{
    camera_.target = Add(camera_.position, ForwardFromAngles(yaw_, pitch_));
    camera_.up = Vector3{0.0F, 1.0F, 0.0F};
    camera_.fovy = config_.fovy_degrees;
    camera_.projection = CAMERA_PERSPECTIVE;
}

void FreeFlyCameraController::CaptureMouse()
{
    if (cursor_captured_) {
        return;
    }
    DisableCursor();
    cursor_captured_ = true;
    mouse_look_active_ = true;
    discard_next_mouse_delta_ = true;
}

void FreeFlyCameraController::StoreResetPose(Vector3 position, Vector3 target)
{
    const Vector3 forward = Normalize(Subtract(target, position));
    reset_position_ = position;
    reset_target_ = target;
    reset_yaw_ = std::atan2(forward.x, forward.z);
    reset_pitch_ = std::asin(std::clamp(forward.y, -1.0F, 1.0F));
}

void FreeFlyCameraController::SetPose(Vector3 position, Vector3 target)
{
    const Vector3 forward = Normalize(Subtract(target, position));
    yaw_ = std::atan2(forward.x, forward.z);
    pitch_ = std::asin(std::clamp(forward.y, -1.0F, 1.0F));
    camera_.position = position;
    camera_.target = target;
    camera_.up = Vector3{0.0F, 1.0F, 0.0F};
    camera_.fovy = config_.fovy_degrees;
    camera_.projection = CAMERA_PERSPECTIVE;
    initialized_ = true;
    current_speed_ = 0.0F;
    ApplyOrientation();
}

std::string ToLogString(const FreeFlyCameraStatus& status)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(1);
    out << "status=" << (status.initialized ? "ready" : "unavailable");
    out << " captured=" << (status.cursor_captured ? "yes" : "no");
    out << " look=" << (status.mouse_look_active ? "yes" : "no");
    out << " speed=" << status.speed;
    out << " yaw=" << status.yaw_degrees;
    out << " pitch=" << status.pitch_degrees;
    out << " pos=" << FormatVector(status.position);
    out << " target=" << FormatVector(status.target);
    return out.str();
}

}  // namespace vox3d
