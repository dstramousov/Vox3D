#pragma once

#include "logger.hpp"

#include <filesystem>
#include <string>
#include <vector>

#ifndef VOX3D_VERSION
#define VOX3D_VERSION "0.4.5-dev"
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

    int base_width = 1280;
    int base_height = 720;
    int fallback_width = 1024;
    int fallback_height = 576;
    float max_monitor_fraction = 0.90F;
    bool window_resizable = false;
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
