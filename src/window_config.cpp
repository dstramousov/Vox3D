#include "window_config.hpp"

#include <algorithm>
#include <cmath>
#include <ostream>

namespace vox3d {
namespace {

[[nodiscard]] bool Fits(int width, int height, int max_width, int max_height)
{
    return width <= max_width && height <= max_height;
}

}  // namespace

float CalculateUiScale(int window_width, int window_height, const AppConfig& config)
{
    const float scale_x = static_cast<float>(window_width) / static_cast<float>(config.base_width);
    const float scale_y = static_cast<float>(window_height) / static_cast<float>(config.base_height);
    return std::clamp(std::min(scale_x, scale_y), config.ui_scale_min, config.ui_scale_max);
}

WindowConfig CalculateWindowConfig(
    int monitor_x,
    int monitor_y,
    int monitor_width,
    int monitor_height,
    const AppConfig& config)
{
    WindowConfig result;
    result.monitor_x = monitor_x;
    result.monitor_y = monitor_y;
    result.monitor_width = monitor_width;
    result.monitor_height = monitor_height;

    const int safe_monitor_width = std::max(monitor_width, config.fallback_width);
    const int safe_monitor_height = std::max(monitor_height, config.fallback_height);
    const int max_window_width = std::max(1, static_cast<int>(std::floor(safe_monitor_width * config.max_monitor_fraction)));
    const int max_window_height = std::max(1, static_cast<int>(std::floor(safe_monitor_height * config.max_monitor_fraction)));

    if (Fits(config.base_width, config.base_height, max_window_width, max_window_height)) {
        result.window_width = config.base_width;
        result.window_height = config.base_height;
    } else if (Fits(config.fallback_width, config.fallback_height, max_window_width, max_window_height)) {
        result.window_width = config.fallback_width;
        result.window_height = config.fallback_height;
    } else {
        int width = max_window_width;
        int height = static_cast<int>(std::round(width * 9.0 / 16.0));
        if (height > max_window_height) {
            height = max_window_height;
            width = static_cast<int>(std::round(height * 16.0 / 9.0));
        }
        result.window_width = std::max(640, width);
        result.window_height = std::max(360, height);
    }

    result.window_width = std::min(result.window_width, max_window_width);
    result.window_height = std::min(result.window_height, max_window_height);

    result.window_x = monitor_x + (safe_monitor_width - result.window_width) / 2;
    result.window_y = monitor_y + (safe_monitor_height - result.window_height) / 2;

    result.ui_scale = CalculateUiScale(result.window_width, result.window_height, config);

    return result;
}

std::ostream& operator<<(std::ostream& os, const WindowConfig& config)
{
    os << "WindowConfig { monitor=" << config.monitor_width << 'x' << config.monitor_height
       << " window=" << config.window_width << 'x' << config.window_height << " pos=" << config.window_x << ','
       << config.window_y << " ui_scale=" << config.ui_scale << " }";
    return os;
}

}  // namespace vox3d
