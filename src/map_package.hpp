#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace vox3d {

/**
 * @brief Compact terrain category used by the diagnostic map overview.
 */
enum class MapCellKind : std::uint8_t {
    kUnknown,
    kOpen,
    kForest,
    kWater,
    kRoad,
    kSwamp,
    kRuins,
    kBuilding,
    kBlocked,
    kStart,
    kGoal,
};

/**
 * @brief Lightweight top-down terrain preview extracted from a map package.
 */
struct MapOverview {
    int width = 0;
    int height = 0;
    std::vector<MapCellKind> cells;
    std::string source_file;
    bool terrain_loaded = false;

    /**
     * @brief Returns true if the preview contains one cell per map tile.
     *
     * @return True when the preview can be drawn as a tile grid.
     */
    [[nodiscard]] bool IsValid() const;
};

/**
 * @brief Lightweight metadata discovered from a map package directory.
 */
struct MapPackageInfo {
    std::filesystem::path path;
    bool configured = false;
    bool exists = false;
    bool is_directory = false;
    bool loaded = false;
    bool metadata_available = false;
    bool terrain_available = false;
    bool elevation_available = false;
    bool collision_available = false;
    std::optional<int> width;
    std::optional<int> height;
    std::optional<int> tile_size;
    std::optional<int> min_level;
    std::optional<int> max_level;
    std::string source_file;
    std::string status;
    MapOverview overview;
    std::vector<std::string> present_files;
    std::vector<std::string> warnings;
};

/**
 * @brief Inspects a map package path and loads a bounded diagnostic overview.
 *
 * The loader validates the directory, looks for common metadata/runtime-grid files,
 * extracts a small summary, and builds a top-down terrain preview when a supported
 * grid file is present. Missing optional files are reported as warnings, not fatal
 * errors.
 *
 * @param package_path Path to the map package directory.
 * @return Discovered map package information.
 */
[[nodiscard]] MapPackageInfo LoadMapPackageInfo(const std::filesystem::path& package_path);

/**
 * @brief Builds a compact stable log string for map package diagnostics.
 *
 * @param info Map package information.
 * @return Compact human-readable summary.
 */
[[nodiscard]] std::string ToLogString(const MapPackageInfo& info);

}  // namespace vox3d
