#include "vox3d/map/runtime_map.hpp"

namespace vox3d {

bool RuntimeMapInfo::IsValid() const
{
    return width > 0 && height > 0;
}

bool RuntimeMap::IsValid() const
{
    return info.IsValid();
}

RuntimeMap BuildRuntimeMap(const MapPackageInfo& package)
{
    RuntimeMap runtime;
    runtime.info.width = package.width.value_or(0);
    runtime.info.height = package.height.value_or(0);
    runtime.info.tile_size_px = package.tile_size.value_or(0);
    if (package.min_level.has_value() && package.max_level.has_value()) {
        runtime.info.levels = LevelRange{*package.min_level, *package.max_level};
    }
    runtime.info.generator_version = package.generator_version;
    runtime.info.schema_version = package.schema_version;
    runtime.info.terrain_loaded = package.terrain_available;
    runtime.info.elevation_loaded = package.elevation_available;
    runtime.info.collision_loaded = package.collision_available;
    runtime.overview = package.overview;
    return runtime;
}

}  // namespace vox3d
