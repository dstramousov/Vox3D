#pragma once

#include "vox3d/core/types.hpp"
#include "vox3d/map/map_package.hpp"

#include <raylib.h>

#include <optional>

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
 * @brief Raylib-backed interactive top-down map view.
 *
 * The view owns a one-pixel-per-tile terrain texture and a Camera2D state.
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
     * @brief Releases the owned texture if it is still loaded.
     */
    ~Map2DView();

    /**
     * @brief Disables copying because the view owns a raylib texture.
     */
    Map2DView(const Map2DView&) = delete;

    /**
     * @brief Disables copy assignment because the view owns a raylib texture.
     *
     * @return Reference to this view; the function is deleted and never called.
     */
    Map2DView& operator=(const Map2DView&) = delete;

    /**
     * @brief Builds the diagnostic terrain texture from a map overview.
     *
     * Existing texture resources are released before loading the new map.
     * The caller must have an initialized raylib window and graphics context.
     *
     * @param overview One-cell-per-tile diagnostic map overview.
     * @return True when the texture was created successfully.
     */
    [[nodiscard]] bool Load(const MapOverview& overview);

    /**
     * @brief Releases the owned terrain texture and clears camera state.
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
     * @brief Draws the terrain texture, zoom-dependent grid, and selection.
     *
     * @param viewport Screen-space rectangle used by the 2D map.
     * @param show_terrain True to draw the terrain texture.
     * @param show_grid True to draw the zoom-dependent grid overlay.
     * @param selection Optional selected tile highlighted above the map.
     */
    void Draw(
        Rectangle viewport,
        bool show_terrain,
        bool show_grid,
        std::optional<TileCoord> selection) const;

    /**
     * @brief Returns true when a valid terrain texture is loaded.
     *
     * @return True when the map can be rendered.
     */
    [[nodiscard]] bool IsLoaded() const;

    /**
     * @brief Returns the latest camera and hover diagnostics.
     *
     * @return Current 2D map view status.
     */
    [[nodiscard]] Map2DViewStatus Status() const;

private:
    [[nodiscard]] Camera2D CameraFor(Rectangle viewport) const;
    [[nodiscard]] float FitZoom(Rectangle viewport) const;
    void EnsureViewport(Rectangle viewport);
    void ZoomAt(Vector2 screen_point, float steps, Rectangle viewport);
    void ClampTarget(Rectangle viewport);

    Texture2D terrain_texture_{};
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
};

}  // namespace vox3d
