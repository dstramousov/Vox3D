#include "app.hpp"
#include "app_config.hpp"
#include "config_loader.hpp"
#include "logger.hpp"
#include "ui_labels.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

[[nodiscard]] bool StartsWith(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

[[nodiscard]] std::filesystem::path ParseConfigPath(int argc, char** argv)
{
    std::filesystem::path config_path = "config/app.json";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (StartsWith(arg, "--config=")) {
            config_path = arg.substr(std::string("--config=").size());
        }
    }
    return config_path;
}

void ApplyCommandLineArguments(int argc, char** argv, vox3d::AppConfig& config)
{
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--debug-ui") {
            config.debug_ui = true;
            continue;
        }
        if (arg == "--no-color") {
            config.no_color = true;
            continue;
        }
        if (StartsWith(arg, "--config=")) {
            continue;
        }
        if (StartsWith(arg, "--log-level=")) {
            const std::string level_value = arg.substr(std::string("--log-level=").size());
            vox3d::LogLevel parsed_level = config.log_level;
            if (vox3d::ParseLogLevel(level_value, parsed_level)) {
                config.log_level = parsed_level;
            } else {
                config.unknown_arguments.push_back(arg);
            }
            continue;
        }
        if (StartsWith(arg, "--raylib-log-level=")) {
            config.raylib_log_level = arg.substr(std::string("--raylib-log-level=").size());
            continue;
        }
        if (StartsWith(arg, "--language=")) {
            config.language = arg.substr(std::string("--language=").size());
            continue;
        }
        if (StartsWith(arg, "--map=")) {
            config.map_package_path = arg.substr(std::string("--map=").size());
            continue;
        }
        config.unknown_arguments.push_back(arg);
    }
}

}  // namespace

int main(int argc, char** argv)
{
    vox3d::AppConfig config;
    std::vector<std::string> config_diagnostics;
    const std::filesystem::path config_path = ParseConfigPath(argc, argv);
    const bool config_loaded = vox3d::LoadAppConfigFromFile(config_path, config, config_diagnostics);
    ApplyCommandLineArguments(argc, argv, config);

    vox3d::Logger logger(
        vox3d::LoggerConfig{config.log_level, vox3d::ShouldUseColor(config.no_color || !config.log_color), "main"},
        std::cout);

    if (config_loaded) {
        logger.Info("config", "loaded path=\"" + config.config_path.string() + "\"");
    }
    for (const auto& diagnostic : config_diagnostics) {
        logger.Warn("config", diagnostic);
    }
    for (const auto& arg : config.unknown_arguments) {
        logger.Warn("cli", "unknown argument ignored arg=\"" + arg + "\"");
    }

    vox3d::UiLabels labels = vox3d::DefaultUiLabels(config.language);
    std::vector<std::string> label_diagnostics;
    const std::filesystem::path labels_path = config.language_dir / (config.language + ".json");
    const bool labels_loaded = vox3d::LoadUiLabelsFromFile(labels_path, labels, label_diagnostics);
    if (labels_loaded) {
        logger.Info("labels", "loaded language=" + labels.language + " path=\"" + labels_path.string() + "\"");
    }
    for (const auto& diagnostic : label_diagnostics) {
        logger.Warn("labels", diagnostic);
    }

    vox3d::App app(config, logger, labels);
    if (!app.Initialize()) {
        app.Shutdown();
        return 1;
    }

    const int exit_code = app.Run();
    app.Shutdown();
    return exit_code;
}
