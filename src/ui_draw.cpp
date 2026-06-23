#include "ui_draw.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace vox3d {
namespace {

constexpr Color kBackground{18, 20, 28, 255};
constexpr Color kPanel{28, 32, 44, 235};
constexpr Color kPanelBorder{180, 184, 196, 255};
constexpr Color kText{214, 218, 228, 255};
constexpr Color kSelectedText{255, 242, 172, 255};
constexpr Color kDisabledText{96, 100, 112, 255};
constexpr Color kMutedText{144, 150, 164, 255};
constexpr Color kModalDim{0, 0, 0, 150};
constexpr Color kAccent{255, 205, 96, 255};

constexpr Color kEditorBackground{7, 118, 151, 255};
constexpr Color kEditorViewport{152, 152, 149, 255};
constexpr Color kEditorViewportText{244, 244, 238, 255};
constexpr Color kEditorPanelText{155, 203, 218, 255};
constexpr Color kEditorStatus{130, 198, 211, 255};
constexpr Color kEditorStatusText{6, 24, 32, 255};
constexpr Color kEditorBorder{8, 12, 16, 255};

constexpr float kBaseTitleFontSize = 30.0F;
constexpr float kBaseSubtitleFontSize = 16.0F;
constexpr float kBaseMenuFontSize = 22.0F;
constexpr float kBasePlaceholderTitleFontSize = 28.0F;
constexpr float kBasePlaceholderHintFontSize = 18.0F;
constexpr float kBaseFpsFontSize = 16.0F;
constexpr float kBaseDebugFontSize = 13.0F;
constexpr float kBaseDialogTitleFontSize = 24.0F;
constexpr float kBaseDialogTextFontSize = 18.0F;
constexpr float kBaseDialogButtonFontSize = 18.0F;
constexpr float kBaseWorkspaceToolFontSize = 18.0F;
constexpr float kBaseWorkspaceStatusFontSize = 16.0F;

[[nodiscard]] float Scaled(float base, float scale, float minimum, float maximum)
{
    return std::clamp(std::round(base * scale), minimum, maximum);
}

[[nodiscard]] float FontSpacing(float font_size)
{
    return std::max(1.0F, std::round(font_size * 0.12F));
}

[[nodiscard]] Vector2 Measure(Font font, const std::string& text, float font_size, float spacing)
{
    return MeasureTextEx(font, text.c_str(), font_size, spacing);
}

void DrawTextCentered(Font font, const std::string& text, float y, float font_size, float spacing, Color color, int window_width)
{
    const Vector2 size = Measure(font, text, font_size, spacing);
    const float x = (static_cast<float>(window_width) - size.x) * 0.5F;
    DrawTextEx(font, text.c_str(), Vector2{x, y}, font_size, spacing, color);
}


[[nodiscard]] float FitTextToWidth(Font font, const std::string& text, float preferred_size, float min_size, float max_width)
{
    float font_size = preferred_size;
    while (font_size > min_size) {
        const float spacing = FontSpacing(font_size);
        if (Measure(font, text, font_size, spacing).x <= max_width) {
            return font_size;
        }
        font_size -= 1.0F;
    }
    return min_size;
}

void PushWordWrappedLine(std::vector<std::string>& lines, std::string& current)
{
    if (!current.empty()) {
        lines.push_back(current);
        current.clear();
    }
}

[[nodiscard]] std::vector<std::string> WrapText(Font font, const std::string& text, float font_size, float spacing, float max_width)
{
    std::vector<std::string> lines;
    std::istringstream input(text);
    std::string word;
    std::string current;

    while (input >> word) {
        const std::string candidate = current.empty() ? word : current + ' ' + word;
        if (Measure(font, candidate, font_size, spacing).x <= max_width) {
            current = candidate;
            continue;
        }

        PushWordWrappedLine(lines, current);
        if (Measure(font, word, font_size, spacing).x <= max_width) {
            current = word;
            continue;
        }

        std::string chunk;
        for (const char ch : word) {
            const std::string candidate_chunk = chunk + ch;
            if (!chunk.empty() && Measure(font, candidate_chunk, font_size, spacing).x > max_width) {
                lines.push_back(chunk);
                chunk.clear();
            }
            chunk.push_back(ch);
        }
        current = chunk;
    }

    PushWordWrappedLine(lines, current);
    if (lines.empty()) {
        lines.push_back("");
    }
    return lines;
}



[[nodiscard]] std::string CompactFloat(float value)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << value;
    return out.str();
}

[[nodiscard]] std::string CompactPercent(double ratio)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << ratio * 100.0 << '%';
    return out.str();
}

[[nodiscard]] std::string CompactSignedPercent(double ratio)
{
    std::ostringstream out;
    if (ratio > 0.0) {
        out << '+';
    }
    out << std::fixed << std::setprecision(1) << ratio * 100.0 << '%';
    return out.str();
}

[[nodiscard]] std::string MeshModeLabel(ChunkMeshBuildMode mode)
{
    switch (mode) {
        case ChunkMeshBuildMode::kSimpleFaces:
            return "simple";
        case ChunkMeshBuildMode::kGreedyFaces:
            return "greedy";
        case ChunkMeshBuildMode::kTerrainSurface:
            return "terrain";
    }
    return "unknown";
}

[[nodiscard]] std::string CompactMeshStats(const WorkspaceState& workspace_state)
{
    const MeshOptimizationStats& stats = workspace_state.mesh_stats;
    return "mesh=" + MeshModeLabel(stats.active_mode) + " chunk=" + std::to_string(workspace_state.chunk_size_tiles)
        + " faces=" + std::to_string(stats.active_faces) + " models=" + std::to_string(stats.draw_models)
        + " saved=" + CompactPercent(stats.ActiveReductionRatio());
}

[[nodiscard]] std::string CompactVector(Vector3 value)
{
    return CompactFloat(value.x) + "," + CompactFloat(value.y) + "," + CompactFloat(value.z);
}

[[nodiscard]] std::string WorkspaceToolLabel(WorkspaceTool tool, const UiLabels& labels)
{
    switch (tool) {
        case WorkspaceTool::kMode:
            return "Mode";
        case WorkspaceTool::kMap2D:
            return "2D Map";
        case WorkspaceTool::kWorld3D:
            return "3D World";
        case WorkspaceTool::kSelection:
            return "Selection";
        case WorkspaceTool::kPackageData:
            return "Package Data";
        case WorkspaceTool::kDebug:
            return labels.workspace_tool_debug;
        case WorkspaceTool::kSettings:
            return labels.workspace_tool_settings;
    }
    return labels.debug_none;
}

[[nodiscard]] std::string MapStatusLabel(const MapPackageInfo& map, const UiLabels& labels)
{
    if (!map.configured) {
        return labels.workspace_status_map_not_configured;
    }
    if (!map.exists || !map.is_directory) {
        return labels.workspace_status_map_missing;
    }
    if (!map.metadata_available) {
        return labels.workspace_status_metadata_unavailable;
    }
    return labels.workspace_status_map_loaded;
}

[[nodiscard]] std::string MapSizeText(const MapPackageInfo& map, const UiLabels& labels)
{
    if (map.width.has_value() && map.height.has_value()) {
        return std::to_string(*map.width) + "x" + std::to_string(*map.height);
    }
    return labels.workspace_map_size_unknown;
}

