#include "vox3d/render_raylib/map_2d_view.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace vox3d {
namespace {

constexpr float kViewportPadding = 8.0F;
constexpr float kZoomStep = 1.20F;
constexpr float kResetFitMultiplier = 2.0F;
constexpr float kMaximumPixelsPerTile = 64.0F;
constexpr float kViewportChangeEpsilon = 0.5F;
constexpr float kTileGridThreshold = 6.0F;
constexpr float kMajorGridThreshold = 1.5F;
constexpr int kMajorGridStep = 16;

[[nodiscard]] Color CellColor(MapCellKind cell)
{
    switch (cell) {
        case MapCellKind::kOpen:
            return Color{92, 150, 82, 255};
        case MapCellKind::kForest:
            return Color{42, 108, 62, 255};
        case MapCellKind::kWater:
            return Color{42, 86, 142, 255};
        case MapCellKind::kRoad:
            return Color{176, 151, 92, 255};
        case MapCellKind::kSwamp:
            return Color{68, 98, 78, 255};
        case MapCellKind::kRuins:
            return Color{122, 117, 112, 255};
        case MapCellKind::kBuilding:
            return Color{86, 78, 76, 255};
        case MapCellKind::kBlocked:
            return Color{32, 35, 38, 255};
        case MapCellKind::kStart:
            return Color{248, 232, 88, 255};
        case MapCellKind::kGoal:
            return Color{226, 90, 72, 255};
        case MapCellKind::kUnknown:
            return Color{106, 108, 104, 255};
    }
    return Color{106, 108, 104, 255};
}

[[nodiscard]] bool ApproximatelyEqual(float lhs, float rhs)
{
    return std::fabs(lhs - rhs) <= kViewportChangeEpsilon;
}

[[nodiscard]] bool PointInRectangle(Vector2 point, Rectangle rectangle)
{
    return point.x >= rectangle.x && point.y >= rectangle.y
        && point.x < rectangle.x + rectangle.width
        && point.y < rectangle.y + rectangle.height;
}

}  // namespace

Map2DView::~Map2DView()
{
    Unload();
}

bool Map2DView::Load(const MapOverview& overview)
{
    Unload();
    if (!overview.IsValid()) {
        return false;
    }

    std::vector<Color> pixels;
    pixels.reserve(overview.cells.size());
    for (const MapCellKind cell : overview.cells) {
        pixels.push_back(CellColor(cell));
    }

    Image image{};
    image.data = pixels.data();
    image.width = overview.width;
    image.height = overview.height;
    image.mipmaps = 1;
    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    terrain_texture_ = LoadTextureFromImage(image);
    if (terrain_texture_.id == 0) {
        terrain_texture_ = Texture2D{};
        return false;
    }

    SetTextureFilter(terrain_texture_, TEXTURE_FILTER_POINT);
    map_width_ = overview.width;
    map_height_ = overview.height;
    loaded_ = true;
    initialized_ = false;
    return true;
}

void Map2DView::Unload()
{
    if (terrain_texture_.id != 0 && IsWindowReady()) {
        UnloadTexture(terrain_texture_);
    }
    terrain_texture_ = Texture2D{};
    map_width_ = 0;
    map_height_ = 0;
    target_ = Vector2{};
    last_viewport_ = Rectangle{};
    zoom_ = 1.0F;
    fit_zoom_ = 1.0F;
    loaded_ = false;
    initialized_ = false;
    panning_ = false;
    hover_tile_valid_ = false;
    hover_tile_ = TileCoord{};
}

void Map2DView::Update(Rectangle viewport, bool enabled)
{
    if (!loaded_ || !enabled || viewport.width <= 1.0F || viewport.height <= 1.0F) {
        panning_ = false;
        hover_tile_valid_ = false;
        return;
    }

    EnsureViewport(viewport);
    const Vector2 mouse = GetMousePosition();
    const bool mouse_in_viewport = PointInRectangle(mouse, viewport);

    if (mouse_in_viewport) {
        const float wheel = GetMouseWheelMove();
        if (std::fabs(wheel) > 0.0001F) {
            ZoomAt(mouse, wheel, viewport);
        }
    }

    panning_ = mouse_in_viewport && IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
    if (panning_) {
        const Vector2 delta = GetMouseDelta();
        target_.x -= delta.x / std::max(zoom_, 0.0001F);
        target_.y -= delta.y / std::max(zoom_, 0.0001F);
        ClampTarget(viewport);
    }

    const std::optional<TileCoord> hover = ScreenToTile(mouse, viewport);
    hover_tile_valid_ = hover.has_value();
    if (hover.has_value()) {
        hover_tile_ = *hover;
    }
}

void Map2DView::FitToMap(Rectangle viewport)
{
    if (!loaded_ || viewport.width <= 1.0F || viewport.height <= 1.0F) {
        return;
    }

    fit_zoom_ = FitZoom(viewport);
    zoom_ = fit_zoom_;
    target_ = Vector2{
        static_cast<float>(map_width_) * 0.5F,
        static_cast<float>(map_height_) * 0.5F,
    };
    last_viewport_ = viewport;
    initialized_ = true;
    ClampTarget(viewport);
}

