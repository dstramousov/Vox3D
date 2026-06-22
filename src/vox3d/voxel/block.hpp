#pragma once

#include <cstdint>
#include <string_view>

namespace vox3d {

/**
 * @brief Integer coordinate of one voxel block in map space.
 *
 * The x and y axes use tile coordinates. The z axis uses the integer map
 * elevation level from the loaded runtime height grid.
 */
struct BlockCoord {
    int x = 0;
    int y = 0;
    int z = 0;
};

/**
 * @brief Stable block category used by the voxel-world foundation.
 */
enum class BlockTypeId : std::uint8_t {
    kEmpty,
    kSubsurface,
    kTerrainSurface,
    kBlockedSurface,
};

/**
 * @brief Runtime voxel block returned by block lookup helpers.
 */
struct VoxelBlock {
    BlockCoord coord;
    BlockTypeId type = BlockTypeId::kEmpty;
    bool solid = false;
    bool blocked = false;
    bool destructible = false;

    /**
     * @brief Returns true when the block occupies space.
     *
     * @return True for solid voxel blocks, false for empty space.
     */
    [[nodiscard]] bool IsSolid() const;
};

/**
 * @brief Converts a block type to a stable lowercase diagnostic name.
 *
 * @param type Block type identifier.
 * @return Stable string representation.
 */
[[nodiscard]] std::string_view ToString(BlockTypeId type);

}  // namespace vox3d
