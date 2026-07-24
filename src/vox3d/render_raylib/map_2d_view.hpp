#pragma once

#include "vox3d/core/types.hpp"
#include "vox3d/map/map_package.hpp"
#include "vox3d/map/runtime_map.hpp"

#include <raylib.h>

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace vox3d {

/**
 * @brief Runtime status exposed by the interactive 2D map view.
 */
struct Map2DViewStatus {
    bool loaded = false;
    bool initialized = false;
    bool panning = false;
    bool hover_tile_valid = false;
    TileCoord hover_tile;
    Vector2 center_tile{};
    float pixels_per_tile = 0.0F;
    float fit_pixels_per_tile = 0.0F;
};

/**
 * @brief One color key displayed in the 2D diagnostic layer legend.
 */
struct Map2DLegendEntry {
    Color color{};
    std::string_view label;
};

/**
 * @brief Optional vector overlays drawn above a 2D diagnostic base layer.
 *
 * Partition sizes are expressed in map tiles. Invalid or zero partition sizes
 * disable the corresponding overlay. Start, goal, object, place, marker, and
 * selection coordinates are ignored when they fall outside the loaded map.
 */
struct Map2DOverlayOptions {
    bool show_grid = false;
    bool show_chunks = false;
    int chunk_size_x = 0;
    int chunk_size_y = 0;
    bool show_vxmap_regions = false;
    int vxmap_region_size_tiles = 0;
    bool show_start_goal = false;
    std::optional<TileCoord> start;
    std::optional<TileCoord> goal;
    bool show_objects = false;
    std::span<const RuntimeMapObject> objects;
    bool show_places = false;
    std::span<const RuntimePlace> places;
    bool show_markers = false;
    std::span<const RuntimeMapMarker> markers;
    std::optional<TileCoord> selection;
};

/**
 * @brief Returns the stable legend entries for a 2D diagnostic base layer.
 *
 * @param base_layer Layer whose color meaning should be described.
 * @return Non-owning view of static legend entries.
 */
[[nodiscard]] std::span<const Map2DLegendEntry> Map2DLegendFor(
    Map2DBaseLayer base_layer);

/**
 * @brief Raylib-backed interactive top-down map view.
 *
 * The view owns one-pixel-per-tile diagnostic textures and a Camera2D state.
 * Map coordinates use tile space where x grows right and y grows down. Mouse
 * wheel zoom is anchored at the cursor and middle-button dragging pans the map.
 */
class Map2DView {
public:
    /**
     * @brief Creates an empty 2D map view.
     */
    Map2DView() = default;

    /**
     * @brief Releases the owned textures if they are still loaded.
     */
    ~Map2DView();

    /**
     * @brief Disables copying because the view owns raylib textures.
     */
    Map2DView(const Map2DView&) = delete;

    /**
     * @brief Disables copy assignment because the view owns raylib textures.
     *
     * @return Reference to this view; the function is deleted and never called.
     */
    Map2DView& operator=(const Map2DView&) = delete;

    /**
     * @brief Builds all available dense diagnostic textures from a runtime map.
     *
     * Existing texture resources are released before loading the new map. All
     * core grids must be valid and have identical dimensions. The caller must
     * have an initialized raylib window and graphics context.
     *
     * @param runtime_map Runtime map containing normalized core grids.
     * @return True when all three diagnostic textures were created successfully.
     */
    [[nodiscard]] bool Load(const RuntimeMap& runtime_map);

    /**
     * @brief Builds a terrain-only texture from a map overview.
     *
     * This fallback preserves 2D terrain inspection when full runtime grids are
     * unavailable. Elevation and collision layers remain unavailable.
     *
     * @param overview One-cell-per-tile diagnostic map overview.
     * @return True when the terrain texture was created successfully.
     */
    [[nodiscard]] bool Load(const MapOverview& overview);

    /**
     * @brief Releases all owned textures and clears camera state.
     */
    void Unload();

    /**
     * @brief Updates hover, wheel zoom, and middle-button panning.
     *
     * @param viewport Screen-space rectangle used by the 2D map.
     * @param enabled True when the 2D workspace mode is active.
     */
    void Update(Rectangle viewport, bool enabled);

    /**
     * @brief Fits the entire map inside the current viewport.
     *
     * @param viewport Screen-space rectangle used by the 2D map.
     */
    void FitToMap(Rectangle viewport);

    /**
     * @brief Restores a centered default inspection view.
     *
     * The reset scale is at least one pixel per tile and at least twice the fit
     * scale, capped by the maximum supported zoom.
     *
     * @param viewport Screen-space rectangle used by the 2D map.
     */
    void ResetView(Rectangle viewport);

    /**
     * @brief Changes zoom by discrete wheel-like steps around viewport center.
     *
     * Positive steps zoom in and negative steps zoom out.
     *
     * @param steps Signed zoom step count.
     * @param viewport Screen-space rectangle used by the 2D map.
     */
    void AdjustZoom(int steps, Rectangle viewport);

    /**
     * @brief Centers the view on one tile and ensures an inspection-scale zoom.
     *
     * Existing zoom is preserved when it is already closer than the minimum
     * inspection scale. The target is clamped so the map remains inside the
     * viewport as far as its dimensions allow.
     *
     * @param tile Tile coordinate to center.
     * @param viewport Screen-space rectangle used by the 2D map.
     * @return True when the tile is inside the loaded map and focus changed.
     */
    [[nodiscard]] bool FocusTile(TileCoord tile, Rectangle viewport);

    /**
     * @brief Converts a screen point to a map tile coordinate.
     *
     * @param screen_point Global screen-space point.
     * @param viewport Screen-space rectangle used by the 2D map.
     * @return Tile coordinate when the point is inside the map, otherwise null.
     */
    [[nodiscard]] std::optional<TileCoord> ScreenToTile(
        Vector2 screen_point,
        Rectangle viewport) const;

    /**
     * @brief Draws the selected base texture and enabled vector overlays.
     *
     * @param viewport Screen-space rectangle used by the 2D map.
     * @param base_layer Diagnostic base layer to display.
     * @param overlays Grid, partition, endpoint, and selection overlay options.
     */
    void Draw(
        Rectangle viewport,
        Map2DBaseLayer base_layer,
        const Map2DOverlayOptions& overlays) const;

    /**
     * @brief Returns true when at least the terrain texture is loaded.
     *
     * @return True when the map can be rendered.
     */
    [[nodiscard]] bool IsLoaded() const;

    /**
     * @brief Returns whether a specific diagnostic base layer is available.
     *
     * @param base_layer Layer to query.
     * @return True when the corresponding texture is loaded.
     */
    [[nodiscard]] bool IsLayerLoaded(Map2DBaseLayer base_layer) const;

    /**
     * @brief Returns the latest texture-loading failure reason.
     *
     * @return Stable diagnostic string, empty after a successful load.
     */
    [[nodiscard]] std::string_view LastLoadError() const;

    /**
     * @brief Returns the latest camera and hover diagnostics.
     *
     * @return Current 2D map view status.
     */
    [[nodiscard]] Map2DViewStatus Status() const;

private:
    [[nodiscard]] Camera2D CameraFor(Rectangle viewport) const;
    [[nodiscard]] float FitZoom(Rectangle viewport) const;
    [[nodiscard]] const Texture2D* TextureFor(Map2DBaseLayer base_layer) const;
    void EnsureViewport(Rectangle viewport);
    void ZoomAt(Vector2 screen_point, float steps, Rectangle viewport);
    void ClampTarget(Rectangle viewport);

    Texture2D terrain_texture_{};
    Texture2D elevation_texture_{};
    Texture2D collision_texture_{};
    Texture2D movement_cost_texture_{};
    Texture2D projectile_block_texture_{};
    Texture2D vision_block_texture_{};
    Texture2D cover_texture_{};
    Texture2D concealment_texture_{};
    int map_width_ = 0;
    int map_height_ = 0;
    Vector2 target_{};
    Rectangle last_viewport_{};
    float zoom_ = 1.0F;
    float fit_zoom_ = 1.0F;
    bool loaded_ = false;
    bool initialized_ = false;
    bool panning_ = false;
    bool hover_tile_valid_ = false;
    TileCoord hover_tile_{};
    std::string last_load_error_;
};

}  // namespace vox3d
