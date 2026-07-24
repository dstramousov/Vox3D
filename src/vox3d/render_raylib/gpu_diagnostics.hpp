#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace vox3d {

/**
 * @brief Driver-reported video-memory information when a supported extension exists.
 */
struct GpuDriverMemoryStats {
    std::string source = "unavailable";
    bool total_available = false;
    bool free_available = false;
    bool used_available = false;
    bool eviction_available = false;
    std::uint64_t total_bytes = 0;
    std::uint64_t free_bytes = 0;
    std::uint64_t used_bytes = 0;
    std::uint64_t eviction_count = 0;
    std::uint64_t evicted_bytes = 0;
};

/**
 * @brief Stable GPU device, frame-time, and driver-memory diagnostics snapshot.
 */
struct GpuDiagnosticsSnapshot {
    std::string vendor = "unavailable";
    std::string renderer = "unavailable";
    std::string opengl_version = "unavailable";
    std::string glsl_version = "unavailable";
    bool initialized = false;
    bool timer_query_supported = false;
    bool render_active = false;
    bool gpu_sample_available = false;
    double cpu_frame_average_ms = 0.0;
    double gpu_frame_last_ms = 0.0;
    double gpu_frame_average_ms = 0.0;
    double gpu_frame_peak_ms = 0.0;
    GpuDriverMemoryStats driver_memory;
};

/**
 * @brief Non-blocking OpenGL diagnostics used by the 3D preview.
 *
 * GPU frame time is measured with a small ring of timer queries. Results are
 * polled on later frames and never force a synchronous wait for the GPU.
 */
class GpuDiagnostics {
public:
    GpuDiagnostics() = default;
    ~GpuDiagnostics();

    GpuDiagnostics(const GpuDiagnostics&) = delete;
    GpuDiagnostics& operator=(const GpuDiagnostics&) = delete;
    GpuDiagnostics(GpuDiagnostics&&) = delete;
    GpuDiagnostics& operator=(GpuDiagnostics&&) = delete;

    /**
     * @brief Loads OpenGL entry points and captures adapter information.
     *
     * A current raylib OpenGL context must exist before this call.
     */
    void Initialize();

    /**
     * @brief Releases timer-query objects while the OpenGL context is alive.
     */
    void Shutdown();

    /**
     * @brief Starts per-application-frame sampling and polls old query results.
     *
     * @param cpu_frame_seconds Previous complete application frame duration.
     */
    void BeginApplicationFrame(float cpu_frame_seconds);

    /**
     * @brief Marks whether the current frame contains an active 3D world draw.
     *
     * @param active True when the 3D preview has uploaded render resources.
     */
    void SetRenderActive(bool active);

    /**
     * @brief Begins an asynchronous GPU timer around 3D world rendering.
     */
    void BeginWorldRender();

    /**
     * @brief Ends the current asynchronous GPU timer, if one was started.
     */
    void EndWorldRender();

    /**
     * @brief Returns the latest stable diagnostics snapshot.
     */
    [[nodiscard]] const GpuDiagnosticsSnapshot& Snapshot() const;

private:
    struct QuerySlot {
        std::uint32_t id = 0;
        bool pending = false;
    };

    void PollTimerQueries();
    void UpdateDisplayAverages();
    void SampleDriverMemory();
    void RecordGpuSample(double milliseconds);
    void ResetState();

    class Impl;
    Impl* impl_ = nullptr;
    std::array<QuerySlot, 4> queries_{};
    std::size_t next_query_ = 0;
    std::size_t active_query_ = 0;
    bool query_active_ = false;
    double sample_window_start_seconds_ = 0.0;
    double driver_sample_time_seconds_ = 0.0;
    double cpu_sample_sum_ms_ = 0.0;
    std::uint64_t cpu_sample_count_ = 0;
    double gpu_sample_sum_ms_ = 0.0;
    std::uint64_t gpu_sample_count_ = 0;
    GpuDiagnosticsSnapshot snapshot_;
};

}  // namespace vox3d
