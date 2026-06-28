#pragma once

#include "vox3d/core/result.hpp"
#include "vox3d/core/types.hpp"
#include "vox3d/map/runtime_map.hpp"
#include "vox3d/mesh/face.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace vox3d {

/**
 * @brief Semantic type of an elevation transition between adjacent tiles.
 */
enum class TransitionFeatureKind : std::uint8_t {
    kRamp,
    kStairs,
    kBridge,
    kDrop,
};

/**
 * @brief Converts a transition feature kind to a stable diagnostic name.
 *
 * @param kind Transition feature kind.
 * @return Stable lowercase string representation.
 */
[[nodiscard]] std::string_view ToString(TransitionFeatureKind kind);

/**
 * @brief One semantic elevation transition detected between adjacent map tiles.
 *
 * The feature is renderer-independent. It does not imply final gameplay
 * movement, final collision, or final mesh geometry. The first version uses
 * the dense runtime height and collision grids to build useful debug markers.
 */
struct TransitionFeature {
    TransitionFeatureKind kind = TransitionFeatureKind::kDrop;
    TileCoord from_tile;
    TileCoord to_tile;
    int from_level = 0;
    int to_level = 0;
    int delta_levels = 0;
    FaceDirection direction = FaceDirection::kEast;
    bool passable = false;
};

/**
 * @brief Aggregate transition feature counters.
 */
struct TransitionFeatureStats {
    std::uint64_t total = 0;
    std::uint64_t ramps = 0;
    std::uint64_t stairs = 0;
    std::uint64_t bridges = 0;
    std::uint64_t drops = 0;
    std::uint64_t passable = 0;
    std::uint64_t blocked = 0;

    /**
     * @brief Returns true when at least one transition was detected.
     *
     * @return True if the feature set is non-empty.
     */
    [[nodiscard]] bool IsValid() const;
};

/**
 * @brief Renderer-independent elevation transition feature set.
 */
struct TransitionFeatureSet {
    TransitionFeatureStats stats;
    std::vector<TransitionFeature> features;
    Diagnostics diagnostics;

    /**
     * @brief Returns true when the feature set was built successfully.
     *
     * Empty maps with zero transitions are still valid if no diagnostics were
     * emitted by the builder.
     *
     * @return True when counters match feature storage.
     */
    [[nodiscard]] bool IsValid() const;
};

/**
 * @brief Builds elevation transition features from runtime height/collision grids.
 *
 * This foundation pass scans east and south neighbour pairs once. It classifies
 * one-level differences as ramps, two-level differences as stairs, and larger
 * differences as drops. Bridges are reserved for explicit map data and are not
 * inferred from height alone.
 *
 * @param map Runtime map containing dense height and collision grids.
 * @return Detected transition features with aggregate counters.
 */
[[nodiscard]] TransitionFeatureSet BuildTransitionFeatures(const RuntimeMap& map);

/**
 * @brief Builds a compact stable log string for transition diagnostics.
 *
 * @param features Transition feature set.
 * @return Compact human-readable summary.
 */
[[nodiscard]] std::string ToLogString(const TransitionFeatureSet& features);

}  // namespace vox3d
