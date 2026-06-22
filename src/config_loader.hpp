#pragma once

#include "app_config.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace vox3d {

/**
 * @brief Loads application configuration from a JSON file.
 *
 * Missing files keep the default configuration and produce a diagnostic message.
 * Invalid fields are ignored individually when possible.
 *
 * @param config_path Path to the JSON configuration file.
 * @param config Configuration object to update.
 * @param diagnostics Human-readable warnings collected during loading.
 * @return True if the configuration file was read and parsed successfully.
 */
[[nodiscard]] bool LoadAppConfigFromFile(
    const std::filesystem::path& config_path,
    AppConfig& config,
    std::vector<std::string>& diagnostics);

}  // namespace vox3d
