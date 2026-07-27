#pragma once

#include "logger.hpp"

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#ifndef VOX3D_VERSION
#define VOX3D_VERSION "0.5.94-dev"
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
    float vegetation_model_lod_near_distance = 70.0F;
    float vegetation_model_lod_far_distance = 140.0F;
    float vegetation_model_cull_distance = 220.0F;
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