[[nodiscard]] std::string MapLevelsText(const MapPackageInfo& map, const UiLabels& labels)
{
    if (map.min_level.has_value() && map.max_level.has_value()) {
        return std::to_string(*map.min_level) + ".." + std::to_string(*map.max_level);
    }
    return labels.workspace_map_levels_unknown;
}


[[nodiscard]] std::string MapTileText(const MapPackageInfo& map, const UiLabels& labels)
{
    if (map.tile_size.has_value()) {
        return std::to_string(*map.tile_size);
    }
    return labels.workspace_map_tile_unknown;
}

[[nodiscard]] const std::string& BoolText(bool value, const UiLabels& labels)
{
    return value ? labels.workspace_yes : labels.workspace_no;
}


[[nodiscard]] std::string WorkspacePanelItemLabel(WorkspacePanelItem item, const UiLabels& labels)
{
    switch (item) {
        case WorkspacePanelItem::kMode2DMap:
            return labels.workspace_subitem_2d_map;
        case WorkspacePanelItem::kMode3DWorld:
            return labels.workspace_subitem_3d_preview;
        case WorkspacePanelItem::k2DNavigationGroup:
            return "Navigation";
        case WorkspacePanelItem::k2DFitView:
            return labels.workspace_subitem_fit_view;
        case WorkspacePanelItem::k2DResetView:
            return labels.workspace_subitem_reset_view;
        case WorkspacePanelItem::k2DZoomIn:
            return "Zoom In";
        case WorkspacePanelItem::k2DZoomOut:
            return "Zoom Out";
        case WorkspacePanelItem::k2DBaseLayerGroup:
            return "Base Layer";
        case WorkspacePanelItem::kLayerTerrain:
            return labels.workspace_terrain_label;
        case WorkspacePanelItem::kLayerElevation:
            return labels.workspace_elevation_label;
        case WorkspacePanelItem::kLayerCollision:
            return labels.workspace_collision_label;
        case WorkspacePanelItem::k2DOverlayGroup:
            return "Overlays";
        case WorkspacePanelItem::kLayerGrid:
            return labels.workspace_subitem_grid;
        case WorkspacePanelItem::k2DChunks:
            return "Chunks";
        case WorkspacePanelItem::k2DStartGoal:
            return "Start / Goal";
        case WorkspacePanelItem::k2DObjects:
            return labels.workspace_tool_objects;
        case WorkspacePanelItem::k2DPlaces:
            return "Places";
        case WorkspacePanelItem::k2DMarkers:
            return "Markers";
        case WorkspacePanelItem::k2DRoutes:
            return "Routes";
        case WorkspacePanelItem::k2DWorldGraph:
            return "World Graph";
        case WorkspacePanelItem::k2DGameplayZones:
            return "Gameplay Zones";
        case WorkspacePanelItem::k2DElevationFeatures:
            return "Elevation Features";
        case WorkspacePanelItem::k2DElevationTransitions:
            return "Elevation Transitions";
        case WorkspacePanelItem::k3DCameraGroup:
            return "Camera";
        case WorkspacePanelItem::kViewFitMap:
            return labels.workspace_subitem_fit_view;
        case WorkspacePanelItem::kViewResetView:
            return labels.workspace_subitem_reset_view;
        case WorkspacePanelItem::k3DCaptureMouse:
            return "Capture Mouse";
        case WorkspacePanelItem::k3DReleaseMouse:
            return "Release Mouse";
        case WorkspacePanelItem::k3DRenderGroup:
            return labels.workspace_tool_render;
        case WorkspacePanelItem::kRenderTerrainMesh:
            return "Terrain Mesh";
        case WorkspacePanelItem::kRenderChunkBounds:
            return labels.workspace_subitem_chunk_bounds;
        case WorkspacePanelItem::kRenderWorldGrid:
            return labels.workspace_subitem_world_grid;
        case WorkspacePanelItem::kRenderCollision:
            return labels.workspace_subitem_collision_overlay;
        case WorkspacePanelItem::kRenderHeight:
            return labels.workspace_subitem_height;
        case WorkspacePanelItem::k3DMeshGroup:
            return "Mesh";
        case WorkspacePanelItem::k3DChunkSizeGroup:
            return "Chunk Size";
        case WorkspacePanelItem::k3DChunkSize16:
            return "16x16";
        case WorkspacePanelItem::k3DChunkSize32:
            return "32x32";
        case WorkspacePanelItem::k3DChunkSizeProfit:
            return "Chunk Profit";
        case WorkspacePanelItem::k3DMeshSimple:
            return "Simple Faces";
        case WorkspacePanelItem::k3DMeshGreedy:
            return "Greedy Faces";
        case WorkspacePanelItem::k3DMeshTerrainSurface:
            return "Terrain Surface";
        case WorkspacePanelItem::k3DDrawModels:
            return "Draw Models";
        case WorkspacePanelItem::k3DVisibleFaces:
            return "Visible Faces";
        case WorkspacePanelItem::k3DCulledFaces:
            return "Culled Faces";
        case WorkspacePanelItem::k3DGreedySaved:
            return "Greedy Saved";
        case WorkspacePanelItem::k3DTerrainFaces:
            return "Terrain Faces";
        case WorkspacePanelItem::k3DTerrainTopFaces:
            return "Terrain Tops";
        case WorkspacePanelItem::k3DTerrainWallFaces:
            return "Terrain Walls";
        case WorkspacePanelItem::k3DTerrainVsGreedy:
            return "Terrain Δ Greedy";
        case WorkspacePanelItem::k3DTotalSaved:
            return "Total Saved";
        case WorkspacePanelItem::k3DChunkMeshes:
            return "Chunk Meshes";
        case WorkspacePanelItem::k3DDirtyRebuildProbe:
            return "Dirty Rebuild Probe";
        case WorkspacePanelItem::k3DDirtyChunks:
            return "Dirty Chunks";
        case WorkspacePanelItem::k3DRebuiltChunks:
            return "Rebuilt Chunks";
        case WorkspacePanelItem::k3DRebuildSaved:
            return "Rebuild Saved";
        case WorkspacePanelItem::kSelectionTileGroup:
            return "Selected Tile";
        case WorkspacePanelItem::kSelectionTileInfo:
            return "Tile Info";
        case WorkspacePanelItem::kSelectionVoxelGroup:
            return "Selected Voxel";
        case WorkspacePanelItem::kSelectionVoxelInfo:
            return "Voxel Info";
        case WorkspacePanelItem::kSelectionChunkGroup:
            return "Selected Chunk";
        case WorkspacePanelItem::kSelectionChunkInfo:
            return "Chunk Info";
        case WorkspacePanelItem::kSelectionActionsGroup:
            return "Actions";
        case WorkspacePanelItem::kSelectionInspect:
            return "Inspect";
        case WorkspacePanelItem::kSelectionFocus:
            return "Focus";
        case WorkspacePanelItem::kSelectionCopyInfo:
            return "Copy Info";
        case WorkspacePanelItem::kPackageMetadataGroup:
            return "Metadata";
        case WorkspacePanelItem::kMapPackage:
            return labels.workspace_subitem_package;
        case WorkspacePanelItem::kMapValidate:
            return labels.workspace_subitem_validate;
        case WorkspacePanelItem::kPackageRuntimeGridsGroup:
            return "Runtime Grids";
        case WorkspacePanelItem::kPackageHeightGrid:
            return "Height Grid";
        case WorkspacePanelItem::kPackageCollisionGrid:
            return "Collision Grid";
        case WorkspacePanelItem::kPackageMovementCostGrid:
            return "Movement Cost Grid";
        case WorkspacePanelItem::kPackageWorldDataGroup:
            return "World Data";
        case WorkspacePanelItem::kPackageObjects:
            return labels.workspace_tool_objects;
        case WorkspacePanelItem::kPackageMarkers:
            return "Markers";
        case WorkspacePanelItem::kPackageRoutes:
            return "Routes";
        case WorkspacePanelItem::kPackageGameplayZones:
            return "Gameplay Zones";
        case WorkspacePanelItem::kDebugRuntimeMap:
            return "RuntimeMap";
        case WorkspacePanelItem::kDebugChunkGrid:
            return "ChunkGrid";
        case WorkspacePanelItem::kDebugVoxelWorld:
            return "VoxelWorld";
        case WorkspacePanelItem::kDebugFaceVisibility:
            return "Face Visibility";
        case WorkspacePanelItem::kDebugChunkMesh:
            return "Chunk Mesh";
        case WorkspacePanelItem::kDebugCamera:
            return "Camera";
        case WorkspacePanelItem::kDebugMemory:
            return labels.workspace_subitem_memory;
        case WorkspacePanelItem::kDebugFps:
            return labels.fps_label;
        case WorkspacePanelItem::kDebugLogs:
            return labels.workspace_subitem_logs;
        case WorkspacePanelItem::kSettingsLanguage:
            return labels.workspace_subitem_language;
        case WorkspacePanelItem::kSettingsCamera:
            return "Camera Settings";
        case WorkspacePanelItem::kSettingsRender:
            return "Render Settings";
    }
    return labels.debug_none;
}

