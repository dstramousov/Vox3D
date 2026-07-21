#pragma once

#include "vox3d/core/result.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace vox3d {

/**
 * @brief runtime_binary block discovered in map.json.
 */
struct VxmapRuntimeManifest {
    bool declared = false;
    std::filesystem::path relative_path;
    std::string format;
    int format_major = 0;
    int format_minor = 0;
    std::string build_id_hex;
    std::uint64_t file_size = 0;
    std::uint32_t section_count = 0;
    std::uint16_t region_size_tiles = 0;
    int regions_x = 0;
    int regions_y = 0;
    int regions_total = 0;
};

/**
 * @brief Lightweight validation report for a vxmap-runtime-v1 container.
 *
 * The first integration step validates the binary fast-path container and logs
 * whether it is usable. It intentionally does not replace the JSON runtime-map
 * builder yet.
 */
struct VxmapRuntimeValidationReport {
    bool present = false;
    bool valid = false;
    std::filesystem::path path;
    std::string fallback_reason;
    std::uint32_t format_major = 0;
    std::uint32_t format_minor = 0;
    std::uint32_t section_count = 0;
    std::uint32_t region_count = 0;
    std::uint32_t terrain_count = 0;
    std::uint32_t width_tiles = 0;
    std::uint32_t height_tiles = 0;
    std::uint16_t tile_size_px = 0;
    std::int16_t min_elevation = 0;
    std::int16_t max_elevation = 0;
    std::uint64_t file_size = 0;
    std::string build_id_hex;
    Diagnostics diagnostics;
};

/**
 * @brief Validates a vxmap-runtime-v1 container without building RuntimeMap.
 *
 * @param package_path Directory containing map.json and map_runtime.vxmap.
 * @param manifest runtime_binary block parsed from map.json.
 * @return Container validation report with a fallback reason on failure.
 */
[[nodiscard]] VxmapRuntimeValidationReport ValidateVxmapRuntimeBinary(
    const std::filesystem::path& package_path,
    const VxmapRuntimeManifest& manifest);

/**
 * @brief Compact log string for the binary container validation report.
 *
 * @param report Validation report.
 * @return Stable human-readable diagnostics line.
 */
[[nodiscard]] std::string ToLogString(const VxmapRuntimeValidationReport& report);

}  // namespace vox3d
