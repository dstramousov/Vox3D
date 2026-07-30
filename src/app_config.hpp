#pragma once

#include "logger.hpp"

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#ifndef VOX3D_VERSION
#define VOX3D_VERSION "0.5.106-dev"
#endif

namespace vox3d {

/**
 * @brief Runtime application configuration loaded from defaults, config file, and CLI overrides.
 */
struct AppConfig {
    std::string app_name = "VoX3D";
    std::string version = VOX3D_VERSION;
    std::filesystem::path config_path = "config/app.json";
    std::string language = "en";
    std::filesystem::path language_dir = "res/lang";
    std::filesystem::path map_package_path;

    bool vegetation_models_enabled = false;
    int vegetation_model_tree_limit = 0;
    std::filesystem::path vegetation_model_asset_directory = "assets/models/trees";
    float vegetation_model_cull_distance = 320.0F;
    float vegetation_model_cull_transition_width = 64.0F;
    bool vegetation_adaptive_cull_enabled = true;
    float vegetation_adaptive_cull_min_distance = 160.0F;
    float vegetation_adaptive_cull_max_distance = 2048.0F;
    float vegetation_adaptive_cull_increase_step = 32.0F;
    float vegetation_adaptive_cull_decrease_step = 64.0F;
    float vegetation_adaptive_cull_increase_delay_seconds = 5.0F;
    float vegetation_adaptive_cull_decrease_delay_seconds = 1.0F;
    float vegetation_adaptive_cull_cooldown_seconds = 2.0F;
    float vegetation_adaptive_cull_low_frame_ratio = 0.60F;
    float vegetation_adaptive_cull_high_frame_ratio = 0.82F;
    bool vegetation_altitude_zoning_enabled = true;
    std::array<float, 6> vegetation_altitude_elevations{-1.0F, 5.0F, 10.0F, 15.0F, 18.0F, 20.0F};
    std::array<float, 6> vegetation_altitude_deciduous{0.70F, 0.50F, 0.20F, 0.05F, 0.00F, 0.00F};
    std::array<float, 6> vegetation_altitude_conifer{0.28F, 0.45F, 0.68F, 0.60F, 0.40F, 0.00F};
    std::array<float, 6> vegetation_altitude_dead{0.02F, 0.05F, 0.12F, 0.35F, 0.60F, 1.00F};
    std::array<float, 6> vegetation_altitude_density{1.00F, 1.00F, 0.85F, 0.55F, 0.15F, 0.02F};
    std::array<float, 6> vegetation_altitude_scale{1.00F, 0.98F, 0.90F, 0.72F, 0.55F, 0.45F};
    bool vegetation_foliage_color_zoning_enabled = true;
    std::array<float, 6> vegetation_deciduous_olive{0.06F, 0.18F, 0.32F, 0.35F, 0.30F, 0.25F};
    std::array<float, 6> vegetation_deciduous_ochre{0.01F, 0.09F, 0.23F, 0.45F, 0.65F, 0.70F};
    std::array<float, 6> vegetation_conifer_cool{0.10F, 0.17F, 0.30F, 0.38F, 0.35F, 0.30F};
    std::array<float, 6> vegetation_conifer_brown{0.00F, 0.03F, 0.07F, 0.17F, 0.35F, 0.45F};
    bool vegetation_lighting_enabled = true;
    float vegetation_light_direction_x = -0.45F;
    float vegetation_light_direction_y = -1.0F;
    float vegetation_light_direction_z = -0.35F;
    float vegetation_light_ambient = 0.58F;
    float vegetation_light_diffuse = 0.62F;
    float vegetation_light_hemisphere = 0.18F;
    float vegetation_crown_bottom_shading = 0.14F;
    float vegetation_flat_foliage_shading = 0.90F;

    bool world_lighting_enabled = true;
    float world_light_direction_x = -0.60F;
    float world_light_direction_y = -1.00F;
    float world_light_direction_z = -0.40F;
    std::array<float, 3> world_sun_color{1.00F, 0.93F, 0.82F};
    std::array<float, 3> world_sky_ambient_color{0.48F, 0.58F, 0.72F};
    std::array<float, 3> world_ground_ambient_color{0.30F, 0.27F, 0.23F};
    float world_sun_intensity = 0.86F;
    float world_ambient_intensity = 0.38F;
    float world_top_brightness = 1.08F;
    float world_side_brightness = 0.94F;
    float world_bottom_brightness = 0.62F;
    float world_color_variation = 0.04F;

    int base_width = 1280;
    int base_height = 720;
    int fallback_width = 1024;
    int fallback_height = 576;
    float max_monitor_fraction = 0.90F;
    bool window_resizable = false;
    bool window_fullscreen = false;
    bool window_vsync = true;
    int target_fps = 60;

    float ui_scale_min = 0.75F;
    float ui_scale_max = 2.00F;
    float ui_font_scale = 1.00F;
    std::filesystem::path ui_title_font_path = "res/fonts/Noto_Sans/static/NotoSans-Bold.ttf";
    std::filesystem::path ui_text_font_path = "res/fonts/Noto_Sans/static/NotoSans-Regular.ttf";

    bool debug_ui = false;
    bool no_color = false;
    bool log_color = true;
    LogLevel log_level = LogLevel::kInfo;
    std::string raylib_log_level = "warning";

    std::vector<std::string> unknown_arguments;
};

}  // namespace vox3d
