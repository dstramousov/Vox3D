#include "vox3d/render/adaptive_tree_visibility.hpp"

#include <algorithm>
#include <cmath>

namespace vox3d {
namespace {

[[nodiscard]] float NonNegative(float value)
{
    return std::isfinite(value) ? std::max(0.0F, value) : 0.0F;
}

[[nodiscard]] double ValidFrameTime(double value)
{
    return std::isfinite(value) && value > 0.0 && value < 10000.0
        ? value
        : 0.0;
}

}  // namespace

void AdaptiveTreeVisibilityController::Configure(
    const AdaptiveTreeVisibilityConfig& config)
{
    config_ = config;
    config_.min_distance = NonNegative(config_.min_distance);
    config_.max_distance = std::max(config_.min_distance, NonNegative(config_.max_distance));
    config_.initial_distance = std::clamp(
        NonNegative(config_.initial_distance),
        config_.min_distance,
        config_.max_distance);
    config_.increase_step = NonNegative(config_.increase_step);
    config_.decrease_step = NonNegative(config_.decrease_step);
    config_.increase_delay_seconds = NonNegative(config_.increase_delay_seconds);
    config_.decrease_delay_seconds = NonNegative(config_.decrease_delay_seconds);
    config_.cooldown_seconds = NonNegative(config_.cooldown_seconds);
    config_.low_frame_ratio = std::clamp(config_.low_frame_ratio, 0.05F, 0.95F);
    config_.high_frame_ratio = std::clamp(
        config_.high_frame_ratio,
        config_.low_frame_ratio + 0.01F,
        1.50F);
    config_.target_fps = std::max(1, config_.target_fps);

    target_frame_ms_ = 1000.0 / static_cast<double>(config_.target_fps);
    status_ = AdaptiveTreeVisibilityStatus{};
    status_.enabled = config_.enabled;
    status_.current_distance = config_.initial_distance;
    status_.min_distance = config_.min_distance;
    status_.max_distance = config_.max_distance;
    status_.low_threshold_ms = target_frame_ms_ * config_.low_frame_ratio;
    status_.high_threshold_ms = target_frame_ms_ * config_.high_frame_ratio;
    status_.decision = config_.enabled
        ? AdaptiveTreeVisibilityDecision::kWaitingForSample
        : AdaptiveTreeVisibilityDecision::kDisabled;
    headroom_seconds_ = 0.0F;
    overload_seconds_ = 0.0F;
    cooldown_seconds_ = 0.0F;
}

bool AdaptiveTreeVisibilityController::Update(
    const AdaptiveTreeVisibilitySample& sample,
    float delta_seconds)
{
    if (!config_.enabled) {
        status_.decision = AdaptiveTreeVisibilityDecision::kDisabled;
        return false;
    }

    const float dt = std::clamp(NonNegative(delta_seconds), 0.0F, 1.0F);
    cooldown_seconds_ = std::max(0.0F, cooldown_seconds_ - dt);
    if (!sample.render_active) {
        headroom_seconds_ = 0.0F;
        overload_seconds_ = 0.0F;
        status_.sample_available = false;
        status_.decision = AdaptiveTreeVisibilityDecision::kWaitingForSample;
        return false;
    }

    const double gpu_ms = sample.gpu_sample_available
        ? ValidFrameTime(sample.gpu_frame_ms)
        : 0.0;
    const double cpu_ms = ValidFrameTime(sample.cpu_frame_ms);
    const double frame_ms = gpu_ms > 0.0 ? gpu_ms : cpu_ms;
    status_.using_gpu_sample = gpu_ms > 0.0;
    status_.sample_available = frame_ms > 0.0;
    status_.sampled_frame_ms = frame_ms;
    if (status_.using_gpu_sample) {
        status_.low_threshold_ms = target_frame_ms_ * config_.low_frame_ratio;
        status_.high_threshold_ms = target_frame_ms_ * config_.high_frame_ratio;
    } else {
        // CPU frame time usually includes VSync wait. Keep a neutral band around
        // the requested frame budget so a stable capped frame rate is not
        // misclassified as overload on systems without timer-query support.
        status_.low_threshold_ms = target_frame_ms_ * 0.85;
        status_.high_threshold_ms = target_frame_ms_ * 1.05;
    }
    if (frame_ms <= 0.0) {
        headroom_seconds_ = 0.0F;
        overload_seconds_ = 0.0F;
        status_.decision = AdaptiveTreeVisibilityDecision::kWaitingForSample;
        return false;
    }

    if (frame_ms > status_.high_threshold_ms) {
        overload_seconds_ += dt;
        headroom_seconds_ = 0.0F;
    } else if (frame_ms < status_.low_threshold_ms) {
        headroom_seconds_ += dt;
        overload_seconds_ = 0.0F;
    } else {
        headroom_seconds_ = 0.0F;
        overload_seconds_ = 0.0F;
    }

    status_.decision = AdaptiveTreeVisibilityDecision::kHolding;
    if (cooldown_seconds_ > 0.0F) {
        return false;
    }

    if (overload_seconds_ >= config_.decrease_delay_seconds
        && status_.current_distance > config_.min_distance) {
        const float next = std::max(
            config_.min_distance,
            status_.current_distance - config_.decrease_step);
        const bool changed = next < status_.current_distance;
        status_.current_distance = next;
        status_.decision = changed
            ? AdaptiveTreeVisibilityDecision::kDecreased
            : AdaptiveTreeVisibilityDecision::kHolding;
        overload_seconds_ = 0.0F;
        cooldown_seconds_ = config_.cooldown_seconds;
        return changed;
    }

    if (headroom_seconds_ >= config_.increase_delay_seconds
        && status_.current_distance < config_.max_distance) {
        const float next = std::min(
            config_.max_distance,
            status_.current_distance + config_.increase_step);
        const bool changed = next > status_.current_distance;
        status_.current_distance = next;
        status_.decision = changed
            ? AdaptiveTreeVisibilityDecision::kIncreased
            : AdaptiveTreeVisibilityDecision::kHolding;
        headroom_seconds_ = 0.0F;
        cooldown_seconds_ = config_.cooldown_seconds;
        return changed;
    }

    return false;
}

const AdaptiveTreeVisibilityStatus& AdaptiveTreeVisibilityController::Status() const
{
    return status_;
}

const char* ToString(AdaptiveTreeVisibilityDecision decision)
{
    switch (decision) {
        case AdaptiveTreeVisibilityDecision::kDisabled:
            return "disabled";
        case AdaptiveTreeVisibilityDecision::kWaitingForSample:
            return "waiting";
        case AdaptiveTreeVisibilityDecision::kHolding:
            return "holding";
        case AdaptiveTreeVisibilityDecision::kIncreased:
            return "increased";
        case AdaptiveTreeVisibilityDecision::kDecreased:
            return "decreased";
    }
    return "unknown";
}

}  // namespace vox3d