void Map2DView::ResetView(Rectangle viewport)
{
    if (!loaded_ || viewport.width <= 1.0F || viewport.height <= 1.0F) {
        return;
    }

    fit_zoom_ = FitZoom(viewport);
    zoom_ = std::clamp(
        std::max(1.0F, fit_zoom_ * kResetFitMultiplier),
        fit_zoom_,
        kMaximumPixelsPerTile);
    target_ = Vector2{
        static_cast<float>(map_width_) * 0.5F,
        static_cast<float>(map_height_) * 0.5F,
    };
    last_viewport_ = viewport;
    initialized_ = true;
    ClampTarget(viewport);
}

void Map2DView::AdjustZoom(int steps, Rectangle viewport)
{
    if (steps == 0 || !loaded_) {
        return;
    }
    EnsureViewport(viewport);
    const Vector2 center{
        viewport.x + viewport.width * 0.5F,
        viewport.y + viewport.height * 0.5F,
    };
    ZoomAt(center, static_cast<float>(steps), viewport);
}

std::optional<TileCoord> Map2DView::ScreenToTile(
    Vector2 screen_point,
    Rectangle viewport) const
{
    if (!loaded_ || !initialized_ || !PointInRectangle(screen_point, viewport)) {
        return std::nullopt;
    }

    const Vector2 world = GetScreenToWorld2D(screen_point, CameraFor(viewport));
    const int tile_x = static_cast<int>(std::floor(world.x));
    const int tile_y = static_cast<int>(std::floor(world.y));
    if (tile_x < 0 || tile_y < 0 || tile_x >= map_width_ || tile_y >= map_height_) {
        return std::nullopt;
    }
    return TileCoord{tile_x, tile_y};
}

void Map2DView::Draw(
    Rectangle viewport,
    bool show_terrain,
    bool show_grid,
    std::optional<TileCoord> selection) const
{
    DrawRectangleRec(viewport, Color{18, 22, 24, 255});
    if (!loaded_ || !initialized_) {
        DrawRectangleLinesEx(viewport, 2.0F, Color{235, 235, 220, 255});
        return;
    }

    BeginScissorMode(
        static_cast<int>(std::floor(viewport.x)),
        static_cast<int>(std::floor(viewport.y)),
        std::max(1, static_cast<int>(std::ceil(viewport.width))),
        std::max(1, static_cast<int>(std::ceil(viewport.height))));
    BeginMode2D(CameraFor(viewport));

    const Rectangle map_bounds{
        0.0F,
        0.0F,
        static_cast<float>(map_width_),
        static_cast<float>(map_height_),
    };
    if (show_terrain && terrain_texture_.id != 0) {
        DrawTexturePro(
            terrain_texture_,
            Rectangle{
                0.0F,
                0.0F,
                static_cast<float>(terrain_texture_.width),
                static_cast<float>(terrain_texture_.height),
            },
            map_bounds,
            Vector2{},
            0.0F,
            WHITE);
    } else {
        DrawRectangleRec(map_bounds, Color{64, 66, 64, 255});
    }

    if (show_grid && zoom_ >= kMajorGridThreshold) {
        const int grid_step = zoom_ >= kTileGridThreshold ? 1 : kMajorGridStep;
        const float line_width = std::max(0.5F / zoom_, 0.015625F);
        const Color grid_color = grid_step == 1
            ? Color{20, 24, 24, 75}
            : Color{20, 24, 24, 150};
        const Camera2D camera = CameraFor(viewport);
        const Vector2 visible_min = GetScreenToWorld2D(
            Vector2{viewport.x, viewport.y},
            camera);
        const Vector2 visible_max = GetScreenToWorld2D(
            Vector2{viewport.x + viewport.width, viewport.y + viewport.height},
            camera);
        const int first_x = std::max(
            grid_step,
            static_cast<int>(std::floor(visible_min.x / static_cast<float>(grid_step))) * grid_step);
        const int last_x = std::min(
            map_width_ - 1,
            static_cast<int>(std::ceil(visible_max.x / static_cast<float>(grid_step))) * grid_step);
        const int first_y = std::max(
            grid_step,
            static_cast<int>(std::floor(visible_min.y / static_cast<float>(grid_step))) * grid_step);
        const int last_y = std::min(
            map_height_ - 1,
            static_cast<int>(std::ceil(visible_max.y / static_cast<float>(grid_step))) * grid_step);
        for (int x = first_x; x <= last_x; x += grid_step) {
            const float world_x = static_cast<float>(x);
            DrawLineEx(
                Vector2{world_x, 0.0F},
                Vector2{world_x, static_cast<float>(map_height_)},
                line_width,
                grid_color);
        }
        for (int y = first_y; y <= last_y; y += grid_step) {
            const float world_y = static_cast<float>(y);
            DrawLineEx(
                Vector2{0.0F, world_y},
                Vector2{static_cast<float>(map_width_), world_y},
                line_width,
                grid_color);
        }
    }

    if (selection.has_value()
        && selection->x >= 0 && selection->y >= 0
        && selection->x < map_width_ && selection->y < map_height_) {
        const float selection_line_width = std::max(2.0F / zoom_, 0.03125F);
        DrawRectangleLinesEx(
            Rectangle{
                static_cast<float>(selection->x),
                static_cast<float>(selection->y),
                1.0F,
                1.0F,
            },
            selection_line_width,
            Color{255, 220, 64, 255});
    }

    const float border_width = std::max(1.0F / zoom_, 0.015625F);
    DrawRectangleLinesEx(map_bounds, border_width, Color{235, 235, 220, 255});

    EndMode2D();
    EndScissorMode();
    DrawRectangleLinesEx(viewport, 2.0F, Color{235, 235, 220, 255});
}

