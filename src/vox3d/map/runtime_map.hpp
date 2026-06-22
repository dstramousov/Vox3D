#pragma once

#include "vox3d/core/types.hpp"
#include "vox3d/map/map_package.hpp"

#include <optional>
#include <string>

namespace vox3d {

/**
 * @brief Normalized runtime map summary used by future voxel/chunk builders.
 */
struct RuntimeMapInfo {
    int width = 0;
    int height = 0;
    int tile_size_px = 0;
    std::optional<LevelRange> levels;
    std::string generator_version;
    std::string schema_version;
    bool terrain_loaded = false;
    bool elevation_loaded = false;
    bool collision_loaded = false;

    /**
     * @brief Returns true when the runtime map dimensions are usable.
     *
     * @return True if width and height are positive.
     */
    [[nodiscard]] bool IsValid() const;
};

/**
 * @brief Lightweight runtime map object produced from a loaded map package.
 */
struct RuntimeMap {
    RuntimeMapInfo info;
    MapOverview overview;

    /**
     * @brief Returns true when the runtime map contains usable dimensions.
     *
     * @return True if the runtime map info is valid.
     */
    [[nodiscard]] bool IsValid() const;
};

/**
 * @brief Builds the first normalized runtime-map shell from package information.
 *
 * This does not build voxels or chunks yet. It gives the upcoming engine layers a
 * stable value object independent from the editor UI.
 *
 * @param package Loaded map package information.
 * @return Lightweight runtime map shell.
 */
[[nodiscard]] RuntimeMap BuildRuntimeMap(const MapPackageInfo& package);

}  // namespace vox3d
