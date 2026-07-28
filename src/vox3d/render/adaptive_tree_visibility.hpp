#pragma once

#include <cstdint>

namespace vox3d {

/**
 * @brief Configuration for adaptive detailed-tree visibility distance.
 */
struct AdaptiveTreeVisibilityConfig {
    bool enabled = true;
    float initial_distance = 320.0F;
    float min_distance = 160.0F;
    float max_distance = 2048.0F;
    float increase_step = 32.0F;
    float decrease_step = 64.0F;
    float increase_delay_seconds = 5.0F;
    float decrease_delay_seconds = 1.0F;
    float cooldown_seconds = 2.0F;
    float low_frame_ratio = 0.60F;
    float high_frame_ratio = 0.82F;
    int target_fps = 60;
};

/**
 * @brief One performance sample consumed by the adaptive tree controller.
 */
struct AdaptiveTreeVisibilitySample {
    bool render_active = false;
    bool gpu_sample_available = false;
    double gpu_frame_ms = 0.0;
    double cpu_frame_ms = 0.0;
};

/**
 * @brief Last action selected by the adaptive tree controller.
 */
enum class AdaptiveTreeVisibilityDecision : std::uint8_t {
    kDisabled,
    kWaitingForSample,
    kHolding,
    kIncreased,
    kDecreased,
};

/**
 * @brief Stable runtime state exposed to diagnostics and the HUD.
 */
struct AdaptiveTreeVisibilityStatus {
    bool enabled = false;
    bool using_gpu_sample = false;
    bool sample_available = false;
    float current_distance = 0.0F;
    float min_distance = 0.0F;
    float max_distance = 0.0F;
    double sampled_frame_ms = 0.0;
    double low_threshold_ms = 0.0;
    double high_threshold_ms = 0.0;
    AdaptiveTreeVisibilityDecision decision =
        AdaptiveTreeVisibilityDecision::kDisabled;
};

/**
 * @brief Adjusts detailed-tree culling distance from smoothed frame timings.
 *
 * Quality increases slowly after sustained headroom and decreases faster after
 * sustained overload. The controller never changes model identity or palette.
 */
class AdaptiveTreeVisibilityController {
public:
    /**
     * @brief Applies configuration and resets all controller timers.
     *
     * @param config New adaptive visibility configuration.
     */
    void Configure(const AdaptiveTreeVisibilityConfig& config);

    /**
     * @brief Updates the current detailed-tree visibility distance.
     *
     * @param sample Latest stable CPU/GPU frame timing sample.
     * @param delta_seconds Elapsed application time since the previous update.
     * @return True when the visibility distance changed.
     */
    [[nodiscard]] bool Update(
        const AdaptiveTreeVisibilitySample& sample,
        float delta_seconds);

    /**
     * @brief Returns the current stable controller state.
     *
     * @return Runtime status suitable for diagnostics and UI display.
     */
    [[nodiscard]] const AdaptiveTreeVisibilityStatus& Status() const;

private:
    AdaptiveTreeVisibilityConfig config_{};
    AdaptiveTreeVisibilityStatus status_{};
    float headroom_seconds_ = 0.0F;
    float overload_seconds_ = 0.0F;
    float cooldown_seconds_ = 0.0F;
    double target_frame_ms_ = 0.0;
};

/**
 * @brief Converts an adaptive-tree decision to a stable lowercase label.
 *
 * @param decision Decision value.
 * @return Stable non-owning label.
 */
[[nodiscard]] const char* ToString(AdaptiveTreeVisibilityDecision decision);

}  // namespace vox3d