bool Map2DView::IsLoaded() const
{
    return loaded_;
}

Map2DViewStatus Map2DView::Status() const
{
    return Map2DViewStatus{
        loaded_,
        initialized_,
        panning_,
        hover_tile_valid_,
        hover_tile_,
        target_,
        zoom_,
        fit_zoom_,
    };
}

Camera2D Map2DView::CameraFor(Rectangle viewport) const
{
    Camera2D camera{};
    camera.offset = Vector2{
        viewport.x + viewport.width * 0.5F,
        viewport.y + viewport.height * 0.5F,
    };
    camera.target = target_;
    camera.rotation = 0.0F;
    camera.zoom = zoom_;
    return camera;
}

float Map2DView::FitZoom(Rectangle viewport) const
{
    if (map_width_ <= 0 || map_height_ <= 0) {
        return 1.0F;
    }

    const float usable_width = std::max(1.0F, viewport.width - kViewportPadding * 2.0F);
    const float usable_height = std::max(1.0F, viewport.height - kViewportPadding * 2.0F);
    return std::clamp(
        std::min(
            usable_width / static_cast<float>(map_width_),
            usable_height / static_cast<float>(map_height_)),
        0.01F,
        kMaximumPixelsPerTile);
}

void Map2DView::EnsureViewport(Rectangle viewport)
{
    if (!loaded_) {
        return;
    }
    if (!initialized_) {
        FitToMap(viewport);
        return;
    }

    const bool viewport_changed = !ApproximatelyEqual(last_viewport_.width, viewport.width)
        || !ApproximatelyEqual(last_viewport_.height, viewport.height)
        || !ApproximatelyEqual(last_viewport_.x, viewport.x)
        || !ApproximatelyEqual(last_viewport_.y, viewport.y);
    if (!viewport_changed) {
        return;
    }

    const float old_fit_zoom = fit_zoom_;
    const bool was_fitted = std::fabs(zoom_ - old_fit_zoom) <= std::max(0.001F, old_fit_zoom * 0.01F);
    fit_zoom_ = FitZoom(viewport);
    last_viewport_ = viewport;
    if (was_fitted) {
        zoom_ = fit_zoom_;
        target_ = Vector2{
            static_cast<float>(map_width_) * 0.5F,
            static_cast<float>(map_height_) * 0.5F,
        };
    } else {
        zoom_ = std::clamp(zoom_, fit_zoom_, kMaximumPixelsPerTile);
    }
    ClampTarget(viewport);
}

void Map2DView::ZoomAt(Vector2 screen_point, float steps, Rectangle viewport)
{
    if (!loaded_ || std::fabs(steps) <= 0.0001F) {
        return;
    }

    EnsureViewport(viewport);
    const Camera2D before_camera = CameraFor(viewport);
    const Vector2 world_before = GetScreenToWorld2D(screen_point, before_camera);
    const float factor = std::pow(kZoomStep, steps);
    zoom_ = std::clamp(zoom_ * factor, fit_zoom_, kMaximumPixelsPerTile);
    const Camera2D after_camera = CameraFor(viewport);
    const Vector2 world_after = GetScreenToWorld2D(screen_point, after_camera);
    target_.x += world_before.x - world_after.x;
    target_.y += world_before.y - world_after.y;
    ClampTarget(viewport);
}

void Map2DView::ClampTarget(Rectangle viewport)
{
    if (!loaded_ || zoom_ <= 0.0F) {
        return;
    }

    const float half_visible_width = viewport.width * 0.5F / zoom_;
    const float half_visible_height = viewport.height * 0.5F / zoom_;
    if (static_cast<float>(map_width_) <= half_visible_width * 2.0F) {
        target_.x = static_cast<float>(map_width_) * 0.5F;
    } else {
        target_.x = std::clamp(
            target_.x,
            half_visible_width,
            static_cast<float>(map_width_) - half_visible_width);
    }
    if (static_cast<float>(map_height_) <= half_visible_height * 2.0F) {
        target_.y = static_cast<float>(map_height_) * 0.5F;
    } else {
        target_.y = std::clamp(
            target_.y,
            half_visible_height,
            static_cast<float>(map_height_) - half_visible_height);
    }
}

}  // namespace vox3d
