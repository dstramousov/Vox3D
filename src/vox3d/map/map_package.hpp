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
 * @brief runtime_binary block from map.json for the vxmap fast path.
 */
struct RuntimeBinaryInfo {
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
    bool runtime_grids_available = false;
    std::optional<int> width;
    std::optional<int> height;
    std::optional<int> tile_size;
    std::optional<int> min_level;
    std::optional<int> max_level;
    std::optional<long long> resolved_seed;
    std::optional<int> object_count;
    std::optional<int> place_count;
    std::optional<int> marker_count;
    std::string schema_version;
    std::string package_schema_version;
    std::string generator_version;
    std::string pipeline_version;
    std::string profile;
    std::string source_file;
    std::string status;
    RuntimeBinaryInfo runtime_binary;
    MapOverview overview;
    std::vector<std::string> present_files;
    std::vector<std::string> warnings;
};

/**
 * @brief Inspects a map package path and loads a bounded diagnostic overview.
 *
 * The loader validates the directory, reads the TopDownMapGen v0.0.68
 * map_package/map.json metadata, inspects known package files, and builds a
 * bounded top-down terrain preview from layers/terrain.json. Missing optional
 * files are reported as warnings, not fatal errors.
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
