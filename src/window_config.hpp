#pragma once

#include "app_config.hpp"

#include <iosfwd>

namespace vox3d {

/**
 * @brief Calculated window placement and UI scale.
 */
struct WindowConfig {
    int monitor_index = 0;
    int monitor_x = 0;
    int monitor_y = 0;
    int monitor_width = 0;
    int monitor_height = 0;
    int window_width = 1280;
    int window_height = 720;
    int window_x = 0;
    int window_y = 0;
    float ui_scale = 1.0F;
};

/**
 * @brief Calculates safe window size, centered position, and UI scale.
 *
 * @param monitor_x Monitor origin X.
 * @param monitor_y Monitor origin Y.
 * @param monitor_width Monitor width in pixels.
 * @param monitor_height Monitor height in pixels.
 * @param config Application configuration.
 * @return Calculated window configuration.
 */
[[nodiscard]] WindowConfig CalculateWindowConfig(
    int monitor_x,
    int monitor_y,
    int monitor_width,
    int monitor_height,
    const AppConfig& config);

/**
 * @brief Calculates UI scale for the current window size.
 *
 * @param window_width Current window width in pixels.
 * @param window_height Current window height in pixels.
 * @param config Application configuration.
 * @return Clamped UI scale.
 */
[[nodiscard]] float CalculateUiScale(int window_width, int window_height, const AppConfig& config);

/**
 * @brief Writes a compact debug representation of the window configuration.
 *
 * @param os Destination stream.
 * @param config Window configuration to dump.
 * @return Destination stream.
 */
std::ostream& operator<<(std::ostream& os, const WindowConfig& config);

}  // namespace vox3d
