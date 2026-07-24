#include "vox3d/render_raylib/map_2d_view.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace vox3d {
namespace {

constexpr float kViewportPadding = 8.0F;
constexpr float kZoomStep = 1.20F;
constexpr float kResetFitMultiplier = 2.0F;
constexpr float kMaximumPixelsPerTile = 64.0F;
constexpr float kViewportChangeEpsilon = 0.5F;
constexpr float kTileGridThreshold = 6.0F;
constexpr float kFocusPixelsPerTile = 12.0F;
constexpr float kPartitionLabelFontPixels = 11.0F;
constexpr float kPartitionLabelSpacingPixels = 1.0F;
constexpr float kChunkLabelMinimumWidthPixels = 52.0F;
constexpr float kRegionLabelMinimumWidthPixels = 80.0F;
constexpr float kObjectFootprintThreshold = 7.0F;
constexpr float kObjectCollisionThreshold = 11.0F;
constexpr float kObjectOrientationThreshold = 13.0F;
constexpr float kObjectLabelThreshold = 22.0F;
constexpr float kVegetationLabelThreshold = 22.0F;
constexpr float kPlaceDetailThreshold = 5.0F;
constexpr float kPlaceLabelThreshold = 8.0F;
constexpr float kMarkerLabelThreshold = 10.0F;

struct ElevationColorStop {
    int level = 0;
    Color color{};
};

constexpr std::array<ElevationColorStop, 8> kElevationPalette{{
    ElevationColorStop{-5, Color{24, 52, 91, 255}},
    ElevationColorStop{-2, Color{42, 86, 142, 255}},
    ElevationColorStop{-1, Color{70, 124, 120, 255}},
    ElevationColorStop{0, Color{92, 150, 82, 255}},
    ElevationColorStop{4, Color{136, 182, 93, 255}},
    ElevationColorStop{10, Color{209, 180, 93, 255}},
    ElevationColorStop{16, Color{154, 103, 72, 255}},
    ElevationColorStop{20, Color{221, 216, 203, 255}},
}};

constexpr Color kStructureHeight0{28, 31, 33, 255};
constexpr Color kStructureHeight1{105, 100, 94, 255};
constexpr Color kStructureHeight2{158, 137, 109, 255};
constexpr Color kStructureHeight3{214, 177, 112, 255};
constexpr Color kCollisionFree{62, 145, 86, 255};
constexpr Color kCollisionBlocked{178, 58, 52, 255};
constexpr Color kMovementBlocked{38, 40, 44, 255};
constexpr Color kMovementCost1{62, 145, 86, 255};
constexpr Color kMovementCost2{218, 186, 72, 255};
constexpr Color kMovementCost3{222, 116, 58, 255};
constexpr Color kMovementCostHigh{180, 55, 70, 255};
constexpr Color kProjectileClear{66, 139, 181, 255};
constexpr Color kProjectileBlocked{193, 69, 97, 255};
constexpr Color kVisionClear{207, 188, 88, 255};
constexpr Color kVisionBlocked{73, 55, 104, 255};
constexpr Color kCoverLow{31, 38, 48, 255};
constexpr Color kCoverHigh{67, 196, 213, 255};
constexpr Color kConcealmentLow{30, 42, 34, 255};
constexpr Color kConcealmentHigh{91, 201, 104, 255};

constexpr std::array<Map2DLegendEntry, 7> kTerrainLegend{{
    Map2DLegendEntry{Color{92, 150, 82, 255}, "Open"},
    Map2DLegendEntry{Color{42, 108, 62, 255}, "Forest"},
    Map2DLegendEntry{Color{42, 86, 142, 255}, "Water"},
    Map2DLegendEntry{Color{176, 151, 92, 255}, "Road"},
    Map2DLegendEntry{Color{68, 98, 78, 255}, "Swamp"},
    Map2DLegendEntry{Color{122, 117, 112, 255}, "Ruins"},
    Map2DLegendEntry{Color{32, 35, 38, 255}, "Blocked"},
}};

constexpr std::array<Map2DLegendEntry, 8> kElevationLegend{{
    Map2DLegendEntry{kElevationPalette[0].color, "-5 deep"},
    Map2DLegendEntry{kElevationPalette[1].color, "-2 water"},
    Map2DLegendEntry{kElevationPalette[2].color, "-1 lowland"},
    Map2DLegendEntry{kElevationPalette[3].color, "0 ground"},
    Map2DLegendEntry{kElevationPalette[4].color, "4 low"},
    Map2DLegendEntry{kElevationPalette[5].color, "10 high"},
    Map2DLegendEntry{kElevationPalette[6].color, "16 mountain"},
    Map2DLegendEntry{kElevationPalette[7].color, "20 peak"},
}};

constexpr std::array<Map2DLegendEntry, 4> kStructureHeightLegend{{
    Map2DLegendEntry{kStructureHeight0, "No structure"},
    Map2DLegendEntry{kStructureHeight1, "Height 1"},
    Map2DLegendEntry{kStructureHeight2, "Height 2"},
    Map2DLegendEntry{kStructureHeight3, "Height 3"},
}};

constexpr std::array<Map2DLegendEntry, 2> kCollisionLegend{{
    Map2DLegendEntry{kCollisionFree, "Passable"},
    Map2DLegendEntry{kCollisionBlocked, "Blocked"},
}};

constexpr std::array<Map2DLegendEntry, 5> kMovementLegend{{
    Map2DLegendEntry{kMovementBlocked, "Blocked"},
    Map2DLegendEntry{kMovementCost1, "Cost 1"},
    Map2DLegendEntry{kMovementCost2, "Cost 2"},
    Map2DLegendEntry{kMovementCost3, "Cost 3"},
    Map2DLegendEntry{kMovementCostHigh, "Cost 4+"},
}};

constexpr std::array<Map2DLegendEntry, 2> kProjectileLegend{{
    Map2DLegendEntry{kProjectileClear, "Projectile clear"},
    Map2DLegendEntry{kProjectileBlocked, "Projectile blocked"},
}};

constexpr std::array<Map2DLegendEntry, 2> kVisionLegend{{
    Map2DLegendEntry{kVisionClear, "Vision clear"},
    Map2DLegendEntry{kVisionBlocked, "Vision blocked"},
}};

constexpr std::array<Map2DLegendEntry, 3> kCoverLegend{{
    Map2DLegendEntry{kCoverLow, "0% cover"},
    Map2DLegendEntry{Color{49, 117, 131, 255}, "50% cover"},
    Map2DLegendEntry{kCoverHigh, "100% cover"},
}};

constexpr std::array<Map2DLegendEntry, 3> kConcealmentLegend{{
    Map2DLegendEntry{kConcealmentLow, "0% concealment"},
    Map2DLegendEntry{Color{61, 122, 69, 255}, "50% concealment"},
    Map2DLegendEntry{kConcealmentHigh, "100% concealment"},
}};

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

[[nodiscard]] std::uint8_t InterpolateChannel(
    std::uint8_t from,
    std::uint8_t to,
    float amount)
{
    const float value = static_cast<float>(from)
        + (static_cast<float>(to) - static_cast<float>(from)) * amount;
    return static_cast<std::uint8_t>(std::clamp(value, 0.0F, 255.0F));
}

[[nodiscard]] Color InterpolateColor(Color from, Color to, float amount)
{
    const float clamped = std::clamp(amount, 0.0F, 1.0F);
    return Color{
        InterpolateChannel(from.r, to.r, clamped),
        InterpolateChannel(from.g, to.g, clamped),
        InterpolateChannel(from.b, to.b, clamped),
        InterpolateChannel(from.a, to.a, clamped),
    };
}

[[nodiscard]] Color ElevationColor(int level)
{
    if (level <= kElevationPalette.front().level) {
        return kElevationPalette.front().color;
    }
    if (level >= kElevationPalette.back().level) {
        return kElevationPalette.back().color;
    }

    for (std::size_t index = 1; index < kElevationPalette.size(); ++index) {
        const ElevationColorStop& upper = kElevationPalette[index];
        if (level > upper.level) {
            continue;
        }
        const ElevationColorStop& lower = kElevationPalette[index - 1];
        const float span = static_cast<float>(upper.level - lower.level);
        const float amount = static_cast<float>(level - lower.level) / span;
        return InterpolateColor(lower.color, upper.color, amount);
    }
    return kElevationPalette.back().color;
}

[[nodiscard]] Color StructureHeightColor(std::uint8_t height)
{
    switch (height) {
        case 0U:
            return kStructureHeight0;
        case 1U:
            return kStructureHeight1;
        case 2U:
            return kStructureHeight2;
        case 3U:
            return kStructureHeight3;
        default:
            return Color{220, 60, 80, 255};
    }
}

[[nodiscard]] Color CollisionColor(std::uint8_t collision)
{
    return collision == 0 ? kCollisionFree : kCollisionBlocked;
}

[[nodiscard]] Color MovementCostColor(int movement_cost)
{
    if (movement_cost < 0) {
        return kMovementBlocked;
    }
    switch (movement_cost) {
        case 0:
        case 1:
            return kMovementCost1;
        case 2:
            return kMovementCost2;
        case 3:
            return kMovementCost3;
        default:
            return kMovementCostHigh;
    }
}

[[nodiscard]] Color ProjectileBlockColor(std::uint8_t blocked)
{
    return blocked == 0 ? kProjectileClear : kProjectileBlocked;
}

[[nodiscard]] Color VisionBlockColor(std::uint8_t blocked)
{
    return blocked == 0 ? kVisionClear : kVisionBlocked;
}

[[nodiscard]] Color ScalarGridColor(
    std::uint8_t value,
    Color low,
    Color high)
{
    return InterpolateColor(low, high, static_cast<float>(value) / 255.0F);
}

[[nodiscard]] Texture2D UploadDiagnosticTexture(
    std::vector<Color>& pixels,
    int width,
    int height)
{
    Image image{};
    image.data = pixels.data();
    image.width = width;
    image.height = height;
    image.mipmaps = 1;
    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    Texture2D texture = LoadTextureFromImage(image);
    if (texture.id != 0) {
        SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    }
    return texture;
}

void UnloadTextureIfLoaded(Texture2D* texture)
{
    if (texture == nullptr) {
        return;
    }
    if (texture->id != 0 && IsWindowReady()) {
        UnloadTexture(*texture);
    }
    *texture = Texture2D{};
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

[[nodiscard]] bool TileInsideMap(TileCoord tile, int map_width, int map_height)
{
    return tile.x >= 0 && tile.y >= 0
        && tile.x < map_width && tile.y < map_height;
}

void DrawPartitionOverlay(
    int map_width,
    int map_height,
    int step_x,
    int step_y,
    Rectangle visible_world,
    float zoom,
    Color line_color,
    float line_width_pixels,
    std::string_view label_prefix,
    float label_minimum_width_pixels)
{
    if (step_x <= 0 || step_y <= 0 || map_width <= 0 || map_height <= 0) {
        return;
    }

    const int first_line_x = std::max(
        step_x,
        static_cast<int>(std::floor(visible_world.x / step_x)) * step_x);
    const int last_line_x = std::min(
        map_width - 1,
        static_cast<int>(std::ceil(
            (visible_world.x + visible_world.width) / step_x)) * step_x);
    const int first_line_y = std::max(
        step_y,
        static_cast<int>(std::floor(visible_world.y / step_y)) * step_y);
    const int last_line_y = std::min(
        map_height - 1,
        static_cast<int>(std::ceil(
            (visible_world.y + visible_world.height) / step_y)) * step_y);
    const float line_width = std::max(line_width_pixels / zoom, 0.015625F);
    for (int x = first_line_x; x <= last_line_x; x += step_x) {
        DrawLineEx(
            Vector2{static_cast<float>(x), 0.0F},
            Vector2{static_cast<float>(x), static_cast<float>(map_height)},
            line_width,
            line_color);
    }
    for (int y = first_line_y; y <= last_line_y; y += step_y) {
        DrawLineEx(
            Vector2{0.0F, static_cast<float>(y)},
            Vector2{static_cast<float>(map_width), static_cast<float>(y)},
            line_width,
            line_color);
    }

    const float cell_pixels = static_cast<float>(std::min(step_x, step_y)) * zoom;
    if (cell_pixels < label_minimum_width_pixels) {
        return;
    }

    const int cells_x = (map_width + step_x - 1) / step_x;
    const int cells_y = (map_height + step_y - 1) / step_y;
    const int first_cell_x = std::clamp(
        static_cast<int>(std::floor(visible_world.x / step_x)),
        0,
        cells_x - 1);
    const int last_cell_x = std::clamp(
        static_cast<int>(std::floor(
            (visible_world.x + visible_world.width) / step_x)),
        0,
        cells_x - 1);
    const int first_cell_y = std::clamp(
        static_cast<int>(std::floor(visible_world.y / step_y)),
        0,
        cells_y - 1);
    const int last_cell_y = std::clamp(
        static_cast<int>(std::floor(
            (visible_world.y + visible_world.height) / step_y)),
        0,
        cells_y - 1);
    const float font_size = kPartitionLabelFontPixels / zoom;
    const float spacing = kPartitionLabelSpacingPixels / zoom;
    const Vector2 label_padding{3.0F / zoom, 2.0F / zoom};
    const Color label_color{line_color.r, line_color.g, line_color.b, 230};
    for (int cell_y = first_cell_y; cell_y <= last_cell_y; ++cell_y) {
        for (int cell_x = first_cell_x; cell_x <= last_cell_x; ++cell_x) {
            const std::string label = std::string(label_prefix)
                + std::to_string(cell_x) + "," + std::to_string(cell_y);
            DrawTextEx(
                GetFontDefault(),
                label.c_str(),
                Vector2{
                    static_cast<float>(cell_x * step_x) + label_padding.x,
                    static_cast<float>(cell_y * step_y) + label_padding.y,
                },
                font_size,
                spacing,
                label_color);
        }
    }
}

void DrawEndpointMarker(
    TileCoord tile,
    std::string_view label,
    Color fill,
    float zoom)
{
    const Vector2 center{
        static_cast<float>(tile.x) + 0.5F,
        static_cast<float>(tile.y) + 0.5F,
    };
    const float radius = std::clamp(7.0F / zoom, 0.18F, 1.25F);
    const float outline_radius = radius + std::max(1.5F / zoom, 0.03F);
    DrawCircleV(center, outline_radius, Color{18, 22, 24, 240});
    DrawCircleV(center, radius, fill);

    const std::string label_text(label);
    const float font_size = 10.0F / zoom;
    const float spacing = 1.0F / zoom;
    const Vector2 text_size = MeasureTextEx(
        GetFontDefault(),
        label_text.c_str(),
        font_size,
        spacing);
    DrawTextEx(
        GetFontDefault(),
        label_text.c_str(),
        Vector2{center.x - text_size.x * 0.5F, center.y - text_size.y * 0.5F},
        font_size,
        spacing,
        Color{20, 24, 24, 255});
}


[[nodiscard]] bool TileNearVisibleWorld(
    TileCoord tile,
    Rectangle visible_world,
    float margin)
{
    return static_cast<float>(tile.x) + 1.0F >= visible_world.x - margin
        && static_cast<float>(tile.y) + 1.0F >= visible_world.y - margin
        && static_cast<float>(tile.x) <= visible_world.x + visible_world.width + margin
        && static_cast<float>(tile.y) <= visible_world.y + visible_world.height + margin;
}

[[nodiscard]] bool BoundsIntersectVisibleWorld(
    const RuntimeTileBounds& bounds,
    Rectangle visible_world)
{
    if (!bounds.IsValid()) {
        return false;
    }
    return static_cast<float>(bounds.max_x + 1) >= visible_world.x
        && static_cast<float>(bounds.max_y + 1) >= visible_world.y
        && static_cast<float>(bounds.min_x) <= visible_world.x + visible_world.width
        && static_cast<float>(bounds.min_y) <= visible_world.y + visible_world.height;
}

[[nodiscard]] Color ObjectColor(RuntimeObjectMarkerKind kind)
{
    switch (kind) {
        case RuntimeObjectMarkerKind::kTree:
            return Color{52, 142, 72, 255};
        case RuntimeObjectMarkerKind::kBush:
            return Color{92, 164, 75, 255};
        case RuntimeObjectMarkerKind::kReed:
            return Color{154, 177, 74, 255};
        case RuntimeObjectMarkerKind::kRuin:
            return Color{162, 151, 143, 255};
        case RuntimeObjectMarkerKind::kCover:
            return Color{221, 163, 72, 255};
        case RuntimeObjectMarkerKind::kLoot:
            return Color{241, 214, 77, 255};
        case RuntimeObjectMarkerKind::kStructure:
            return Color{103, 177, 213, 255};
        case RuntimeObjectMarkerKind::kTrench:
            return Color{171, 113, 68, 255};
        case RuntimeObjectMarkerKind::kUnknown:
            return Color{202, 103, 196, 255};
    }
    return Color{202, 103, 196, 255};
}

[[nodiscard]] Color MarkerColor(std::string_view type)
{
    if (type == "start") {
        return Color{96, 225, 118, 255};
    }
    if (type == "goal") {
        return Color{238, 91, 78, 255};
    }
    if (type == "loot") {
        return Color{244, 211, 72, 255};
    }
    if (type == "story") {
        return Color{195, 110, 222, 255};
    }
    if (type == "defensive_point") {
        return Color{232, 139, 70, 255};
    }
    if (type == "point_of_interest") {
        return Color{81, 188, 221, 255};
    }
    return Color{214, 214, 206, 255};
}

[[nodiscard]] std::string_view MarkerGlyph(std::string_view type)
{
    if (type == "start") {
        return "S";
    }
    if (type == "goal") {
        return "G";
    }
    if (type == "loot") {
        return "$";
    }
    if (type == "story") {
        return "!";
    }
    if (type == "defensive_point") {
        return "D";
    }
    if (type == "point_of_interest") {
        return "P";
    }
    return "M";
}

void DrawTileFootprint(
    std::span<const TileCoord> tiles,
    Rectangle visible_world,
    float zoom,
    Color fill,
    Color outline)
{
    const float line_width = std::max(1.0F / zoom, 0.02F);
    for (const TileCoord tile : tiles) {
        if (!TileNearVisibleWorld(tile, visible_world, 0.0F)) {
            continue;
        }
        const Rectangle tile_bounds{
            static_cast<float>(tile.x),
            static_cast<float>(tile.y),
            1.0F,
            1.0F,
        };
        if (fill.a > 0) {
            DrawRectangleRec(tile_bounds, fill);
        }
        DrawRectangleLinesEx(tile_bounds, line_width, outline);
    }
}

void DrawObjectOrientation(
    const RuntimeMapObject& object,
    float zoom,
    Color color)
{
    Vector2 direction{};
    if (object.orientation == "east_west") {
        direction = Vector2{1.0F, 0.0F};
    } else if (object.orientation == "north_south"
        || object.orientation == "vertical") {
        direction = Vector2{0.0F, 1.0F};
    } else {
        return;
    }

    const Vector2 center{
        static_cast<float>(object.anchor.x) + 0.5F,
        static_cast<float>(object.anchor.y) + 0.5F,
    };
    const float half_length = std::clamp(7.0F / zoom, 0.3F, 0.75F);
    const float line_width = std::max(1.5F / zoom, 0.025F);
    DrawLineEx(
        Vector2{
            center.x - direction.x * half_length,
            center.y - direction.y * half_length,
        },
        Vector2{
            center.x + direction.x * half_length,
            center.y + direction.y * half_length,
        },
        line_width,
        color);
}

void DrawRuntimeObjectsOverlay(
    std::span<const RuntimeMapObject> objects,
    Rectangle visible_world,
    float zoom)
{
    for (const RuntimeMapObject& object : objects) {
        const bool visible = TileNearVisibleWorld(object.anchor, visible_world, 3.0F)
            || BoundsIntersectVisibleWorld(object.visual_bounds, visible_world);
        if (!visible) {
            continue;
        }

        const Color color = ObjectColor(object.kind);
        if (zoom >= kObjectFootprintThreshold) {
            DrawTileFootprint(
                object.footprint,
                visible_world,
                zoom,
                Color{color.r, color.g, color.b, 48},
                Color{color.r, color.g, color.b, 205});
        }
        if (zoom >= kObjectCollisionThreshold) {
            DrawTileFootprint(
                object.collision_footprint,
                visible_world,
                zoom,
                Color{0, 0, 0, 0},
                Color{235, 74, 70, 220});
        }

        const Vector2 center{
            static_cast<float>(object.anchor.x) + 0.5F,
            static_cast<float>(object.anchor.y) + 0.5F,
        };
        const float radius = std::clamp(4.5F / zoom, 0.16F, 0.72F);
        DrawCircleV(center, radius + std::max(1.0F / zoom, 0.025F), Color{18, 22, 24, 235});
        DrawCircleV(center, radius, color);
        if (object.interactive) {
            DrawCircleLinesV(
                center,
                radius + std::max(2.0F / zoom, 0.05F),
                Color{245, 230, 111, 235});
        }

        if (zoom >= kObjectOrientationThreshold) {
            DrawObjectOrientation(object, zoom, Color{22, 26, 28, 235});
        }
        if (zoom >= kObjectLabelThreshold) {
            const float font_size = 9.0F / zoom;
            const float spacing = 0.5F / zoom;
            DrawTextEx(
                GetFontDefault(),
                object.type.c_str(),
                Vector2{
                    center.x + radius + 2.0F / zoom,
                    center.y - font_size * 0.5F,
                },
                font_size,
                spacing,
                Color{244, 244, 232, 245});
        }
    }
}

void DrawVegetationOverlay(
    std::span<const RuntimeObjectMarker> markers,
    Rectangle visible_world,
    float zoom)
{
    const float marker_size = std::clamp(3.0F / zoom, 0.20F, 0.70F);
    for (const RuntimeObjectMarker& marker : markers) {
        if (!marker.visual_only || marker.role != "vegetation"
            || !TileNearVisibleWorld(marker.tile, visible_world, 0.0F)) {
            continue;
        }

        const Vector2 center{
            static_cast<float>(marker.tile.x) + 0.5F,
            static_cast<float>(marker.tile.y) + 0.5F,
        };
        const Color color = ObjectColor(marker.kind);
        DrawRectangleRec(
            Rectangle{
                center.x - marker_size * 0.5F,
                center.y - marker_size * 0.5F,
                marker_size,
                marker_size,
            },
            Color{color.r, color.g, color.b, 230});

        if (zoom >= kVegetationLabelThreshold) {
            const float font_size = 9.0F / zoom;
            const float spacing = 0.5F / zoom;
            DrawTextEx(
                GetFontDefault(),
                marker.type.c_str(),
                Vector2{
                    center.x + marker_size * 0.5F + 2.0F / zoom,
                    center.y - font_size * 0.5F,
                },
                font_size,
                spacing,
                Color{224, 242, 220, 245});
        }
    }
}

void DrawPlacesOverlay(
    std::span<const RuntimePlace> places,
    Rectangle visible_world,
    float zoom)
{
    const float line_width = std::max(1.5F / zoom, 0.025F);
    for (const RuntimePlace& place : places) {
        const RuntimeTileBounds fallback_bounds{
            place.center.x - place.radius,
            place.center.y - place.radius,
            place.center.x + place.radius,
            place.center.y + place.radius,
        };
        const RuntimeTileBounds& bounds = place.bounds.IsValid()
            ? place.bounds
            : fallback_bounds;
        if (!BoundsIntersectVisibleWorld(bounds, visible_world)
            && !TileNearVisibleWorld(place.center, visible_world, static_cast<float>(place.radius))) {
            continue;
        }

        const Color place_color{74, 202, 218, 220};
        const Vector2 center{
            static_cast<float>(place.center.x) + 0.5F,
            static_cast<float>(place.center.y) + 0.5F,
        };
        if (place.radius > 0) {
            DrawCircleV(
                center,
                static_cast<float>(place.radius),
                Color{place_color.r, place_color.g, place_color.b, 24});
            DrawCircleLinesV(center, static_cast<float>(place.radius), place_color);
        }
        if (zoom >= kPlaceDetailThreshold && bounds.IsValid()) {
            DrawRectangleLinesEx(
                Rectangle{
                    static_cast<float>(bounds.min_x),
                    static_cast<float>(bounds.min_y),
                    static_cast<float>(bounds.max_x - bounds.min_x + 1),
                    static_cast<float>(bounds.max_y - bounds.min_y + 1),
                },
                line_width,
                Color{place_color.r, place_color.g, place_color.b, 180});
            const float entrance_radius = std::clamp(3.5F / zoom, 0.12F, 0.45F);
            for (const RuntimePlaceEntrance& entrance : place.entrances) {
                DrawCircleV(
                    Vector2{
                        static_cast<float>(entrance.tile.x) + 0.5F,
                        static_cast<float>(entrance.tile.y) + 0.5F,
                    },
                    entrance_radius,
                    Color{245, 236, 154, 235});
            }
        }
        DrawCircleV(center, std::clamp(4.0F / zoom, 0.16F, 0.55F), place_color);
        if (zoom >= kPlaceLabelThreshold) {
            const float font_size = 10.0F / zoom;
            const float spacing = 0.5F / zoom;
            DrawTextEx(
                GetFontDefault(),
                place.type.c_str(),
                Vector2{
                    center.x + 4.0F / zoom,
                    center.y - font_size * 0.6F,
                },
                font_size,
                spacing,
                Color{210, 247, 250, 245});
        }
    }
}

void DrawMarkersOverlay(
    std::span<const RuntimeMapMarker> markers,
    Rectangle visible_world,
    float zoom,
    bool endpoints_already_drawn)
{
    for (const RuntimeMapMarker& marker : markers) {
        if (endpoints_already_drawn
            && (marker.type == "start" || marker.type == "goal")) {
            continue;
        }
        if (!TileNearVisibleWorld(marker.tile, visible_world, 1.0F)) {
            continue;
        }

        const Vector2 center{
            static_cast<float>(marker.tile.x) + 0.5F,
            static_cast<float>(marker.tile.y) + 0.5F,
        };
        const Color color = MarkerColor(marker.type);
        const float radius = std::clamp(6.0F / zoom, 0.22F, 0.85F);
        DrawCircleV(center, radius + std::max(1.5F / zoom, 0.035F), Color{18, 22, 24, 240});
        DrawCircleV(center, radius, color);

        const std::string glyph(MarkerGlyph(marker.type));
        const float glyph_size = 9.0F / zoom;
        const float glyph_spacing = 0.5F / zoom;
        const Vector2 glyph_bounds = MeasureTextEx(
            GetFontDefault(),
            glyph.c_str(),
            glyph_size,
            glyph_spacing);
        DrawTextEx(
            GetFontDefault(),
            glyph.c_str(),
            Vector2{
                center.x - glyph_bounds.x * 0.5F,
                center.y - glyph_bounds.y * 0.5F,
            },
            glyph_size,
            glyph_spacing,
            Color{24, 27, 28, 255});

        if (zoom >= kMarkerLabelThreshold) {
            const float font_size = 9.0F / zoom;
            DrawTextEx(
                GetFontDefault(),
                marker.type.c_str(),
                Vector2{
                    center.x + radius + 2.0F / zoom,
                    center.y - font_size * 0.5F,
                },
                font_size,
                0.5F / zoom,
                Color{246, 243, 226, 245});
        }
    }
}

}  // namespace

std::span<const Map2DLegendEntry> Map2DLegendFor(
    Map2DBaseLayer base_layer)
{
    switch (base_layer) {
        case Map2DBaseLayer::kTerrain:
            return kTerrainLegend;
        case Map2DBaseLayer::kElevation:
            return kElevationLegend;
        case Map2DBaseLayer::kStructureHeight:
            return kStructureHeightLegend;
        case Map2DBaseLayer::kCollision:
            return kCollisionLegend;
        case Map2DBaseLayer::kMovementCost:
            return kMovementLegend;
        case Map2DBaseLayer::kProjectileBlock:
            return kProjectileLegend;
        case Map2DBaseLayer::kVisionBlock:
            return kVisionLegend;
        case Map2DBaseLayer::kCover:
            return kCoverLegend;
        case Map2DBaseLayer::kConcealment:
            return kConcealmentLegend;
    }
    return {};
}

Map2DView::~Map2DView()
{
    Unload();
}

bool Map2DView::Load(const RuntimeMap& runtime_map)
{
    Unload();
    if (!runtime_map.overview.IsValid()) {
        last_load_error_ = "overview_missing";
        return false;
    }
    if (!runtime_map.height.IsValid()) {
        last_load_error_ = "elevation_grid_invalid";
        return false;
    }
    if (!runtime_map.collision.IsValid()) {
        last_load_error_ = "collision_grid_invalid";
        return false;
    }

    const int width = runtime_map.overview.width;
    const int height = runtime_map.overview.height;
    const auto dimensions_match = [width, height](const auto& grid) {
        return !grid.IsValid() || (grid.width == width && grid.height == height);
    };
    if (!dimensions_match(runtime_map.height)
        || !dimensions_match(runtime_map.collision)
        || !dimensions_match(runtime_map.structure_height)
        || !dimensions_match(runtime_map.movement_cost)
        || !dimensions_match(runtime_map.projectile_block)
        || !dimensions_match(runtime_map.vision_block)
        || !dimensions_match(runtime_map.cover)
        || !dimensions_match(runtime_map.concealment)) {
        last_load_error_ = "layer_dimensions_mismatch";
        return false;
    }

    const auto upload_colors = [width, height](const auto& cells, auto color_for) {
        std::vector<Color> pixels;
        pixels.reserve(cells.size());
        for (const auto& value : cells) {
            pixels.push_back(color_for(value));
        }
        return UploadDiagnosticTexture(pixels, width, height);
    };

    terrain_texture_ = upload_colors(runtime_map.overview.cells, CellColor);
    if (terrain_texture_.id == 0) {
        last_load_error_ = "terrain_texture_upload_failed";
        return false;
    }
    elevation_texture_ = upload_colors(runtime_map.height.cells, ElevationColor);
    if (elevation_texture_.id == 0) {
        Unload();
        last_load_error_ = "elevation_texture_upload_failed";
        return false;
    }
    if (runtime_map.info.structure_height_loaded && runtime_map.structure_height.IsValid()) {
        structure_height_texture_ = upload_colors(
            runtime_map.structure_height.cells,
            StructureHeightColor);
    }
    collision_texture_ = upload_colors(runtime_map.collision.cells, CollisionColor);
    if (collision_texture_.id == 0) {
        Unload();
        last_load_error_ = "collision_texture_upload_failed";
        return false;
    }

    if (runtime_map.movement_cost.IsValid()) {
        movement_cost_texture_ = upload_colors(
            runtime_map.movement_cost.cells,
            MovementCostColor);
    }
    if (runtime_map.projectile_block.IsValid()) {
        projectile_block_texture_ = upload_colors(
            runtime_map.projectile_block.cells,
            ProjectileBlockColor);
    }
    if (runtime_map.vision_block.IsValid()) {
        vision_block_texture_ = upload_colors(
            runtime_map.vision_block.cells,
            VisionBlockColor);
    }
    if (runtime_map.cover.IsValid()) {
        cover_texture_ = upload_colors(runtime_map.cover.cells, [](std::uint8_t value) {
            return ScalarGridColor(value, kCoverLow, kCoverHigh);
        });
    }
    if (runtime_map.concealment.IsValid()) {
        concealment_texture_ = upload_colors(runtime_map.concealment.cells, [](std::uint8_t value) {
            return ScalarGridColor(value, kConcealmentLow, kConcealmentHigh);
        });
    }

    map_width_ = width;
    map_height_ = height;
    loaded_ = true;
    initialized_ = false;
    last_load_error_.clear();
    return true;
}

bool Map2DView::Load(const MapOverview& overview)
{
    Unload();
    if (!overview.IsValid()) {
        last_load_error_ = "overview_invalid";
        return false;
    }

    std::vector<Color> pixels;
    pixels.reserve(overview.cells.size());
    for (const MapCellKind cell : overview.cells) {
        pixels.push_back(CellColor(cell));
    }

    terrain_texture_ = UploadDiagnosticTexture(
        pixels,
        overview.width,
        overview.height);
    if (terrain_texture_.id == 0) {
        terrain_texture_ = Texture2D{};
        last_load_error_ = "terrain_texture_upload_failed";
        return false;
    }

    map_width_ = overview.width;
    map_height_ = overview.height;
    loaded_ = true;
    initialized_ = false;
    last_load_error_.clear();
    return true;
}

void Map2DView::Unload()
{
    UnloadTextureIfLoaded(&terrain_texture_);
    UnloadTextureIfLoaded(&elevation_texture_);
    UnloadTextureIfLoaded(&structure_height_texture_);
    UnloadTextureIfLoaded(&collision_texture_);
    UnloadTextureIfLoaded(&movement_cost_texture_);
    UnloadTextureIfLoaded(&projectile_block_texture_);
    UnloadTextureIfLoaded(&vision_block_texture_);
    UnloadTextureIfLoaded(&cover_texture_);
    UnloadTextureIfLoaded(&concealment_texture_);
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
    last_load_error_.clear();
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

    panning_ = mouse_in_viewport && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
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

bool Map2DView::FocusTile(TileCoord tile, Rectangle viewport)
{
    if (!loaded_ || !TileInsideMap(tile, map_width_, map_height_)) {
        return false;
    }

    EnsureViewport(viewport);
    zoom_ = std::clamp(
        std::max(zoom_, kFocusPixelsPerTile),
        fit_zoom_,
        kMaximumPixelsPerTile);
    target_ = Vector2{
        static_cast<float>(tile.x) + 0.5F,
        static_cast<float>(tile.y) + 0.5F,
    };
    ClampTarget(viewport);
    return true;
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
    Map2DBaseLayer base_layer,
    const Map2DOverlayOptions& overlays) const
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
    const Texture2D* texture = TextureFor(base_layer);
    if (texture != nullptr && texture->id != 0) {
        DrawTexturePro(
            *texture,
            Rectangle{
                0.0F,
                0.0F,
                static_cast<float>(texture->width),
                static_cast<float>(texture->height),
            },
            map_bounds,
            Vector2{},
            0.0F,
            WHITE);
    } else {
        DrawRectangleRec(map_bounds, Color{64, 66, 64, 255});
    }

    const Camera2D camera = CameraFor(viewport);
    const Vector2 visible_min = GetScreenToWorld2D(
        Vector2{viewport.x, viewport.y},
        camera);
    const Vector2 visible_max = GetScreenToWorld2D(
        Vector2{viewport.x + viewport.width, viewport.y + viewport.height},
        camera);
    const Rectangle visible_world{
        visible_min.x,
        visible_min.y,
        visible_max.x - visible_min.x,
        visible_max.y - visible_min.y,
    };

    if (overlays.show_grid && zoom_ >= kTileGridThreshold) {
        const float line_width = std::max(0.5F / zoom_, 0.015625F);
        const Color grid_color{20, 24, 24, 75};
        const int first_x = std::max(
            1,
            static_cast<int>(std::floor(visible_min.x)));
        const int last_x = std::min(
            map_width_ - 1,
            static_cast<int>(std::ceil(visible_max.x)));
        const int first_y = std::max(
            1,
            static_cast<int>(std::floor(visible_min.y)));
        const int last_y = std::min(
            map_height_ - 1,
            static_cast<int>(std::ceil(visible_max.y)));
        for (int x = first_x; x <= last_x; ++x) {
            const float world_x = static_cast<float>(x);
            DrawLineEx(
                Vector2{world_x, 0.0F},
                Vector2{world_x, static_cast<float>(map_height_)},
                line_width,
                grid_color);
        }
        for (int y = first_y; y <= last_y; ++y) {
            const float world_y = static_cast<float>(y);
            DrawLineEx(
                Vector2{0.0F, world_y},
                Vector2{static_cast<float>(map_width_), world_y},
                line_width,
                grid_color);
        }
    }

    if (overlays.show_vxmap_regions && overlays.vxmap_region_size_tiles > 0) {
        DrawPartitionOverlay(
            map_width_,
            map_height_,
            overlays.vxmap_region_size_tiles,
            overlays.vxmap_region_size_tiles,
            visible_world,
            zoom_,
            Color{64, 215, 225, 230},
            2.5F,
            "R ",
            kRegionLabelMinimumWidthPixels);
    }
    if (overlays.show_chunks && overlays.chunk_size_x > 0 && overlays.chunk_size_y > 0) {
        DrawPartitionOverlay(
            map_width_,
            map_height_,
            overlays.chunk_size_x,
            overlays.chunk_size_y,
            visible_world,
            zoom_,
            Color{242, 174, 70, 215},
            1.5F,
            "C ",
            kChunkLabelMinimumWidthPixels);
    }

    if (overlays.show_places) {
        DrawPlacesOverlay(overlays.places, visible_world, zoom_);
    }
    if (overlays.show_vegetation) {
        DrawVegetationOverlay(
            overlays.vegetation_markers,
            visible_world,
            zoom_);
    }
    if (overlays.show_objects) {
        DrawRuntimeObjectsOverlay(overlays.objects, visible_world, zoom_);
    }
    if (overlays.show_markers) {
        DrawMarkersOverlay(
            overlays.markers,
            visible_world,
            zoom_,
            overlays.show_start_goal);
    }

    if (overlays.show_start_goal) {
        if (overlays.start.has_value()
            && TileInsideMap(*overlays.start, map_width_, map_height_)) {
            DrawEndpointMarker(
                *overlays.start,
                "S",
                Color{96, 225, 118, 255},
                zoom_);
        }
        if (overlays.goal.has_value()
            && TileInsideMap(*overlays.goal, map_width_, map_height_)) {
            DrawEndpointMarker(
                *overlays.goal,
                "G",
                Color{238, 91, 78, 255},
                zoom_);
        }
    }

    if (overlays.selection.has_value()
        && TileInsideMap(*overlays.selection, map_width_, map_height_)) {
        const float selection_line_width = std::max(2.0F / zoom_, 0.03125F);
        DrawRectangleLinesEx(
            Rectangle{
                static_cast<float>(overlays.selection->x),
                static_cast<float>(overlays.selection->y),
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

bool Map2DView::IsLayerLoaded(Map2DBaseLayer base_layer) const
{
    const Texture2D* texture = TextureFor(base_layer);
    return texture != nullptr && texture->id != 0;
}

std::string_view Map2DView::LastLoadError() const
{
    return last_load_error_;
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

const Texture2D* Map2DView::TextureFor(Map2DBaseLayer base_layer) const
{
    switch (base_layer) {
        case Map2DBaseLayer::kTerrain:
            return &terrain_texture_;
        case Map2DBaseLayer::kElevation:
            return &elevation_texture_;
        case Map2DBaseLayer::kStructureHeight:
            return &structure_height_texture_;
        case Map2DBaseLayer::kCollision:
            return &collision_texture_;
        case Map2DBaseLayer::kMovementCost:
            return &movement_cost_texture_;
        case Map2DBaseLayer::kProjectileBlock:
            return &projectile_block_texture_;
        case Map2DBaseLayer::kVisionBlock:
            return &vision_block_texture_;
        case Map2DBaseLayer::kCover:
            return &cover_texture_;
        case Map2DBaseLayer::kConcealment:
            return &concealment_texture_;
    }
    return nullptr;
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