[[nodiscard]] std::string WorkspacePanelItemText(WorkspacePanelItemState item, const UiLabels& labels)
{
    std::string prefix;
    switch (item.kind) {
        case WorkspacePanelItemKind::kGroup:
            prefix = item.depth <= 1 ? "- " : "  ";
            break;
        case WorkspacePanelItemKind::kAction:
            prefix = item.enabled ? "  " : "[-] ";
            break;
        case WorkspacePanelItemKind::kCheckbox:
            prefix = item.enabled ? (item.checked ? "[x] " : "[ ] ") : "[-] ";
            break;
        case WorkspacePanelItemKind::kRadio:
            prefix = item.enabled ? (item.checked ? "(x) " : "( ) ") : "(-) ";
            break;
        case WorkspacePanelItemKind::kValue:
            prefix = item.enabled ? "= " : "- ";
            break;
    }
    return prefix + WorkspacePanelItemLabel(item.item, labels);
}

[[nodiscard]] std::string FormatMemory(std::uint64_t bytes, const UiLabels& labels)
{
    if (bytes == 0) {
        return labels.debug_none;
    }

    const double mib = static_cast<double>(bytes) / (1024.0 * 1024.0);
    std::ostringstream out;
    if (mib >= 100.0) {
        out << static_cast<int>(std::round(mib)) << " MiB";
    } else {
        out.setf(std::ios::fixed);
        out.precision(1);
        out << mib << " MiB";
    }
    return out.str();
}

[[nodiscard]] Color CellColor(MapCellKind kind)
{
    switch (kind) {
        case MapCellKind::kOpen:
            return Color{176, 178, 158, 255};
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

void DrawWorkspaceOverview(
    const WorkspaceState& workspace_state,
    const WorkspaceLayout& workspace,
    const UiMetrics& metrics)
{
    const MapOverview& overview = workspace_state.map.overview;
    const Rectangle area = workspace.map_overview;
    DrawRectangleRec(area, Color{118, 120, 118, 255});
    DrawRectangleLinesEx(area, metrics.workspace_border_width, kEditorBorder);

    if (!overview.IsValid() || !workspace_state.show_terrain_layer) {
        return;
    }

    const float padding = std::max(4.0F, metrics.workspace_border_width * 2.0F);
    const Rectangle inner{
        area.x + padding,
        area.y + padding,
        std::max(1.0F, area.width - padding * 2.0F),
        std::max(1.0F, area.height - padding * 2.0F),
    };

    const float map_aspect = static_cast<float>(overview.width) / static_cast<float>(overview.height);
    float draw_width = inner.width;
    float draw_height = draw_width / map_aspect;
    if (draw_height > inner.height) {
        draw_height = inner.height;
        draw_width = draw_height * map_aspect;
    }

    const Rectangle map_rect{
        inner.x + (inner.width - draw_width) * 0.5F,
        inner.y + (inner.height - draw_height) * 0.5F,
        draw_width,
        draw_height,
    };

    constexpr int kMaxDrawAxis = 320;
    const int draw_cols = std::max(1, std::min(overview.width, kMaxDrawAxis));
    const int draw_rows = std::max(1, std::min(overview.height, kMaxDrawAxis));
    const float cell_width = map_rect.width / static_cast<float>(draw_cols);
    const float cell_height = map_rect.height / static_cast<float>(draw_rows);

    for (int y = 0; y < draw_rows; ++y) {
        const int source_y = std::min(overview.height - 1, static_cast<int>((static_cast<long long>(y) * overview.height) / draw_rows));
        for (int x = 0; x < draw_cols; ++x) {
            const int source_x = std::min(overview.width - 1, static_cast<int>((static_cast<long long>(x) * overview.width) / draw_cols));
            const std::size_t index = static_cast<std::size_t>(source_y) * static_cast<std::size_t>(overview.width)
                + static_cast<std::size_t>(source_x);
            DrawRectangleRec(
                Rectangle{
                    map_rect.x + static_cast<float>(x) * cell_width,
                    map_rect.y + static_cast<float>(y) * cell_height,
                    std::max(1.0F, cell_width + 0.6F),
                    std::max(1.0F, cell_height + 0.6F),
                },
                CellColor(overview.cells[index]));
        }
    }

    if (workspace_state.show_grid_layer) {
        const int grid_step_x = std::max(1, draw_cols / 16);
        const int grid_step_y = std::max(1, draw_rows / 16);
        constexpr Color grid{20, 24, 24, 90};
        for (int x = grid_step_x; x < draw_cols; x += grid_step_x) {
            const float px = map_rect.x + static_cast<float>(x) * cell_width;
            DrawLineEx(Vector2{px, map_rect.y}, Vector2{px, map_rect.y + map_rect.height}, 1.0F, grid);
        }
        for (int y = grid_step_y; y < draw_rows; y += grid_step_y) {
            const float py = map_rect.y + static_cast<float>(y) * cell_height;
            DrawLineEx(Vector2{map_rect.x, py}, Vector2{map_rect.x + map_rect.width, py}, 1.0F, grid);
        }
    }

    DrawRectangleLinesEx(map_rect, metrics.workspace_border_width, Color{235, 235, 220, 255});
}


void DrawWorkspaceWirePlaceholder(const WorkspaceLayout& workspace, const UiMetrics& metrics)
{
    const Rectangle area = workspace.viewport;
    const float center_x = area.x + area.width * 0.5F;
    const float center_y = area.y + area.height * 0.54F;
    const float w = std::min(area.width, area.height) * 0.22F;
    const float h = w * 0.72F;

    const Vector2 a{center_x - w * 0.55F, center_y + h * 0.35F};
    const Vector2 b{center_x + w * 0.45F, center_y + h * 0.48F};
    const Vector2 c{center_x + w * 0.60F, center_y - h * 0.35F};
    const Vector2 d{center_x - w * 0.38F, center_y - h * 0.48F};
    const Vector2 e{center_x - w * 0.10F, center_y - h * 0.82F};
    const Vector2 f{center_x + w * 0.75F, center_y - h * 0.55F};
    const Vector2 g{center_x + w * 0.60F, center_y + h * 0.15F};
    const Vector2 hpt{center_x - w * 0.25F, center_y - h * 0.05F};

    constexpr Color wire{245, 245, 240, 255};
    DrawLineEx(a, b, metrics.workspace_border_width, wire);
    DrawLineEx(b, c, metrics.workspace_border_width, wire);
    DrawLineEx(c, d, metrics.workspace_border_width, wire);
    DrawLineEx(d, a, metrics.workspace_border_width, wire);
    DrawLineEx(d, e, metrics.workspace_border_width, wire);
    DrawLineEx(e, f, metrics.workspace_border_width, wire);
    DrawLineEx(f, c, metrics.workspace_border_width, wire);
    DrawLineEx(f, g, metrics.workspace_border_width, wire);
    DrawLineEx(g, b, metrics.workspace_border_width, wire);
    DrawLineEx(a, hpt, metrics.workspace_border_width, wire);
    DrawLineEx(hpt, e, metrics.workspace_border_width, wire);
    DrawLineEx(hpt, g, metrics.workspace_border_width, wire);
    DrawLineEx(hpt, c, metrics.workspace_border_width, wire);
}

[[nodiscard]] MainMenuLayout BuildMainMenuLayout(const MenuState& menu, const UiFontSet& fonts, const UiMetrics& metrics)
{
    MainMenuLayout layout;
    layout.items.reserve(menu.items.size());
    layout.title_y = static_cast<float>(metrics.window_height) * 0.22F;
    layout.subtitle_y = layout.title_y + metrics.title_font_size + 18.0F * metrics.scale;

    float max_label_width = 0.0F;
    for (const auto& item : menu.items) {
        max_label_width = std::max(max_label_width, Measure(fonts.text, "> " + item.title, metrics.menu_font_size, metrics.text_spacing).x);
    }

    const float item_height = metrics.menu_font_size + metrics.menu_item_padding_y * 2.0F;
    const float total_height = static_cast<float>(menu.items.size()) * item_height
        + static_cast<float>(menu.items.size() > 0 ? menu.items.size() - 1 : 0) * metrics.menu_item_gap;
    const float start_y = static_cast<float>(metrics.window_height) * 0.52F - total_height * 0.5F;
    const float item_width = std::min(
        std::max(max_label_width + metrics.menu_item_padding_x * 2.0F, 190.0F * metrics.scale),
        static_cast<float>(metrics.window_width) - metrics.screen_padding * 2.0F);
    const float x = (static_cast<float>(metrics.window_width) - item_width) * 0.5F;

    for (std::size_t i = 0; i < menu.items.size(); ++i) {
        const float y = start_y + static_cast<float>(i) * (item_height + metrics.menu_item_gap);
        const Rectangle bounds{x, y, item_width, item_height};
        layout.items.push_back(MenuItemBounds{
            static_cast<int>(i),
            bounds,
            Vector2{bounds.x + metrics.menu_item_padding_x, bounds.y + metrics.menu_item_padding_y},
        });
    }

    return layout;
}


[[nodiscard]] PlaceholderLayout BuildPlaceholderLayout(const UiFontSet& fonts, const UiMetrics& metrics, const UiLabels& labels)
{
    PlaceholderLayout layout;
    layout.actions.reserve(2);
    layout.title_y = static_cast<float>(metrics.window_height) * 0.42F;
    layout.hint_y = layout.title_y + 48.0F * metrics.scale;

    const float font_size = metrics.dialog_button_font_size;
    const float spacing = FontSpacing(font_size);
    const float main_menu_width = Measure(fonts.text, labels.placeholder_main_menu, font_size, spacing).x;
    const float exit_width = Measure(fonts.text, labels.placeholder_exit, font_size, spacing).x;
    const float button_width = std::clamp(
        std::max({main_menu_width, exit_width, 130.0F * metrics.scale}) + metrics.menu_item_padding_x * 2.0F,
        130.0F * metrics.scale,
        260.0F * metrics.scale);
    const float button_height = std::max(
        metrics.dialog_button_font_size + metrics.menu_item_padding_y * 2.0F,
        42.0F * metrics.scale);
    const float button_gap = 18.0F * metrics.scale;
    const float total_width = button_width * 2.0F + button_gap;
    const float start_x = (static_cast<float>(metrics.window_width) - total_width) * 0.5F;
    const float y = layout.hint_y + metrics.placeholder_hint_font_size + 36.0F * metrics.scale;

    const Rectangle main_menu{start_x, y, button_width, button_height};
    const Rectangle exit{start_x + button_width + button_gap, y, button_width, button_height};
    layout.actions.push_back(PlaceholderActionBounds{
        PlaceholderAction::kMainMenu,
        main_menu,
        Vector2{main_menu.x, main_menu.y + (main_menu.height - font_size) * 0.5F},
    });
    layout.actions.push_back(PlaceholderActionBounds{
        PlaceholderAction::kExit,
        exit,
        Vector2{exit.x, exit.y + (exit.height - font_size) * 0.5F},
    });
    return layout;
}


[[nodiscard]] WorkspaceLayout BuildWorkspaceLayout(const UiMetrics& metrics, const WorkspaceState& workspace_state)
{
    WorkspaceLayout layout;
    layout.tools.reserve(7);

    const float status_height = metrics.workspace_status_height;
    const float panel_width = metrics.workspace_panel_width;
    const float border = metrics.workspace_border_width;
    const float padding = metrics.workspace_panel_padding;
    const float gap = metrics.workspace_tool_gap;

    layout.status_bar = Rectangle{
        0.0F,
        static_cast<float>(metrics.window_height) - status_height,
        static_cast<float>(metrics.window_width),
        status_height,
    };
    layout.tool_panel = Rectangle{
        static_cast<float>(metrics.window_width) - panel_width,
        0.0F,
        panel_width,
        static_cast<float>(metrics.window_height) - status_height,
    };
    layout.viewport = Rectangle{
        0.0F,
        0.0F,
        static_cast<float>(metrics.window_width) - panel_width,
        static_cast<float>(metrics.window_height) - status_height,
    };

    const float viewport_padding = std::max(border * 4.0F, metrics.screen_padding * 0.45F);
    layout.map_summary = Rectangle{};
    layout.map_overview = Rectangle{
        layout.viewport.x + viewport_padding,
        layout.viewport.y + viewport_padding,
        std::max(1.0F, layout.viewport.width - viewport_padding * 2.0F),
        std::max(1.0F, layout.viewport.height - viewport_padding * 2.0F),
    };

    layout.tool_header = Rectangle{
        layout.tool_panel.x + border,
        layout.tool_panel.y + border,
        std::max(1.0F, layout.tool_panel.width - border * 2.0F),
        0.0F,
    };

    constexpr std::array<WorkspaceTool, 7> tools{
        WorkspaceTool::kMode,
        WorkspaceTool::kMap2D,
        WorkspaceTool::kWorld3D,
        WorkspaceTool::kSelection,
        WorkspaceTool::kPackageData,
        WorkspaceTool::kDebug,
        WorkspaceTool::kSettings,
    };

    const float tool_height = metrics.workspace_tool_font_size + gap * 1.35F;
    const float item_height = metrics.workspace_tool_font_size + gap * 1.05F;
    const float tool_x = layout.tool_panel.x + padding;
    float row_y = layout.tool_panel.y + padding;
    const float tool_width = std::max(1.0F, layout.tool_panel.width - padding * 2.0F);
    const float row_bottom = layout.tool_panel.y + layout.tool_panel.height - padding;
    const float subitem_indent = gap * 2.4F;

    const auto selected_items = BuildWorkspacePanelItems(workspace_state);
    for (WorkspaceTool tool : tools) {
        if (row_y + tool_height > row_bottom) {
            break;
        }

        const Rectangle bounds{tool_x, row_y, tool_width, tool_height};
        layout.tools.push_back(WorkspaceToolBounds{
            tool,
            bounds,
            Vector2{bounds.x + gap, bounds.y + (bounds.height - metrics.workspace_tool_font_size) * 0.5F},
        });
        row_y += tool_height;

        if (tool != workspace_state.selected_tool || !workspace_state.selected_tool_expanded) {
            continue;
        }

        layout.panel_items.reserve(selected_items.size());
        for (const WorkspacePanelItemState& item : selected_items) {
            if (row_y + item_height > row_bottom) {
                break;
            }
            const float indent = subitem_indent * static_cast<float>(std::max(0, item.depth));
            const Rectangle item_bounds{
                tool_x + indent,
                row_y,
                std::max(1.0F, tool_width - indent),
                item_height,
            };
            layout.panel_items.push_back(WorkspacePanelItemBounds{
                item.item,
                item.kind,
                item.depth,
                item_bounds,
                Vector2{item_bounds.x + gap, item_bounds.y + (item_bounds.height - metrics.workspace_tool_font_size) * 0.5F},
                item.enabled,
                item.checked,
            });
            row_y += item_height;
        }
    }

    layout.tool_menu = Rectangle{
        tool_x,
        layout.tool_panel.y + padding,
        tool_width,
        std::max(1.0F, row_y - (layout.tool_panel.y + padding)),
    };

    const float info_y = row_y + gap * 2.0F;
    layout.tool_info = Rectangle{
        layout.tool_panel.x + padding,
        info_y,
        tool_width,
        std::max(1.0F, row_bottom - info_y),
    };

    return layout;
}

[[nodiscard]] ConfirmDialogLayout BuildExitDialogLayout(const UiFontSet& fonts, const UiMetrics& metrics, const UiLabels& labels)
{
    ConfirmDialogLayout layout;

    const float max_panel_width = static_cast<float>(metrics.window_width) * 0.76F;
    const float min_panel_width = std::min(440.0F * metrics.scale, max_panel_width);
    const float panel_width = std::clamp(610.0F * metrics.scale, min_panel_width, max_panel_width);
    const float content_width = std::max(1.0F, panel_width - metrics.dialog_padding * 2.0F);

    const std::string& title = labels.dialog_exit_title;
    const std::string& message = labels.dialog_exit_message;
    const float title_size = FitTextToWidth(fonts.text, title, metrics.dialog_title_font_size, 14.0F, content_width);
    const float text_size = FitTextToWidth(fonts.text, message, metrics.dialog_text_font_size, 12.0F, content_width);
    layout.message_lines = WrapText(fonts.text, message, text_size, FontSpacing(text_size), content_width);

    const float message_height = static_cast<float>(layout.message_lines.size()) * (text_size + 8.0F * metrics.scale);
    const float buttons_top_gap = 30.0F * metrics.scale;
    const float title_to_message_gap = 28.0F * metrics.scale;
    const float panel_height = metrics.dialog_padding + title_size + title_to_message_gap + message_height + buttons_top_gap
        + metrics.dialog_button_height + metrics.dialog_padding;

    layout.panel = Rectangle{
        (static_cast<float>(metrics.window_width) - panel_width) * 0.5F,
        (static_cast<float>(metrics.window_height) - panel_height) * 0.5F,
        panel_width,
        panel_height,
    };
    layout.title_y = layout.panel.y + metrics.dialog_padding;
    layout.message_y = layout.title_y + title_size + title_to_message_gap;

    const float button_total_width = metrics.dialog_button_width * 2.0F + metrics.dialog_button_gap;
    const float button_y = layout.panel.y + layout.panel.height - metrics.dialog_padding - metrics.dialog_button_height;
    const float button_x = layout.panel.x + (layout.panel.width - button_total_width) * 0.5F;
    layout.buttons = DialogButtonBounds{
        Rectangle{button_x, button_y, metrics.dialog_button_width, metrics.dialog_button_height},
        Rectangle{button_x + metrics.dialog_button_width + metrics.dialog_button_gap, button_y, metrics.dialog_button_width, metrics.dialog_button_height},
    };

    return layout;
}

}  // namespace

std::string_view ToString(PlaceholderAction action)
{
    switch (action) {
        case PlaceholderAction::kMainMenu:
            return "main_menu";
        case PlaceholderAction::kExit:
            return "exit";
    }
    return "unknown";
}

UiMetrics CalculateUiMetrics(const WindowConfig& window, const AppConfig& config)
{
    const float scale = CalculateUiScale(window.window_width, window.window_height, config);
    const float resolved_font_scale = std::clamp(config.ui_font_scale, 0.50F, 2.00F);
    const float text_scale = scale * resolved_font_scale;
    UiMetrics metrics;
    metrics.window_width = window.window_width;
    metrics.window_height = window.window_height;
    metrics.scale = scale;
    metrics.font_scale = resolved_font_scale;

    metrics.title_font_size = Scaled(kBaseTitleFontSize, text_scale, 20.0F, 46.0F);
    metrics.subtitle_font_size = Scaled(kBaseSubtitleFontSize, text_scale, 12.0F, 24.0F);
    metrics.menu_font_size = Scaled(kBaseMenuFontSize, text_scale, 16.0F, 32.0F);
    metrics.placeholder_title_font_size = Scaled(kBasePlaceholderTitleFontSize, text_scale, 18.0F, 38.0F);
    metrics.placeholder_hint_font_size = Scaled(kBasePlaceholderHintFontSize, text_scale, 13.0F, 26.0F);
    metrics.fps_font_size = Scaled(kBaseFpsFontSize, text_scale, 12.0F, 22.0F);
    metrics.debug_font_size = Scaled(kBaseDebugFontSize, text_scale, 10.0F, 18.0F);
    metrics.dialog_title_font_size = Scaled(kBaseDialogTitleFontSize, text_scale, 18.0F, 34.0F);
    metrics.dialog_text_font_size = Scaled(kBaseDialogTextFontSize, text_scale, 14.0F, 26.0F);
    metrics.dialog_button_font_size = Scaled(kBaseDialogButtonFontSize, text_scale, 14.0F, 26.0F);
    metrics.workspace_tool_font_size = Scaled(kBaseWorkspaceToolFontSize, text_scale, 13.0F, 26.0F);
    metrics.workspace_status_font_size = Scaled(kBaseWorkspaceStatusFontSize, text_scale, 12.0F, 22.0F);

    metrics.text_spacing = FontSpacing(metrics.menu_font_size);
    metrics.screen_padding = Scaled(16.0F, scale, 10.0F, 32.0F);
    metrics.menu_item_gap = Scaled(12.0F, scale, 8.0F, 24.0F);
    metrics.menu_item_padding_x = Scaled(18.0F, scale, 12.0F, 32.0F);
    metrics.menu_item_padding_y = Scaled(8.0F, scale, 6.0F, 14.0F);
    metrics.debug_line_gap = Scaled(15.0F, scale, 12.0F, 20.0F);
    metrics.dialog_padding = Scaled(40.0F, scale, 24.0F, 64.0F);
    metrics.dialog_button_width = Scaled(150.0F, scale, 120.0F, 210.0F);
    metrics.dialog_button_height = std::max(Scaled(48.0F, scale, 38.0F, 68.0F), metrics.dialog_button_font_size + Scaled(22.0F, scale, 16.0F, 30.0F));
    metrics.dialog_button_gap = Scaled(28.0F, scale, 16.0F, 42.0F);
    metrics.modal_border_width = Scaled(2.0F, scale, 1.0F, 4.0F);
    metrics.workspace_panel_width = Scaled(230.0F, scale, 185.0F, 300.0F);
    metrics.workspace_status_height = Scaled(42.0F, scale, 30.0F, 58.0F);
    metrics.workspace_border_width = Scaled(2.0F, scale, 1.0F, 4.0F);
    metrics.workspace_tool_gap = Scaled(7.0F, scale, 4.0F, 12.0F);
    metrics.workspace_panel_padding = Scaled(10.0F, scale, 7.0F, 18.0F);
    return metrics;
}

UiLayoutCache RebuildUiLayout(
    const MenuState& menu,
    const UiFontSet& fonts,
    const WindowConfig& window,
    const AppConfig& config,
    const UiLabels& labels,
    const WorkspaceState& workspace)
{
    UiLayoutCache layout;
    layout.metrics = CalculateUiMetrics(window, config);
    layout.main_menu = BuildMainMenuLayout(menu, fonts, layout.metrics);
    layout.placeholder = BuildPlaceholderLayout(fonts, layout.metrics, labels);
    layout.workspace = BuildWorkspaceLayout(layout.metrics, workspace);
    layout.exit_dialog = BuildExitDialogLayout(fonts, layout.metrics, labels);
    return layout;
}

void DrawMainMenu(const MenuState& menu, const UiFontSet& fonts, const UiLabels& labels, const UiLayoutCache& layout)
{
    const UiMetrics& metrics = layout.metrics;
    DrawTextCentered(fonts.title, labels.app_title, layout.main_menu.title_y, metrics.title_font_size, FontSpacing(metrics.title_font_size), kSelectedText, metrics.window_width);
    DrawTextCentered(
        fonts.text,
        labels.app_subtitle,
        layout.main_menu.subtitle_y,
        metrics.subtitle_font_size,
        FontSpacing(metrics.subtitle_font_size),
        kMutedText,
        metrics.window_width);

    for (const auto& item_bounds : layout.main_menu.items) {
        const auto& item = menu.items[static_cast<std::size_t>(item_bounds.index)];
        const bool selected = item_bounds.index == menu.selected_index;
        const Color color = !item.enabled ? kDisabledText : (selected ? kSelectedText : kText);

        if (selected && item.enabled) {
            DrawRectangleRounded(item_bounds.bounds, 0.18F, 8, Color{48, 52, 68, 230});
            DrawRectangleRoundedLinesEx(item_bounds.bounds, 0.18F, 8, metrics.modal_border_width, kAccent);
        }

        const std::string marker = selected ? "> " : "  ";
        DrawTextEx(
            fonts.text,
            (marker + item.title).c_str(),
            item_bounds.text_position,
            metrics.menu_font_size,
            FontSpacing(metrics.menu_font_size),
            color);
    }
}

void DrawPlaceholderScreen(
    const std::string& title,
    PlaceholderAction selected_action,
    const UiFontSet& fonts,
    const UiLabels& labels,
    const UiLayoutCache& layout)
{
    const UiMetrics& metrics = layout.metrics;
    const PlaceholderLayout& placeholder = layout.placeholder;
    DrawTextCentered(
        fonts.text,
        title,
        placeholder.title_y,
        metrics.placeholder_title_font_size,
        FontSpacing(metrics.placeholder_title_font_size),
        kText,
        metrics.window_width);
    DrawTextCentered(
        fonts.text,
        labels.placeholder_back_hint,
        placeholder.hint_y,
        metrics.placeholder_hint_font_size,
        FontSpacing(metrics.placeholder_hint_font_size),
        kMutedText,
        metrics.window_width);

    const std::array<std::pair<PlaceholderAction, std::string>, 2> actions{{
        {PlaceholderAction::kMainMenu, labels.placeholder_main_menu},
        {PlaceholderAction::kExit, labels.placeholder_exit},
    }};

    for (const auto& bounds : placeholder.actions) {
        const auto label_it = std::find_if(actions.begin(), actions.end(), [action = bounds.action](const auto& item) {
            return item.first == action;
        });
        const std::string& label = label_it != actions.end() ? label_it->second : labels.debug_none;
        const bool selected = bounds.action == selected_action;
        DrawRectangleRounded(bounds.bounds, 0.16F, 8, selected ? Color{66, 60, 42, 245} : Color{38, 42, 54, 220});
        DrawRectangleRoundedLinesEx(bounds.bounds, 0.16F, 8, metrics.modal_border_width, selected ? kAccent : kPanelBorder);

        const float font_size = metrics.dialog_button_font_size;
        const float spacing = FontSpacing(font_size);
        const Vector2 measured = Measure(fonts.text, label, font_size, spacing);
        DrawTextEx(
            fonts.text,
            label.c_str(),
            Vector2{
                bounds.bounds.x + (bounds.bounds.width - measured.x) * 0.5F,
                bounds.bounds.y + (bounds.bounds.height - measured.y) * 0.5F,
            },
            font_size,
            spacing,
            selected ? kSelectedText : kText);
    }
}



void DrawWorkspace(
    const WorkspaceState& workspace_state,
    const RaylibChunkMeshPreview* mesh_preview,
    const Camera3D* preview_camera,
    FreeFlyCameraStatus camera_status,
    const UiFontSet& fonts,
    const UiLabels& labels,
    const UiLayoutCache& layout)
{
    const UiMetrics& metrics = layout.metrics;
    const WorkspaceLayout& workspace = layout.workspace;

    DrawRectangle(0, 0, metrics.window_width, metrics.window_height, kEditorBackground);

    DrawRectangleRec(workspace.viewport, kEditorViewport);
    DrawRectangleLinesEx(workspace.viewport, metrics.workspace_border_width, kEditorBorder);
    DrawRectangleRec(workspace.tool_panel, kEditorBackground);
    DrawRectangleLinesEx(workspace.tool_panel, metrics.workspace_border_width, kEditorBorder);
    DrawRectangleRec(workspace.status_bar, kEditorStatus);
    DrawRectangleLinesEx(workspace.status_bar, metrics.workspace_border_width, kEditorBorder);

    const float spacing = FontSpacing(metrics.workspace_tool_font_size);
    for (const auto& tool_bounds : workspace.tools) {
        const bool selected = workspace_state.selected_tool == tool_bounds.tool;
        const std::string label = WorkspaceToolLabel(tool_bounds.tool, labels);
        const std::string marker = selected && workspace_state.selected_tool_expanded ? "- " : "+ ";
        if (selected) {
            DrawRectangleRec(tool_bounds.bounds, Color{4, 92, 120, 255});
        }
        DrawTextEx(
            fonts.text,
            (marker + label).c_str(),
            tool_bounds.text_position,
            metrics.workspace_tool_font_size,
            spacing,
            selected ? kAccent : kEditorPanelText);
    }

    for (const auto& item_bounds : workspace.panel_items) {
        const WorkspacePanelItemState item{
            item_bounds.item,
            item_bounds.kind,
            item_bounds.depth,
            item_bounds.enabled,
            item_bounds.checked,
        };
        const std::string text = WorkspacePanelItemText(item, labels);
        const bool is_group = item_bounds.kind == WorkspacePanelItemKind::kGroup;
        const Color color = is_group
            ? kEditorViewportText
            : (!item_bounds.enabled ? Color{74, 138, 154, 255} : (item_bounds.checked ? kAccent : kEditorPanelText));
        DrawTextEx(
            fonts.text,
            text.c_str(),
            item_bounds.text_position,
            metrics.workspace_tool_font_size,
            spacing,
            color);
    }

    const float compact_size = std::max(10.0F, metrics.workspace_status_font_size - 1.0F);
    const float compact_spacing = FontSpacing(compact_size);
    const float info_bottom = workspace.tool_info.y + workspace.tool_info.height;
    std::vector<std::string> tool_info_lines{
        labels.workspace_status_ready + ": " + MapStatusLabel(workspace_state.map, labels),
        labels.workspace_map_label + ": " + (workspace_state.map.configured ? workspace_state.map.path.filename().string() : labels.debug_none),
        labels.workspace_size_label + ": " + MapSizeText(workspace_state.map, labels),
        labels.workspace_tile_label + ": " + MapTileText(workspace_state.map, labels),
        labels.workspace_levels_label + ": " + MapLevelsText(workspace_state.map, labels),
        labels.workspace_terrain_label + ": " + BoolText(workspace_state.runtime_map.info.terrain_loaded, labels),
        labels.workspace_elevation_label + ": " + BoolText(workspace_state.runtime_map.info.elevation_loaded, labels),
        labels.workspace_collision_label + ": " + BoolText(workspace_state.runtime_map.info.collision_loaded, labels),
    };
    if (workspace_state.chunk_grid.IsValid()) {
        tool_info_lines.push_back("Chunk Size: " + std::to_string(workspace_state.chunk_size_tiles));
        tool_info_lines.push_back("Chunks: " + std::to_string(workspace_state.chunk_grid.info.chunks_x) + "x"
            + std::to_string(workspace_state.chunk_grid.info.chunks_y) + "="
            + std::to_string(workspace_state.chunk_grid.info.total_chunks));
    }
    if (workspace_state.chunk_meshes.IsValid()) {
        tool_info_lines.push_back("Mesh: " + MeshModeLabel(workspace_state.mesh_mode));
        tool_info_lines.push_back("Faces: " + std::to_string(workspace_state.mesh_stats.active_faces));
        tool_info_lines.push_back("Models: " + std::to_string(workspace_state.mesh_stats.draw_models));
        tool_info_lines.push_back("Simple: " + std::to_string(workspace_state.mesh_stats.simple_faces));
        tool_info_lines.push_back("Greedy: " + std::to_string(workspace_state.mesh_stats.greedy_faces));
        tool_info_lines.push_back("Terrain: " + std::to_string(workspace_state.mesh_stats.terrain_faces));
        tool_info_lines.push_back("Terrain T/W: " + std::to_string(workspace_state.mesh_stats.terrain_top_faces) + "/"
            + std::to_string(workspace_state.mesh_stats.terrain_wall_faces));
        tool_info_lines.push_back("Terrain Δ greedy: " + CompactSignedPercent(workspace_state.mesh_stats.TerrainVsGreedyDeltaRatio()));
        tool_info_lines.push_back("Saved: " + CompactPercent(workspace_state.mesh_stats.ActiveReductionRatio()));
    }
    if (workspace_state.chunk_size_comparison.available) {
        tool_info_lines.push_back("Chunk Δ models: " + CompactSignedPercent(workspace_state.chunk_size_comparison.DrawModelDeltaRatio()));
        tool_info_lines.push_back("Chunk Δ faces: " + CompactSignedPercent(workspace_state.chunk_size_comparison.FaceDeltaRatio()));
    }
    if (workspace_state.chunk_mesh_cache.IsValid()) {
        tool_info_lines.push_back("Dirty: " + std::to_string(workspace_state.chunk_mesh_cache.info.dirty_chunks));
    }
    if (workspace_state.last_mesh_rebuild.attempted) {
        tool_info_lines.push_back("Rebuilt: " + std::to_string(workspace_state.last_mesh_rebuild.rebuilt_chunks) + "/"
            + std::to_string(workspace_state.last_mesh_rebuild.total_chunks));
        tool_info_lines.push_back("Rebuild saved: " + CompactPercent(workspace_state.last_mesh_rebuild.SavedRebuildWorkRatio()));
    }
    if (workspace_state.show_3d_preview && camera_status.initialized) {
        tool_info_lines.push_back(std::string("Cam: ") + (camera_status.cursor_captured ? "captured" : "free"));
        tool_info_lines.push_back("Yaw/Pitch: " + CompactFloat(camera_status.yaw_degrees) + "/" + CompactFloat(camera_status.pitch_degrees));
        tool_info_lines.push_back("Pos: " + CompactVector(camera_status.position));
    }
    float info_y = workspace.tool_info.y;
    for (const auto& line : tool_info_lines) {
        if (info_y + compact_size > info_bottom) {
            break;
        }
        DrawTextEx(fonts.text, line.c_str(), Vector2{workspace.tool_info.x, info_y}, compact_size, compact_spacing, kEditorPanelText);
        info_y += compact_size + metrics.workspace_tool_gap * 0.4F;
    }

    if (workspace_state.show_3d_preview && mesh_preview != nullptr && preview_camera != nullptr && mesh_preview->IsUploaded()) {
        DrawRectangleRec(workspace.map_overview, Color{18, 22, 24, 255});
        const RaylibChunkMeshDebugOverlayOptions overlays{
            workspace_state.show_3d_chunk_bounds,
            workspace_state.show_3d_world_grid,
            workspace_state.show_3d_collision_overlay,
            workspace_state.show_3d_height_overlay,
        };
        mesh_preview->Draw(
            workspace.map_overview,
            workspace_state.chunk_meshes,
            *preview_camera,
            &workspace_state.runtime_map,
            &workspace_state.chunk_grid,
            overlays);
        DrawRectangleLinesEx(workspace.map_overview, metrics.workspace_border_width, Color{235, 235, 220, 255});
    } else {
        DrawWorkspaceOverview(workspace_state, workspace, metrics);
        if (!workspace_state.map.overview.IsValid() || !workspace_state.show_terrain_layer) {
            DrawWorkspaceWirePlaceholder(workspace, metrics);
            DrawTextCentered(
                fonts.text,
                labels.workspace_overview_unavailable,
                workspace.map_overview.y + workspace.map_overview.height * 0.5F + metrics.workspace_status_font_size * 2.2F,
                metrics.workspace_status_font_size,
                FontSpacing(metrics.workspace_status_font_size),
                kEditorViewportText,
                static_cast<int>(workspace.viewport.width));
        }
    }

    const std::string preview_mode = workspace_state.show_3d_preview ? "3D" : "2D";
    const std::string controls = workspace_state.show_3d_preview
        ? " | click: capture | Esc: release | WASD | Q/E | wheel | F fit | F4-F10 | F3 | "
        : " | F3 2D/3D | ";
    const std::string mesh_status = workspace_state.chunk_meshes.IsValid()
        ? " | " + CompactMeshStats(workspace_state)
        : "";
    const std::string status = labels.workspace_status_ready + " | " + MapStatusLabel(workspace_state.map, labels) + " | view="
        + preview_mode + mesh_status + controls + labels.workspace_status_escape_hint;
    DrawTextEx(
        fonts.text,
        status.c_str(),
        Vector2{metrics.screen_padding * 0.35F, workspace.status_bar.y + (workspace.status_bar.height - metrics.workspace_status_font_size) * 0.5F},
        metrics.workspace_status_font_size,
        FontSpacing(metrics.workspace_status_font_size),
        kEditorStatusText);
}

void DrawFpsCounter(
    const UiFontSet& fonts,
    const UiLabels& labels,
    const UiLayoutCache& layout,
    const ProcessMemoryInfo& memory)
{
    const UiMetrics& metrics = layout.metrics;
    const Font font = fonts.text;
    const std::string text = labels.fps_label + ": " + std::to_string(GetFPS()) + " | " + labels.memory_label + ": "
        + (memory.available ? FormatMemory(memory.resident_bytes, labels) : labels.debug_none);
    const float spacing = FontSpacing(metrics.fps_font_size);
    const Vector2 size = Measure(font, text, metrics.fps_font_size, spacing);
    const Rectangle status = layout.workspace.status_bar;
    const float y = status.height > 1.0F
        ? status.y + (status.height - size.y) * 0.5F
        : metrics.screen_padding;
    DrawTextEx(
        font,
        text.c_str(),
        Vector2{static_cast<float>(metrics.window_width) - size.x - metrics.screen_padding * 0.35F, y},
        metrics.fps_font_size,
        spacing,
        status.height > 1.0F ? kEditorStatusText : kText);
}

void DrawDebugOverlay(
    const UiFontSet& fonts,
    const AppConfig& config,
    const WindowConfig& window,
    AppScreen screen,
    ModalDialog dialog,
    const MenuState& menu,
    const WorkspaceState& workspace,
    std::string_view hovered_item,
    const UiLabels& labels,
    const UiLayoutCache& layout)
{
    const UiMetrics& metrics = layout.metrics;
    const Font font = fonts.text;
    const float spacing = FontSpacing(metrics.debug_font_size);

    Rectangle panel = layout.workspace.tool_info;
    Color text_color = kEditorStatusText;
    if (panel.width <= 1.0F || panel.height <= 1.0F) {
        const float panel_width = std::min(static_cast<float>(metrics.window_width) * 0.44F, 360.0F * metrics.scale);
        const float panel_height = std::min(static_cast<float>(metrics.window_height) * 0.42F, 230.0F * metrics.scale);
        panel = Rectangle{
            static_cast<float>(metrics.window_width) - panel_width - metrics.screen_padding,
            static_cast<float>(metrics.window_height) - panel_height - metrics.workspace_status_height - metrics.screen_padding,
            panel_width,
            panel_height,
        };
        text_color = kMutedText;
    }

    DrawRectangleRec(panel, kEditorStatus);
    DrawRectangleLinesEx(panel, metrics.workspace_border_width, kEditorBorder);

    const float x = panel.x + metrics.workspace_tool_gap;
    float y = panel.y + metrics.workspace_tool_gap;
    const float bottom = panel.y + panel.height - metrics.workspace_tool_gap;

    std::array<std::string, 10> lines{};
    lines[0] = labels.debug_version + ": " + config.version;
    lines[1] = labels.debug_screen + ": " + std::string(ToString(screen));
    lines[2] = labels.debug_window + ": " + std::to_string(window.window_width) + "x" + std::to_string(window.window_height);
    lines[3] = labels.debug_ui_scale + ": " + std::to_string(window.ui_scale).substr(0, 4);
    lines[4] = labels.debug_modal + ": " + std::string(ToString(dialog));
    lines[5] = labels.debug_selected + ": " + labels.debug_none;
    lines[6] = labels.debug_hovered + ": " + (hovered_item.empty() ? labels.debug_none : std::string(hovered_item));
    lines[7] = labels.debug_workspace_tool + ": " + std::string(ToString(workspace.selected_tool));
    lines[8] = labels.debug_map_path + ": " + (workspace.map.configured ? workspace.map.path.filename().string() : labels.debug_none);
    lines[9] = labels.debug_map_loaded + ": " + (workspace.map.loaded ? labels.dialog_yes : labels.dialog_no);
    if (menu.selected_index >= 0 && menu.selected_index < static_cast<int>(menu.items.size())) {
        lines[5] = labels.debug_selected + ": " + std::string(ToString(menu.items[static_cast<std::size_t>(menu.selected_index)].id));
    }

    for (const auto& line : lines) {
        if (y + metrics.debug_font_size > bottom) {
            break;
        }
        DrawTextEx(font, line.c_str(), Vector2{x, y}, metrics.debug_font_size, spacing, text_color);
        y += metrics.debug_line_gap;
    }
}

void DrawExitDialog(const ConfirmDialogState& state, const UiFontSet& fonts, const UiLabels& labels, const UiLayoutCache& layout)
{
    const UiMetrics& metrics = layout.metrics;
    const ConfirmDialogLayout& dialog = layout.exit_dialog;

    DrawRectangle(0, 0, metrics.window_width, metrics.window_height, kModalDim);
    DrawRectangleRounded(dialog.panel, 0.08F, 12, kPanel);
    DrawRectangleRoundedLinesEx(dialog.panel, 0.08F, 12, metrics.modal_border_width, kPanelBorder);

    DrawTextCentered(
        fonts.text,
        labels.dialog_exit_title,
        dialog.title_y,
        metrics.dialog_title_font_size,
        FontSpacing(metrics.dialog_title_font_size),
        kText,
        metrics.window_width);

    float message_y = dialog.message_y;
    for (const auto& line : dialog.message_lines) {
        DrawTextCentered(
            fonts.text,
            line,
            message_y,
            metrics.dialog_text_font_size,
            FontSpacing(metrics.dialog_text_font_size),
            kMutedText,
            metrics.window_width);
        message_y += metrics.dialog_text_font_size + 8.0F * metrics.scale;
    }

    const std::array<std::pair<Rectangle, DialogChoice>, 2> button_data{{
        {dialog.buttons.yes, DialogChoice::kYes},
        {dialog.buttons.no, DialogChoice::kNo},
    }};

    for (const auto& [rect, choice] : button_data) {
        const bool selected = state.selected_choice == choice;
        DrawRectangleRounded(rect, 0.16F, 8, selected ? Color{66, 60, 42, 245} : Color{38, 42, 54, 245});
        DrawRectangleRoundedLinesEx(rect, 0.16F, 8, metrics.modal_border_width, selected ? kAccent : kPanelBorder);

        const std::string& label = choice == DialogChoice::kYes ? labels.dialog_yes : labels.dialog_no;
        const float label_size = metrics.dialog_button_font_size;
        const float spacing = FontSpacing(label_size);
        const Vector2 measured = Measure(fonts.text, label, label_size, spacing);
        DrawTextEx(
            fonts.text,
            label.c_str(),
            Vector2{rect.x + (rect.width - measured.x) * 0.5F, rect.y + (rect.height - measured.y) * 0.5F},
            label_size,
            spacing,
            selected ? kSelectedText : kText);
    }
}

}  // namespace vox3d
