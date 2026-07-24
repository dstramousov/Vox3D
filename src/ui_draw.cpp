#include "ui_draw.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
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
constexpr Color kHelpHotkey{255, 170, 72, 255};

constexpr Color kEditorBackground{7, 118, 151, 255};
constexpr Color kEditorViewport{152, 152, 149, 255};
constexpr Color kEditorViewportText{244, 244, 238, 255};
constexpr Color kEditorPanelText{155, 203, 218, 255};
constexpr Color kEditorHotkeyText{226, 58, 48, 255};
constexpr Color kEditorTooltipBackground{10, 18, 24, 242};
constexpr Color kEditorTooltipBorder{225, 232, 238, 255};
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

[[nodiscard]] std::string ColorModeLabel(WorkspaceColorMode mode)
{
    switch (mode) {
        case WorkspaceColorMode::kTraversal:
            return "traversal";
        case WorkspaceColorMode::kGeographic:
            return "geographic";
        case WorkspaceColorMode::kChunkId:
            return "chunk";
        case WorkspaceColorMode::kFaceType:
            return "face type";
    }
    return "unknown";
}

[[nodiscard]] std::string VisibilityModeLabel(WorkspaceVisibilityMode mode)
{
    switch (mode) {
        case WorkspaceVisibilityMode::kAllChunks:
            return "all";
        case WorkspaceVisibilityMode::kRadiusFade:
            return "radius fade";
        case WorkspaceVisibilityMode::kHardCull:
            return "hard cull";
        case WorkspaceVisibilityMode::kFrustumCull:
            return "frustum cull";
    }
    return "unknown";
}

[[nodiscard]] std::string ValidationModeLabel(WorkspaceValidationMode mode)
{
    switch (mode) {
        case WorkspaceValidationMode::kOff:
            return "off";
        case WorkspaceValidationMode::kManual:
            return "manual";
        case WorkspaceValidationMode::kOnLoad:
            return "on load";
    }
    return "unknown";
}

[[nodiscard]] std::string ValidationStatusLabel(WorkspaceValidationStatus status)
{
    switch (status) {
        case WorkspaceValidationStatus::kDisabled:
            return "disabled";
        case WorkspaceValidationStatus::kNotRun:
            return "not run";
        case WorkspaceValidationStatus::kDone:
            return "done";
    }
    return "unknown";
}

[[nodiscard]] std::string PathProfileLabel(PathProfile profile)
{
    switch (profile) {
        case PathProfile::kShortest:
            return "shortest";
        case PathProfile::kSafe:
            return "safe";
    }
    return "unknown";
}

[[nodiscard]] std::string PathStatusLabel(PathProbeStatus status)
{
    switch (status) {
        case PathProbeStatus::kNotRun:
            return "not run";
        case PathProbeStatus::kFound:
            return "found";
        case PathProbeStatus::kNotFound:
            return "not found";
        case PathProbeStatus::kInvalidRequest:
            return "invalid";
    }
    return "unknown";
}

[[nodiscard]] std::string MillisecondsText(double value)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << value << " ms";
    return out.str();
}

[[nodiscard]] std::string CompactVector(Vector3 value)
{
    return CompactFloat(value.x) + "," + CompactFloat(value.y) + "," + CompactFloat(value.z);
}

[[nodiscard]] std::string MapStatusLabel(const MapPackageInfo& map, const UiLabels& labels);
[[nodiscard]] std::string MapSizeText(const MapPackageInfo& map, const UiLabels& labels);
[[nodiscard]] std::string MapLevelsText(const MapPackageInfo& map, const UiLabels& labels);
[[nodiscard]] std::string MapTileText(const MapPackageInfo& map, const UiLabels& labels);
[[nodiscard]] std::string TileCoordText(TileCoord tile);

[[nodiscard]] std::string PathPickStatusText(const WorkspaceState& workspace_state)
{
    switch (workspace_state.path_pick_mode) {
        case WorkspacePathPickMode::kSelect:
            break;
        case WorkspacePathPickMode::kPickStart:
            return "Path pick: select START";
        case WorkspacePathPickMode::kPickGoal:
            return "Path pick: select GOAL";
    }

    if (workspace_state.path_probe.HasPath()) {
        return "Path found";
    }
    if (workspace_state.path_probe.IsValid()) {
        return "Path " + PathStatusLabel(workspace_state.path_probe.status);
    }
    if (workspace_state.has_path_start || workspace_state.has_path_goal) {
        return "Path endpoints";
    }
    return "Path idle";
}

[[nodiscard]] std::string PathStatusCompactText(const WorkspaceState& workspace_state)
{
    if (workspace_state.path_pick_mode != WorkspacePathPickMode::kSelect) {
        return PathPickStatusText(workspace_state);
    }
    if (workspace_state.path_probe.HasPath()) {
        return "Path " + std::to_string(workspace_state.path_probe.path.size())
            + " / cost " + CompactFloat(static_cast<float>(workspace_state.path_probe.cost.total));
    }
    if (workspace_state.path_probe.IsValid()) {
        return "Path " + PathStatusLabel(workspace_state.path_probe.status);
    }
    if (workspace_state.has_path_start || workspace_state.has_path_goal) {
        return "Path endpoints";
    }
    return "Path none";
}

[[nodiscard]] std::string ValidationStatusCompactText(const WorkspaceState& workspace_state)
{
    if (workspace_state.passability_validation_status == WorkspaceValidationStatus::kDone
        && workspace_state.passability_validation.IsValid()) {
        return "Val issues " + std::to_string(workspace_state.passability_validation.stats.stored_issues)
            + " / " + MillisecondsText(workspace_state.passability_validation_last_run_ms);
    }
    return "Val " + ValidationStatusLabel(workspace_state.passability_validation_status);
}

[[nodiscard]] std::string_view Map2DBaseLayerLabel(Map2DBaseLayer layer)
{
    switch (layer) {
        case Map2DBaseLayer::kTerrain:
            return "Terrain";
        case Map2DBaseLayer::kElevation:
            return "Elevation";
        case Map2DBaseLayer::kStructureHeight:
            return "Ruin Height";
        case Map2DBaseLayer::kCollision:
            return "Collision";
        case Map2DBaseLayer::kMovementCost:
            return "Movement";
        case Map2DBaseLayer::kProjectileBlock:
            return "Projectile";
        case Map2DBaseLayer::kVisionBlock:
            return "Vision";
        case Map2DBaseLayer::kCover:
            return "Cover";
        case Map2DBaseLayer::kConcealment:
            return "Concealment";
    }
    return "Unknown";
}

[[nodiscard]] int PercentFromByte(std::uint8_t value)
{
    return (static_cast<int>(value) * 100 + 127) / 255;
}

[[nodiscard]] std::string Map2DCellValueText(
    const RuntimeMap& map,
    Map2DBaseLayer layer,
    TileCoord tile)
{
    if (!map.IsValid() || tile.x < 0 || tile.y < 0
        || tile.x >= map.info.width || tile.y >= map.info.height) {
        return "unavailable";
    }
    const std::size_t index = static_cast<std::size_t>(tile.y)
        * static_cast<std::size_t>(map.info.width)
        + static_cast<std::size_t>(tile.x);

    switch (layer) {
        case Map2DBaseLayer::kTerrain:
            return map.terrain.IsValid() ? map.terrain.cells[index] : "unavailable";
        case Map2DBaseLayer::kElevation:
            return map.height.IsValid() ? std::to_string(map.height.cells[index]) : "unavailable";
        case Map2DBaseLayer::kStructureHeight:
            return map.info.structure_height_loaded && map.structure_height.IsValid()
                ? std::to_string(static_cast<int>(map.structure_height.cells[index]))
                : "unavailable";
        case Map2DBaseLayer::kCollision:
            return map.collision.IsValid()
                ? (map.collision.cells[index] == 0U ? "passable" : "blocked")
                : "unavailable";
        case Map2DBaseLayer::kMovementCost:
            if (!map.movement_cost.IsValid()) {
                return "unavailable";
            }
            return map.movement_cost.cells[index] < 0
                ? "blocked"
                : "cost " + std::to_string(map.movement_cost.cells[index]);
        case Map2DBaseLayer::kProjectileBlock:
            return map.projectile_block.IsValid()
                ? (map.projectile_block.cells[index] == 0U ? "clear" : "blocked")
                : "unavailable";
        case Map2DBaseLayer::kVisionBlock:
            return map.vision_block.IsValid()
                ? (map.vision_block.cells[index] == 0U ? "clear" : "blocked")
                : "unavailable";
        case Map2DBaseLayer::kCover:
            return map.cover.IsValid()
                ? std::to_string(PercentFromByte(map.cover.cells[index])) + "%"
                : "unavailable";
        case Map2DBaseLayer::kConcealment:
            return map.concealment.IsValid()
                ? std::to_string(PercentFromByte(map.concealment.cells[index])) + "%"
                : "unavailable";
    }
    return "unavailable";
}

void DrawMap2DLegend(
    Font font,
    Rectangle viewport,
    Map2DBaseLayer layer,
    float font_size)
{
    const std::span<const Map2DLegendEntry> entries = Map2DLegendFor(layer);
    if (entries.empty() || viewport.width < 180.0F || viewport.height < 140.0F) {
        return;
    }

    const float spacing = FontSpacing(font_size);
    const float padding = std::max(7.0F, font_size * 0.45F);
    const float swatch = std::max(10.0F, font_size * 0.72F);
    const float row_height = std::max(swatch, font_size) + 4.0F;
    const std::string title(Map2DBaseLayerLabel(layer));

    float text_width = Measure(font, title, font_size, spacing).x;
    for (const Map2DLegendEntry& entry : entries) {
        text_width = std::max(
            text_width,
            Measure(font, std::string(entry.label), font_size, spacing).x);
    }

    const float width = padding * 3.0F + swatch + text_width;
    const float height = padding * 2.0F + row_height * static_cast<float>(entries.size() + 1U);
    const Rectangle panel{
        viewport.x + 12.0F,
        viewport.y + 12.0F,
        width,
        height,
    };
    DrawRectangleRec(panel, Color{10, 18, 24, 220});
    DrawRectangleLinesEx(panel, 1.0F, Color{225, 232, 238, 230});

    float y = panel.y + padding;
    DrawTextEx(
        font,
        title.c_str(),
        Vector2{panel.x + padding, y},
        font_size,
        spacing,
        kSelectedText);
    y += row_height;
    for (const Map2DLegendEntry& entry : entries) {
        const Rectangle swatch_bounds{
            panel.x + padding,
            y + (row_height - swatch) * 0.5F,
            swatch,
            swatch,
        };
        DrawRectangleRec(swatch_bounds, entry.color);
        DrawRectangleLinesEx(swatch_bounds, 1.0F, Color{230, 230, 225, 210});
        DrawTextEx(
            font,
            std::string(entry.label).c_str(),
            Vector2{panel.x + padding * 2.0F + swatch, y + 1.0F},
            font_size,
            spacing,
            kText);
        y += row_height;
    }
}

[[nodiscard]] std::string CompactStatusText(
    const WorkspaceState& workspace_state,
    Map2DViewStatus map_2d_status,
    const UiLabels& labels)
{
    const std::string preview_mode = workspace_state.show_3d_preview ? "3D" : "2D";
    if (!workspace_state.show_3d_preview) {
        std::string status = labels.workspace_status_ready + " | 2D | Layer "
            + std::string(Map2DBaseLayerLabel(workspace_state.map_2d_base_layer));
        if (map_2d_status.loaded && map_2d_status.initialized) {
            const float zoom_ratio = map_2d_status.fit_pixels_per_tile > 0.0001F
                ? map_2d_status.pixels_per_tile / map_2d_status.fit_pixels_per_tile
                : 1.0F;
            status += " | Zoom " + CompactFloat(zoom_ratio) + "x";
        }
        if (map_2d_status.hover_tile_valid) {
            status += " | Tile " + TileCoordText(map_2d_status.hover_tile);
            status += " | Value " + Map2DCellValueText(
                workspace_state.runtime_map,
                workspace_state.map_2d_base_layer,
                map_2d_status.hover_tile);
        } else {
            status += " | Tile none";
        }
        if (workspace_state.runtime_map.IsValid()) {
            status += " | Map " + std::to_string(workspace_state.runtime_map.info.width) + "x"
                + std::to_string(workspace_state.runtime_map.info.height);
        }
        return status;
    }
    if (!workspace_state.chunk_meshes.IsValid()) {
        return labels.workspace_status_ready + " | " + preview_mode + " | " + MapStatusLabel(workspace_state.map, labels);
    }
    if (workspace_state.show_3d_preview && workspace_state.visibility_stats.resident_chunks > 0) {
        return labels.workspace_status_ready + " | " + preview_mode + " | " + MeshModeLabel(workspace_state.mesh_mode)
            + " | " + ColorModeLabel(workspace_state.color_mode)
            + " | " + VisibilityModeLabel(workspace_state.visibility_mode)
            + " | Visible " + std::to_string(workspace_state.visibility_stats.visible_chunks) + "/"
            + std::to_string(workspace_state.visibility_stats.resident_chunks)
            + " | Resident " + std::to_string(workspace_state.progressive_chunks_built) + "/"
            + std::to_string(workspace_state.progressive_chunks_total)
            + " | Drawn " + std::to_string(workspace_state.visibility_stats.drawn_models)
            + " | Faces " + std::to_string(workspace_state.visibility_stats.drawn_faces)
            + " | " + PathStatusCompactText(workspace_state)
            + " | " + ValidationStatusCompactText(workspace_state);
    }

    return labels.workspace_status_ready + " | " + preview_mode + " | " + MeshModeLabel(workspace_state.mesh_mode)
        + " | " + ColorModeLabel(workspace_state.color_mode)
        + " | Chunk " + std::to_string(workspace_state.chunk_size_tiles)
        + " | Faces " + std::to_string(workspace_state.mesh_stats.active_faces)
        + " | Models " + std::to_string(workspace_state.mesh_stats.draw_models)
        + " | Resident " + std::to_string(workspace_state.progressive_chunks_built) + "/"
        + std::to_string(workspace_state.progressive_chunks_total)
        + " | Saved " + CompactPercent(workspace_state.mesh_stats.ActiveReductionRatio())
        + " | " + PathStatusCompactText(workspace_state)
        + " | " + ValidationStatusCompactText(workspace_state);
}

void PushMapStats(std::vector<std::string>& lines, const WorkspaceState& workspace_state, const UiLabels& labels)
{
    lines.push_back("Map");
    lines.push_back("  Status: " + MapStatusLabel(workspace_state.map, labels));
    lines.push_back("  Package: " + (workspace_state.map.configured ? workspace_state.map.path.filename().string() : labels.debug_none));
    lines.push_back("  Size: " + MapSizeText(workspace_state.map, labels));
    lines.push_back("  Tile: " + MapTileText(workspace_state.map, labels));
    lines.push_back("  Levels: " + MapLevelsText(workspace_state.map, labels));
    lines.push_back("  Object markers: " + std::to_string(workspace_state.runtime_map.info.object_markers));
    lines.push_back("");
}

void PushVisibilityStats(std::vector<std::string>& lines, const WorkspaceState& workspace_state)
{
    lines.push_back("Visibility");
    lines.push_back("  Mode: " + VisibilityModeLabel(workspace_state.visibility_mode));
    lines.push_back("  Radius/Fade: " + std::to_string(workspace_state.visibility_radius_chunks) + "/"
        + std::to_string(workspace_state.visibility_fade_ring_chunks));
    if (workspace_state.visibility_stats.resident_chunks == 0) {
        lines.push_back("  unavailable");
        lines.push_back("");
        return;
    }

    lines.push_back("  Resident: " + std::to_string(workspace_state.visibility_stats.resident_chunks));
    lines.push_back("  Visible/Fade: " + std::to_string(workspace_state.visibility_stats.visible_chunks) + "/"
        + std::to_string(workspace_state.visibility_stats.fade_chunks));
    lines.push_back("  Hidden: " + std::to_string(workspace_state.visibility_stats.hidden_chunks));
    lines.push_back("  Models: " + std::to_string(workspace_state.visibility_stats.drawn_models) + "/"
        + std::to_string(workspace_state.visibility_stats.resident_models));
    lines.push_back("  Faces: " + std::to_string(workspace_state.visibility_stats.drawn_faces) + "/"
        + std::to_string(workspace_state.visibility_stats.total_faces));
    lines.push_back("  Draw saved: " + CompactPercent(workspace_state.visibility_stats.DrawSavedRatio()));
    lines.push_back("  Face saved: " + CompactPercent(workspace_state.visibility_stats.FaceSavedRatio()));
    lines.push_back("");
}

void PushTransitionStats(std::vector<std::string>& lines, const WorkspaceState& workspace_state)
{
    lines.push_back("Transitions");
    if (!workspace_state.transition_features.IsValid()) {
        lines.push_back("  unavailable");
        lines.push_back("");
        return;
    }

    const TransitionFeatureStats& stats = workspace_state.transition_features.stats;
    lines.push_back("  Total: " + std::to_string(stats.total));
    lines.push_back("  R/S/B/D: " + std::to_string(stats.ramps) + "/"
        + std::to_string(stats.stairs) + "/"
        + std::to_string(stats.bridges) + "/"
        + std::to_string(stats.drops));
    lines.push_back("  Passable: " + std::to_string(stats.passable));
    lines.push_back("  Blocked: " + std::to_string(stats.blocked));
    lines.push_back("  Overlay: " + std::string(workspace_state.show_transition_overlay ? "on" : "off"));
    lines.push_back("  Shown: "
        + std::string(workspace_state.show_transition_ramps ? "R" : "-")
        + std::string(workspace_state.show_transition_stairs ? "S" : "-")
        + std::string(workspace_state.show_transition_bridges ? "B" : "-")
        + std::string(workspace_state.show_transition_drops ? "D" : "-"));
    lines.push_back("");
}

void PushMovementStats(std::vector<std::string>& lines, const WorkspaceState& workspace_state)
{
    lines.push_back("Movement");
    if (!workspace_state.movement_probe.IsValid()) {
        lines.push_back("  select a tile");
        lines.push_back("");
        return;
    }

    const MovementProbeResult& probe = workspace_state.movement_probe;
    lines.push_back("  Tile: " + TileCoordText(probe.source_tile));
    lines.push_back("  Checked: " + std::to_string(probe.stats.checked));
    lines.push_back("  Passable: " + std::to_string(probe.stats.passable));
    lines.push_back("  Blocked: " + std::to_string(probe.stats.blocked));
    lines.push_back("  Overlay: " + std::string(workspace_state.show_movement_probe ? "on" : "off"));
    lines.push_back("");
}

void PushPathStats(std::vector<std::string>& lines, const WorkspaceState& workspace_state)
{
    lines.push_back("Path Probe");
    lines.push_back("  Profile: " + PathProfileLabel(workspace_state.path_profile));
    lines.push_back("  Tool: " + std::string(ToString(workspace_state.path_pick_mode)));
    lines.push_back("  Start: " + (workspace_state.has_path_start ? TileCoordText(workspace_state.path_start) : std::string("none")));
    lines.push_back("  Goal: " + (workspace_state.has_path_goal ? TileCoordText(workspace_state.path_goal) : std::string("none")));
    if (!workspace_state.path_probe.IsValid()) {
        lines.push_back("  Status: not run");
        lines.push_back("");
        return;
    }

    const PathProbeResult& path = workspace_state.path_probe;
    lines.push_back("  Status: " + PathStatusLabel(path.status));
    lines.push_back("  Steps: " + std::to_string(path.path.size()));
    lines.push_back("  Cost: " + CompactFloat(static_cast<float>(path.cost.total)));
    lines.push_back("  Visited: " + std::to_string(path.stats.visited_nodes));
    lines.push_back("  Expanded: " + std::to_string(path.stats.expanded_nodes));
    lines.push_back("  Blocked edges: " + std::to_string(path.stats.blocked_edges));
    lines.push_back("  Cost B/T/E/Tr/S: "
        + CompactFloat(static_cast<float>(path.cost.base)) + "/"
        + CompactFloat(static_cast<float>(path.cost.terrain)) + "/"
        + CompactFloat(static_cast<float>(path.cost.elevation)) + "/"
        + CompactFloat(static_cast<float>(path.cost.transition)) + "/"
        + CompactFloat(static_cast<float>(path.cost.safety)));
    lines.push_back("  Overlay: " + std::string(workspace_state.show_path_overlay ? "on" : "off")
        + ", visited " + std::string(workspace_state.show_path_visited ? "on" : "off"));
    if (path.stats.visited_storage_truncated) {
        lines.push_back("  Visited storage: truncated");
    }
    lines.push_back("");
}

void PushPassabilityStats(std::vector<std::string>& lines, const WorkspaceState& workspace_state)
{
    lines.push_back("Passability Validation");
    lines.push_back("  Mode: " + ValidationModeLabel(workspace_state.validation_mode));
    lines.push_back("  Status: " + ValidationStatusLabel(workspace_state.passability_validation_status));
    if (workspace_state.passability_validation_last_run_ms > 0.0) {
        lines.push_back("  Last run: " + MillisecondsText(workspace_state.passability_validation_last_run_ms));
    }
    if (!workspace_state.passability_validation.IsValid()) {
        lines.push_back("  Report: not available");
        lines.push_back("");
        return;
    }

    const PassabilityValidationStats& stats = workspace_state.passability_validation.stats;
    lines.push_back("  Edges: " + std::to_string(stats.checked_edges));
    lines.push_back("  Pass/Block: " + std::to_string(stats.passable_edges) + "/"
        + std::to_string(stats.blocked_edges));
    lines.push_back("  Invalid: " + std::to_string(stats.invalid_transitions));
    lines.push_back("  Drops: " + std::to_string(stats.suspicious_drops));
    lines.push_back("  Blocked R/S: " + std::to_string(stats.blocked_ramps) + "/"
        + std::to_string(stats.blocked_stairs));
    lines.push_back("  Isolated: " + std::to_string(stats.isolated_tiles));
    lines.push_back("  Stored issues: " + std::to_string(stats.stored_issues)
        + std::string(stats.issue_storage_truncated ? " (truncated)" : ""));
    lines.push_back("  Overlay: " + std::string(workspace_state.show_passability_issues ? "on" : "off"));
    lines.push_back("");
}

void PushMeshStats(std::vector<std::string>& lines, const WorkspaceState& workspace_state)
{
    lines.push_back("Mesh");
    if (!workspace_state.chunk_meshes.IsValid()) {
        lines.push_back("  unavailable");
        lines.push_back("");
        return;
    }

    lines.push_back("  Mode: " + MeshModeLabel(workspace_state.mesh_mode));
    lines.push_back("  Chunk: " + std::to_string(workspace_state.chunk_size_tiles));
    lines.push_back("  Chunks: " + std::to_string(workspace_state.chunk_grid.info.chunks_x) + "x"
        + std::to_string(workspace_state.chunk_grid.info.chunks_y) + " = "
        + std::to_string(workspace_state.chunk_grid.info.total_chunks));
    lines.push_back("  Faces: " + std::to_string(workspace_state.mesh_stats.active_faces));
    lines.push_back("  Models: " + std::to_string(workspace_state.mesh_stats.draw_models));
    lines.push_back("  Saved: " + CompactPercent(workspace_state.mesh_stats.ActiveReductionRatio()));
    lines.push_back("");

    lines.push_back("Comparison");
    lines.push_back("  Simple: " + std::to_string(workspace_state.mesh_stats.simple_faces));
    lines.push_back("  Greedy: " + std::to_string(workspace_state.mesh_stats.greedy_faces));
    lines.push_back("  Terrain raw: "
        + std::to_string(workspace_state.mesh_stats.terrain_raw_top_faces
            + workspace_state.mesh_stats.terrain_raw_wall_faces));
    lines.push_back("  Terrain merged: " + std::to_string(workspace_state.mesh_stats.terrain_faces));
    lines.push_back("  Raw T/W: " + std::to_string(workspace_state.mesh_stats.terrain_raw_top_faces) + "/"
        + std::to_string(workspace_state.mesh_stats.terrain_raw_wall_faces));
    lines.push_back("  Merged T/W/C: " + std::to_string(workspace_state.mesh_stats.terrain_top_faces) + "/"
        + std::to_string(workspace_state.mesh_stats.terrain_wall_faces) + "/"
        + std::to_string(workspace_state.mesh_stats.terrain_cliff_faces));
    lines.push_back("  Passes: "
        + std::string(workspace_state.show_terrain_tops ? "T" : "-")
        + std::string(workspace_state.show_terrain_walls ? "W" : "-")
        + std::string(workspace_state.show_terrain_cliffs ? "C" : "-"));
    lines.push_back("  Terrain merge: " + CompactPercent(workspace_state.mesh_stats.TerrainMergeReductionRatio()));
    lines.push_back("  Terrain vs greedy: " + CompactSignedPercent(workspace_state.mesh_stats.TerrainVsGreedyDeltaRatio()));
    if (workspace_state.chunk_size_comparison.available) {
        lines.push_back("");
        lines.push_back("Chunk Profit");
        lines.push_back("  Models: " + CompactSignedPercent(workspace_state.chunk_size_comparison.DrawModelDeltaRatio()));
        lines.push_back("  Faces: " + CompactSignedPercent(workspace_state.chunk_size_comparison.FaceDeltaRatio()));
    }
    lines.push_back("");
}

void PushDirtyStats(std::vector<std::string>& lines, const WorkspaceState& workspace_state)
{
    lines.push_back("Dirty Cache");
    if (!workspace_state.chunk_mesh_cache.IsValid() && !workspace_state.last_mesh_rebuild.attempted) {
        lines.push_back("  unavailable for terrain mode");
        return;
    }
    if (workspace_state.chunk_mesh_cache.IsValid()) {
        lines.push_back("  Dirty: " + std::to_string(workspace_state.chunk_mesh_cache.info.dirty_chunks));
    }
    if (workspace_state.last_mesh_rebuild.attempted) {
        lines.push_back("  Rebuilt: " + std::to_string(workspace_state.last_mesh_rebuild.rebuilt_chunks) + "/"
            + std::to_string(workspace_state.last_mesh_rebuild.total_chunks));
        lines.push_back("  Reused: " + std::to_string(workspace_state.last_mesh_rebuild.reused_chunks));
        lines.push_back("  Saved work: " + CompactPercent(workspace_state.last_mesh_rebuild.SavedRebuildWorkRatio()));
    }
}

[[nodiscard]] std::vector<std::string> BuildStatsPanelLines(
    const WorkspaceState& workspace_state,
    FreeFlyCameraStatus camera_status,
    const UiLabels& labels)
{
    std::vector<std::string> lines;
    PushMapStats(lines, workspace_state, labels);
    lines.push_back("Color");
    lines.push_back("  Mode: " + ColorModeLabel(workspace_state.color_mode));
    lines.push_back("  Geographic bands: -5..20");
    lines.push_back("  Legend: blue low, green mid");
    lines.push_back("          yellow/brown high, white peak");
    lines.push_back("");
    PushVisibilityStats(lines, workspace_state);
    PushTransitionStats(lines, workspace_state);
    PushMovementStats(lines, workspace_state);
    PushPathStats(lines, workspace_state);
    PushPassabilityStats(lines, workspace_state);
    PushMeshStats(lines, workspace_state);
    PushDirtyStats(lines, workspace_state);
    if (workspace_state.show_3d_preview && camera_status.initialized) {
        lines.push_back("");
        lines.push_back("Camera");
        lines.push_back(std::string("  State: ") + (camera_status.cursor_captured ? "captured" : "free"));
        lines.push_back("  Yaw/Pitch: " + CompactFloat(camera_status.yaw_degrees) + "/" + CompactFloat(camera_status.pitch_degrees));
        lines.push_back("  Pos: " + CompactVector(camera_status.position));
    }
    return lines;
}

enum class TextPanelRowKind {
    kBlank,
    kGroup,
    kValue,
};

struct TextPanelRow {
    TextPanelRowKind kind = TextPanelRowKind::kBlank;
    WorkspacePanelItem item = WorkspacePanelItem::kStatsMapGroup;
    int depth = 0;
    std::string label;
    bool checked = false;
};

[[nodiscard]] WorkspacePanelItem StatsGroupItemForLabel(std::string_view label)
{
    if (label == "Map") {
        return WorkspacePanelItem::kStatsMapGroup;
    }
    if (label == "Color") {
        return WorkspacePanelItem::kStatsColorGroup;
    }
    if (label == "Visibility") {
        return WorkspacePanelItem::kStatsVisibilityGroup;
    }
    if (label == "Transitions") {
        return WorkspacePanelItem::kStatsTransitionsGroup;
    }
    if (label == "Movement") {
        return WorkspacePanelItem::kStatsMovementGroup;
    }
    if (label == "Path Probe") {
        return WorkspacePanelItem::kStatsPathGroup;
    }
    if (label == "Passability Validation") {
        return WorkspacePanelItem::kStatsPassabilityGroup;
    }
    if (label == "Mesh") {
        return WorkspacePanelItem::kStatsMeshGroup;
    }
    if (label == "Comparison") {
        return WorkspacePanelItem::kStatsComparisonGroup;
    }
    if (label == "Chunk Profit") {
        return WorkspacePanelItem::kStatsChunkProfitGroup;
    }
    if (label == "Dirty Cache") {
        return WorkspacePanelItem::kStatsDirtyCacheGroup;
    }
    if (label == "Camera") {
        return WorkspacePanelItem::kStatsCameraGroup;
    }
    return WorkspacePanelItem::kStatsMapGroup;
}

[[nodiscard]] std::string TrimTextPanelIndent(std::string_view line)
{
    const std::size_t first = line.find_first_not_of(' ');
    if (first == std::string_view::npos) {
        return {};
    }
    return std::string(line.substr(first));
}

[[nodiscard]] std::vector<TextPanelRow> BuildTextPanelRowsFromLines(
    const std::vector<std::string>& lines,
    const WorkspaceState& workspace_state,
    WorkspacePanelItem fallback_group,
    WorkspacePanelItem (*group_item_for_label)(std::string_view))
{
    std::vector<TextPanelRow> rows;
    rows.reserve(lines.size());

    WorkspacePanelItem current_group = fallback_group;
    bool current_group_expanded = true;

    for (const std::string& line : lines) {
        if (line.empty()) {
            if (current_group_expanded) {
                rows.push_back(TextPanelRow{TextPanelRowKind::kBlank, current_group, 0, {}, false});
            }
            continue;
        }

        const bool value_row = line.size() >= 2 && line[0] == ' ' && line[1] == ' ';
        if (!value_row) {
            current_group = group_item_for_label(line);
            current_group_expanded = !IsWorkspacePanelGroupCollapsed(workspace_state, current_group);
            rows.push_back(TextPanelRow{
                TextPanelRowKind::kGroup,
                current_group,
                0,
                line,
                current_group_expanded,
            });
            continue;
        }

        if (!current_group_expanded) {
            continue;
        }

        rows.push_back(TextPanelRow{
            TextPanelRowKind::kValue,
            current_group,
            1,
            TrimTextPanelIndent(line),
            false,
        });
    }

    return rows;
}

[[nodiscard]] std::vector<TextPanelRow> BuildStatsTextPanelRows(
    const WorkspaceState& workspace_state,
    FreeFlyCameraStatus camera_status,
    const UiLabels& labels)
{
    return BuildTextPanelRowsFromLines(
        BuildStatsPanelLines(workspace_state, camera_status, labels),
        workspace_state,
        WorkspacePanelItem::kStatsMapGroup,
        StatsGroupItemForLabel);
}

[[nodiscard]] std::string TileCoordText(TileCoord tile)
{
    return std::to_string(tile.x) + "," + std::to_string(tile.y);
}

[[nodiscard]] std::string ChunkBoundsText(const TileBounds& bounds)
{
    return std::to_string(bounds.min_x) + "," + std::to_string(bounds.min_y)
        + ".." + std::to_string(bounds.max_x) + "," + std::to_string(bounds.max_y);
}

[[nodiscard]] std::string MovementDirectionLabel(FaceDirection direction)
{
    switch (direction) {
        case FaceDirection::kNorth:
            return "N";
        case FaceDirection::kSouth:
            return "S";
        case FaceDirection::kWest:
            return "W";
        case FaceDirection::kEast:
            return "E";
        case FaceDirection::kDown:
            return "D";
        case FaceDirection::kUp:
            return "U";
    }
    return "?";
}

[[nodiscard]] std::string MovementTransitionLabel(const MovementProbeStep& step)
{
    if (!step.has_transition) {
        return "flat";
    }
    return std::string(ToString(step.transition_kind));
}

[[nodiscard]] std::string MovementBlockLabel(MovementBlockReason reason)
{
    switch (reason) {
        case MovementBlockReason::kNone:
            return "ok";
        case MovementBlockReason::kMissingData:
            return "missing";
        case MovementBlockReason::kSourceBlocked:
            return "source blocked";
        case MovementBlockReason::kOutOfBounds:
            return "outside";
        case MovementBlockReason::kTargetBlocked:
            return "target blocked";
        case MovementBlockReason::kDrop:
            return "drop";
        case MovementBlockReason::kTransitionBlocked:
            return "transition blocked";
    }
    return "unknown";
}

[[nodiscard]] std::string MovementStepText(const MovementProbeStep& step)
{
    std::string text = "  " + MovementDirectionLabel(step.direction) + ": ";
    text += step.passable ? "pass" : "block";
    text += ", d" + std::to_string(step.delta_levels);
    text += ", " + MovementTransitionLabel(step);
    if (!step.passable) {
        text += ", " + MovementBlockLabel(step.block_reason);
    }
    return text;
}

[[nodiscard]] bool SameTile(TileCoord left, TileCoord right)
{
    return left.x == right.x && left.y == right.y;
}

[[nodiscard]] bool TileInList(
    TileCoord tile,
    const std::vector<TileCoord>& tiles)
{
    return std::any_of(tiles.begin(), tiles.end(), [tile](TileCoord candidate) {
        return SameTile(tile, candidate);
    });
}

[[nodiscard]] bool ObjectTouchesTile(
    const RuntimeMapObject& object,
    TileCoord tile)
{
    return SameTile(object.anchor, tile)
        || TileInList(tile, object.footprint)
        || TileInList(tile, object.collision_footprint)
        || object.visual_bounds.Contains(tile);
}

[[nodiscard]] std::vector<std::string> BuildInspectPanelLines(const WorkspaceState& workspace_state)
{
    std::vector<std::string> lines;
    lines.push_back("Selection");
    if (!workspace_state.selected_tile.IsValid()) {
        lines.push_back("  Tile: none");
        lines.push_back("  Chunk: none");
        lines.push_back("  Transitions: none");
        lines.push_back("");
        lines.push_back("  Click the 2D map or 3D terrain");
        lines.push_back("  to inspect a tile.");
        return lines;
    }

    const TileInspectResult& tile = workspace_state.selected_tile;
    lines.push_back("  Tile: " + TileCoordText(tile.tile));
    lines.push_back("  Terrain: " + tile.terrain);
    lines.push_back("  Elevation: " + std::to_string(tile.elevation));
    lines.push_back("  Ground top: " + std::to_string(tile.elevation + 1));
    if (tile.structure_height_available) {
        lines.push_back(
            "  Structure height: "
            + std::to_string(static_cast<int>(tile.structure_height)));
        lines.push_back(
            "  Structure top: "
            + std::to_string(
                tile.elevation + 1 + static_cast<int>(tile.structure_height)));
    }
    lines.push_back("  Collision: " + std::string(tile.blocked ? "blocked" : "free"));
    if (tile.movement_cost_available) {
        lines.push_back("  Movement cost: "
            + (tile.movement_cost < 0 ? std::string("blocked") : std::to_string(tile.movement_cost)));
    }
    if (tile.projectile_block_available) {
        lines.push_back("  Projectile: " + std::string(tile.projectile_blocked ? "blocked" : "clear"));
    }
    if (tile.vision_block_available) {
        lines.push_back("  Vision: " + std::string(tile.vision_blocked ? "blocked" : "clear"));
    }
    if (tile.cover_available) {
        lines.push_back("  Cover: " + std::to_string(PercentFromByte(tile.cover)) + "%");
    }
    if (tile.concealment_available) {
        lines.push_back("  Concealment: " + std::to_string(PercentFromByte(tile.concealment)) + "%");
    }
    if (tile.chunk_found) {
        lines.push_back("  Chunk: " + TileCoordText(TileCoord{tile.chunk.x, tile.chunk.y}));
        lines.push_back("  Chunk bounds: " + ChunkBoundsText(tile.chunk_bounds));
    } else {
        lines.push_back("  Chunk: none");
    }
    const int region_size = workspace_state.map.runtime_binary.region_size_tiles;
    if (region_size > 0) {
        const int region_x = tile.tile.x / region_size;
        const int region_y = tile.tile.y / region_size;
        const int max_x = std::min(
            workspace_state.runtime_map.info.width,
            (region_x + 1) * region_size);
        const int max_y = std::min(
            workspace_state.runtime_map.info.height,
            (region_y + 1) * region_size);
        lines.push_back("  VXMAP region: " + TileCoordText(TileCoord{region_x, region_y}));
        lines.push_back("  Region bounds: "
            + std::to_string(region_x * region_size) + ","
            + std::to_string(region_y * region_size) + "-"
            + std::to_string(max_x - 1) + ","
            + std::to_string(max_y - 1));
    } else {
        lines.push_back("  VXMAP region: unavailable");
    }
    lines.push_back("");
    lines.push_back("World data");

    int object_count = 0;
    for (const RuntimeMapObject& object : workspace_state.runtime_map.runtime_objects) {
        if (!ObjectTouchesTile(object, tile.tile)) {
            continue;
        }
        ++object_count;
        if (object_count <= 6) {
            lines.push_back("  Object: " + object.id + " [" + object.type + "]");
            lines.push_back("    Anchor: " + TileCoordText(object.anchor)
                + ", orientation: "
                + (object.orientation.empty() ? std::string("none") : object.orientation));
            lines.push_back("    Blocks M/P/V: "
                + std::string(object.blocks_movement ? "yes" : "no") + "/"
                + std::string(object.blocks_projectiles ? "yes" : "no") + "/"
                + std::string(object.blocks_vision ? "yes" : "no"));
        }
    }
    if (object_count == 0) {
        lines.push_back("  Objects: none");
    } else if (object_count > 6) {
        lines.push_back("  Objects: +" + std::to_string(object_count - 6) + " more");
    }

    int vegetation_count = 0;
    for (const RuntimeObjectMarker& marker : workspace_state.runtime_map.object_markers) {
        if (!marker.visual_only || marker.role != "vegetation"
            || !SameTile(marker.tile, tile.tile)) {
            continue;
        }
        ++vegetation_count;
        if (vegetation_count <= 6) {
            lines.push_back("  Vegetation: " + marker.type);
        }
    }
    if (vegetation_count == 0) {
        lines.push_back("  Vegetation: none");
    } else if (vegetation_count > 6) {
        lines.push_back(
            "  Vegetation: +" + std::to_string(vegetation_count - 6) + " more");
    }

    int place_count = 0;
    for (const RuntimePlace& place : workspace_state.runtime_map.places) {
        const bool inside = SameTile(place.center, tile.tile)
            || place.bounds.Contains(tile.tile);
        if (!inside) {
            continue;
        }
        ++place_count;
        if (place_count <= 4) {
            lines.push_back("  Place: " + place.id + " [" + place.type + "]");
            lines.push_back("    Center: " + TileCoordText(place.center)
                + ", radius: " + std::to_string(place.radius));
        }
    }
    if (place_count == 0) {
        lines.push_back("  Places: none");
    } else if (place_count > 4) {
        lines.push_back("  Places: +" + std::to_string(place_count - 4) + " more");
    }

    int marker_count = 0;
    for (const RuntimeMapMarker& marker : workspace_state.runtime_map.markers) {
        if (!SameTile(marker.tile, tile.tile)) {
            continue;
        }
        ++marker_count;
        if (marker_count <= 6) {
            lines.push_back("  Marker: " + marker.id + " [" + marker.type + "]");
        }
    }
    if (marker_count == 0) {
        lines.push_back("  Markers: none");
    } else if (marker_count > 6) {
        lines.push_back("  Markers: +" + std::to_string(marker_count - 6) + " more");
    }

    lines.push_back("");
    lines.push_back("Transitions");
    lines.push_back("  Total: " + std::to_string(tile.transitions.total));
    lines.push_back("  R/S/B/D: " + std::to_string(tile.transitions.ramps) + "/"
        + std::to_string(tile.transitions.stairs) + "/"
        + std::to_string(tile.transitions.bridges) + "/"
        + std::to_string(tile.transitions.drops));
    lines.push_back("  Passable: " + std::to_string(tile.transitions.passable));
    lines.push_back("  Blocked: " + std::to_string(tile.transitions.blocked));
    lines.push_back("");
    lines.push_back("Movement");
    if (!workspace_state.movement_probe.IsValid()) {
        lines.push_back("  unavailable");
        return lines;
    }
    const MovementProbeResult& probe = workspace_state.movement_probe;
    lines.push_back("  P/B: " + std::to_string(probe.stats.passable) + "/" + std::to_string(probe.stats.blocked));
    for (int index = 0; index < probe.step_count; ++index) {
        lines.push_back(MovementStepText(probe.steps[static_cast<std::size_t>(index)]));
    }
    lines.push_back("");
    lines.push_back("Path");
    lines.push_back("  Profile: " + PathProfileLabel(workspace_state.path_profile));
    lines.push_back("  Tool: " + std::string(ToString(workspace_state.path_pick_mode)));
    lines.push_back("  Start: " + (workspace_state.has_path_start ? TileCoordText(workspace_state.path_start) : std::string("none")));
    lines.push_back("  Goal: " + (workspace_state.has_path_goal ? TileCoordText(workspace_state.path_goal) : std::string("none")));
    if (workspace_state.path_probe.IsValid()) {
        lines.push_back("  Status: " + PathStatusLabel(workspace_state.path_probe.status));
        const int step_index = FindPathStepIndex(workspace_state.path_probe, tile.tile);
        if (step_index >= 0) {
            const PathStep& step = workspace_state.path_probe.path[static_cast<std::size_t>(step_index)];
            lines.push_back("  On path: yes " + std::to_string(step_index) + "/"
                + std::to_string(workspace_state.path_probe.path.size()));
            lines.push_back("  Step cost: " + CompactFloat(static_cast<float>(step.step_cost)));
            lines.push_back("  Acc cost: " + CompactFloat(static_cast<float>(step.accumulated_cost)));
        } else {
            lines.push_back("  On path: no");
        }
    } else {
        lines.push_back("  Status: not run");
    }
    lines.push_back("");
    lines.push_back("Validation");
    lines.push_back("  Mode: " + ValidationModeLabel(workspace_state.validation_mode));
    lines.push_back("  Status: " + ValidationStatusLabel(workspace_state.passability_validation_status));
    if (workspace_state.passability_validation.IsValid()) {
        const PassabilityValidationStats& stats = workspace_state.passability_validation.stats;
        lines.push_back("  Invalid: " + std::to_string(stats.invalid_transitions));
        lines.push_back("  Blocked R/S: " + std::to_string(stats.blocked_ramps) + "/"
            + std::to_string(stats.blocked_stairs));
        lines.push_back("  Drops: " + std::to_string(stats.suspicious_drops));
        lines.push_back("  Isolated: " + std::to_string(stats.isolated_tiles));
    } else {
        lines.push_back("  report not available");
    }
    return lines;
}

struct SelectionInfoOverlayGeometry {
    Rectangle panel;
    Rectangle content;
    float padding = 0.0F;
    float title_font_size = 0.0F;
    float body_font_size = 0.0F;
    float footer_font_size = 0.0F;
    float line_height = 0.0F;
    int visible_rows = 0;
};

[[nodiscard]] SelectionInfoOverlayGeometry BuildSelectionInfoOverlayGeometry(
    const UiLayoutCache& layout)
{
    const UiMetrics& metrics = layout.metrics;
    const float scale = metrics.scale;
    const float window_width = static_cast<float>(metrics.window_width);
    const float window_height = static_cast<float>(metrics.window_height);
    const float margin = std::max(14.0F, 24.0F * scale);
    const float max_panel_width = std::max(1.0F, window_width - margin * 2.0F);
    const float max_panel_height = std::max(1.0F, window_height - margin * 2.0F);

    SelectionInfoOverlayGeometry geometry;
    geometry.padding = std::max(12.0F, 18.0F * scale);
    geometry.title_font_size = std::max(16.0F, metrics.workspace_tab_font_size);
    geometry.body_font_size = std::max(12.0F, metrics.workspace_status_font_size);
    geometry.footer_font_size = std::max(10.0F, metrics.workspace_status_font_size - 2.0F);
    geometry.line_height = geometry.body_font_size + std::max(4.0F, 6.0F * scale);

    const float desired_width = std::max(420.0F * scale, window_width * 0.48F);
    const float desired_height = std::max(360.0F * scale, window_height * 0.70F);
    const float panel_width = std::min(desired_width, max_panel_width);
    const float panel_height = std::min(desired_height, max_panel_height);
    geometry.panel = Rectangle{
        (window_width - panel_width) * 0.5F,
        (window_height - panel_height) * 0.5F,
        panel_width,
        panel_height,
    };

    const float title_height = geometry.title_font_size + std::max(10.0F, 12.0F * scale);
    const float footer_height = geometry.footer_font_size + std::max(12.0F, 14.0F * scale);
    geometry.content = Rectangle{
        geometry.panel.x + geometry.padding,
        geometry.panel.y + geometry.padding + title_height,
        std::max(1.0F, geometry.panel.width - geometry.padding * 2.0F),
        std::max(
            1.0F,
            geometry.panel.height - geometry.padding * 2.0F - title_height - footer_height),
    };
    geometry.visible_rows = std::max(
        1,
        static_cast<int>(std::floor(geometry.content.height / geometry.line_height)));
    return geometry;
}

struct HelpControlLine
{
    std::string_view hotkey;
    std::string_view action;
};

[[nodiscard]] std::optional<HelpControlLine> ParseHelpControlLine(
    std::string_view line)
{
    if (line.size() < 2 || line.rfind("  ", 0) != 0) {
        return std::nullopt;
    }

    const std::string_view content = line.substr(2);
    const std::size_t separator = content.find("  ");
    if (separator == std::string_view::npos) {
        return HelpControlLine{content, {}};
    }

    std::size_t action_start = separator;
    while (action_start < content.size() && content[action_start] == ' ') {
        ++action_start;
    }
    return HelpControlLine{
        content.substr(0, separator),
        content.substr(action_start),
    };
}

// Keep this list synchronized with workspace input handling whenever controls change.
[[nodiscard]] std::vector<std::string> BuildHelpPanelLines()
{
    return {
        "2D controls",
        "  LMB + drag       Pan map",
        "  RMB              Select tile and open context menu",
        "  MMB              Select tile and open Info",
        "  Wheel            Zoom around cursor",
        "  + / -            Zoom in / out",
        "  F                Fit whole map",
        "  R                Reset 2D view",
        "  Home             Focus Start",
        "  End              Focus Goal",
        "  I                Selection Info",
        "  S                Statistics",
        "  F1               Help",
        "  Esc              Close overlay / exit",
        "  Wheel over menu  Scroll side panel",
        "  PgUp / PgDn      Scroll panel or overlay",
        "",
        "3D controls",
        "  LMB (free)       Select tile and capture mouse",
        "  MMB / RMB (free) Capture mouse",
        "  Mouse move       Look around while captured",
        "  W / A / S / D    Move",
        "  Q / E            Move down / up",
        "  Shift / Ctrl     Fast / slow movement",
        "  Wheel            Dolly forward / backward",
        "  RMB / F2 / Esc   Release captured mouse",
        "  F                Fit map in viewport",
        "  R                Reset camera",
        "  I                Selection Info",
        "  S (mouse free)   Statistics",
        "  F1               Help",
        "  F3 / P           Start path pick",
        "  LMB in path pick  Select Start, then Goal",
        "  RMB / Esc        Cancel path pick",
        "  X                Clear path probe",
        "  T                Toggle transitions",
        "  M                Toggle movement probe",
        "  V                Toggle passability issues",
        "  F4               Toggle chunk bounds",
        "  F5               Toggle world grid",
        "  F6               Toggle collision overlay",
        "  F7               Toggle height overlay",
        "  F8               Cycle mesh mode",
        "  F9               Toggle chunk size",
        "  F10              Run dirty rebuild probe",
        "  F11              Cycle color mode",
        "  F12              Cycle visibility mode",
        "  Esc (mouse free) Close overlay / exit",
    };
}

[[nodiscard]] std::string WorkspaceViewModeLabel(
    WorkspaceViewMode mode,
    const UiLabels& labels)
{
    switch (mode) {
        case WorkspaceViewMode::kMap2D:
            return labels.workspace_subitem_2d_map;
        case WorkspaceViewMode::kWorld3D:
            return labels.workspace_subitem_3d_preview;
    }
    return "Unknown";
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


[[nodiscard]] std::string WorkspacePanelItemLabel(WorkspacePanelItem item, const WorkspaceState& workspace_state, const UiLabels& labels)
{
    switch (item) {
        case WorkspacePanelItem::k2DBaseLayerGroup:
            return "Base Layer";
        case WorkspacePanelItem::kLayerTerrain:
            return labels.workspace_terrain_label;
        case WorkspacePanelItem::kLayerElevation:
            return labels.workspace_elevation_label;
        case WorkspacePanelItem::kLayerStructureHeight:
            return "Ruin Height";
        case WorkspacePanelItem::kLayerCollision:
            return labels.workspace_collision_label;
        case WorkspacePanelItem::kLayerMovementCost:
            return "Movement Cost";
        case WorkspacePanelItem::kLayerProjectileBlock:
            return "Projectile Block";
        case WorkspacePanelItem::kLayerVisionBlock:
            return "Vision Block";
        case WorkspacePanelItem::kLayerCover:
            return "Cover";
        case WorkspacePanelItem::kLayerConcealment:
            return "Concealment";
        case WorkspacePanelItem::k2DOverlayGroup:
            return "Overlays";
        case WorkspacePanelItem::kLayerGrid:
            return labels.workspace_subitem_grid;
        case WorkspacePanelItem::k2DChunks:
            if (workspace_state.chunk_grid.info.chunk_size_x > 0
                && workspace_state.chunk_grid.info.chunk_size_y > 0) {
                return "Chunks "
                    + std::to_string(workspace_state.chunk_grid.info.chunk_size_x)
                    + "x"
                    + std::to_string(workspace_state.chunk_grid.info.chunk_size_y);
            }
            return "Chunks";
        case WorkspacePanelItem::k2DVxmapRegions:
            if (workspace_state.map.runtime_binary.region_size_tiles > 0) {
                const int region_size = workspace_state.map.runtime_binary.region_size_tiles;
                return "VXMAP Regions " + std::to_string(region_size)
                    + "x" + std::to_string(region_size);
            }
            return "VXMAP Regions";
        case WorkspacePanelItem::k2DStartGoal:
            return "Start / Goal";
        case WorkspacePanelItem::k2DObjects:
            return labels.workspace_tool_objects + " ("
                + std::to_string(workspace_state.runtime_map.info.runtime_objects) + ")";
        case WorkspacePanelItem::k2DVegetation:
            return "Vegetation ("
                + std::to_string(workspace_state.runtime_map.info.vegetation_markers) + ")";
        case WorkspacePanelItem::k2DPlaces:
            return "Places (" + std::to_string(workspace_state.runtime_map.info.places) + ")";
        case WorkspacePanelItem::k2DMarkers:
            return "Markers (" + std::to_string(workspace_state.runtime_map.info.markers) + ")";
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
        case WorkspacePanelItem::k3DRenderGroup:
            return "Display";
        case WorkspacePanelItem::kRenderTerrainMesh:
            return "Terrain Mesh";
        case WorkspacePanelItem::k3DColorModeGroup:
            return "Color";
        case WorkspacePanelItem::k3DColorTraversal:
            return "Traversal";
        case WorkspacePanelItem::k3DColorGeographic:
            return "Geographic";
        case WorkspacePanelItem::k3DColorChunkId:
            return "Chunk Id";
        case WorkspacePanelItem::k3DColorFaceType:
            return "Face Type";
        case WorkspacePanelItem::k3DDebugOverlaysGroup:
            return "Debug Overlays";
        case WorkspacePanelItem::k3DVisibilityGroup:
            return "Visibility";
        case WorkspacePanelItem::k3DVisibilityAllChunks:
            return "All Chunks";
        case WorkspacePanelItem::k3DVisibilityRadiusFade:
            return "Radius Fade";
        case WorkspacePanelItem::k3DVisibilityHardCull:
            return "Hard Cull";
        case WorkspacePanelItem::k3DVisibilityFrustumCull:
            return "Frustum Cull";
        case WorkspacePanelItem::k3DVisibilityRadiusMinus:
            return "Radius -";
        case WorkspacePanelItem::k3DVisibilityRadiusPlus:
            return "Radius +";
        case WorkspacePanelItem::k3DVisibilityFadeMinus:
            return "Fade Ring -";
        case WorkspacePanelItem::k3DVisibilityFadePlus:
            return "Fade Ring +";
        case WorkspacePanelItem::k3DShowHiddenBounds:
            return "Hidden Bounds";
        case WorkspacePanelItem::k3DTerrainPassGroup:
            return "Terrain";
        case WorkspacePanelItem::k3DTerrainPassTops:
            return "Tops";
        case WorkspacePanelItem::k3DTerrainPassWalls:
            return "Walls";
        case WorkspacePanelItem::k3DTerrainPassCliffs:
            return "Cliffs";
        case WorkspacePanelItem::k3DTransitionGroup:
            return "Transitions";
        case WorkspacePanelItem::k3DShowTransitions:
            return "Show Transitions";
        case WorkspacePanelItem::k3DTransitionRamps:
            return "Ramps";
        case WorkspacePanelItem::k3DTransitionStairs:
            return "Stairs";
        case WorkspacePanelItem::k3DTransitionBridges:
            return "Bridges";
        case WorkspacePanelItem::k3DTransitionDrops:
            return "Drops";
        case WorkspacePanelItem::k3DMovementGroup:
            return "Probes";
        case WorkspacePanelItem::k3DShowMovementProbe:
            return "Movement Probe";
        case WorkspacePanelItem::k3DPathGroup:
            return "Path";
        case WorkspacePanelItem::k3DPathProfileShortest:
            return "Shortest";
        case WorkspacePanelItem::k3DPathProfileSafe:
            return "Safe";
        case WorkspacePanelItem::k3DPathStatusValue:
            return "Status: " + PathPickStatusText(workspace_state);
        case WorkspacePanelItem::k3DPathStartValue:
            return "Start: " + (workspace_state.has_path_start ? TileCoordText(workspace_state.path_start) : std::string("none"));
        case WorkspacePanelItem::k3DPathGoalValue:
            return "Goal: " + (workspace_state.has_path_goal ? TileCoordText(workspace_state.path_goal) : std::string("none"));
        case WorkspacePanelItem::k3DPathToolSelect:
            return "Cancel Pick";
        case WorkspacePanelItem::k3DPathToolPickStart:
            return "F3 Pick Path";
        case WorkspacePanelItem::k3DPathToolPickGoal:
            return "Pick Goal";
        case WorkspacePanelItem::k3DRunPathProbe:
            return "Run Path";
        case WorkspacePanelItem::k3DClearPathProbe:
            return "Clear Path";
        case WorkspacePanelItem::k3DShowPath:
            return "Show Path";
        case WorkspacePanelItem::k3DShowPathVisited:
            return "Show Visited";
        case WorkspacePanelItem::k3DValidationGroup:
            return "Validation";
        case WorkspacePanelItem::k3DValidationModeOff:
            return "Off";
        case WorkspacePanelItem::k3DValidationModeManual:
            return "Manual";
        case WorkspacePanelItem::k3DValidationModeOnLoad:
            return "On Load";
        case WorkspacePanelItem::k3DRunPassabilityValidation:
            return "Run Validation";
        case WorkspacePanelItem::k3DClearPassabilityValidation:
            return "Clear Validation";
        case WorkspacePanelItem::k3DShowPassabilityIssues:
            return "Show Issues";
        case WorkspacePanelItem::k3DValidationInvalidTransitions:
            return "Invalid Transitions";
        case WorkspacePanelItem::k3DValidationBlockedTransitions:
            return "Blocked Transitions";
        case WorkspacePanelItem::k3DValidationSuspiciousDrops:
            return "Suspicious Drops";
        case WorkspacePanelItem::k3DValidationIsolatedTiles:
            return "Isolated Tiles";
        case WorkspacePanelItem::kRenderChunkBounds:
            return labels.workspace_subitem_chunk_bounds;
        case WorkspacePanelItem::kRenderWorldGrid:
            return labels.workspace_subitem_world_grid;
        case WorkspacePanelItem::kRenderCollision:
            return labels.workspace_subitem_collision_overlay;
        case WorkspacePanelItem::kRenderHeight:
            return labels.workspace_subitem_height;
        case WorkspacePanelItem::k3DObjectsGroup:
            return "Objects";
        case WorkspacePanelItem::k3DObjectsAll:
            return "All Objects";
        case WorkspacePanelItem::k3DObjectsTrees:
            return "Trees";
        case WorkspacePanelItem::k3DObjectsBushes:
            return "Bushes";
        case WorkspacePanelItem::k3DObjectsReeds:
            return "Reeds";
        case WorkspacePanelItem::k3DObjectsRuins:
            return "Ruins";
        case WorkspacePanelItem::k3DObjectsCover:
            return "Cover";
        case WorkspacePanelItem::k3DObjectsLoot:
            return "Loot / Cache";
        case WorkspacePanelItem::k3DObjectsStructures:
            return "Structures";
        case WorkspacePanelItem::k3DObjectsTrenches:
            return "Trenches";
        case WorkspacePanelItem::k3DObjectsUnknown:
            return "Unknown";
        case WorkspacePanelItem::k3DMeshGroup:
            return "Mesh";
        case WorkspacePanelItem::k3DChunkSizeGroup:
            return "Chunk Size";
        case WorkspacePanelItem::k3DChunkSize16:
            return "16";
        case WorkspacePanelItem::k3DChunkSize32:
            return "32";
        case WorkspacePanelItem::k3DChunkSizeProfit:
            return "Chunk Profit";
        case WorkspacePanelItem::k3DMeshSimple:
            return "Simple";
        case WorkspacePanelItem::k3DMeshGreedy:
            return "Greedy";
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
        case WorkspacePanelItem::kStatsMapGroup:
            return "Map";
        case WorkspacePanelItem::kStatsColorGroup:
            return "Color";
        case WorkspacePanelItem::kStatsVisibilityGroup:
            return "Visibility";
        case WorkspacePanelItem::kStatsTransitionsGroup:
            return "Transitions";
        case WorkspacePanelItem::kStatsMovementGroup:
            return "Movement";
        case WorkspacePanelItem::kStatsPathGroup:
            return "Path Probe";
        case WorkspacePanelItem::kStatsPassabilityGroup:
            return "Passability Validation";
        case WorkspacePanelItem::kStatsMeshGroup:
            return "Mesh";
        case WorkspacePanelItem::kStatsComparisonGroup:
            return "Comparison";
        case WorkspacePanelItem::kStatsChunkProfitGroup:
            return "Chunk Profit";
        case WorkspacePanelItem::kStatsDirtyCacheGroup:
            return "Dirty Cache";
        case WorkspacePanelItem::kStatsCameraGroup:
            return "Camera";
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

[[nodiscard]] std::string WorkspacePanelItemMarker(WorkspacePanelItemState item)
{
    switch (item.kind) {
        case WorkspacePanelItemKind::kGroup:
            if (item.depth == 0 && IsCollapsibleWorkspacePanelGroup(item.item)) {
                return item.checked ? "[-]" : "[+]";
            }
            return "-";
        case WorkspacePanelItemKind::kAction:
            return item.enabled ? "" : "[-]";
        case WorkspacePanelItemKind::kCheckbox:
            return item.enabled ? (item.checked ? "[x]" : "[ ]") : "[-]";
        case WorkspacePanelItemKind::kRadio:
            return item.enabled ? (item.checked ? "(x)" : "( )") : "(-)";
        case WorkspacePanelItemKind::kValue:
            return item.enabled ? "=" : "-";
    }
    return "";
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


[[nodiscard]] WorkspaceLayout BuildWorkspaceLayout(
    const UiFontSet& fonts,
    const UiMetrics& metrics,
    const WorkspaceState& workspace_state,
    FreeFlyCameraStatus /*camera_status*/,
    const UiLabels& labels)
{
    WorkspaceLayout layout;
    layout.mode_buttons.reserve(2);

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

    const float tool_x = layout.tool_panel.x + padding;
    const float tool_width = std::max(1.0F, layout.tool_panel.width - padding * 2.0F);
    const float row_bottom = layout.tool_panel.y + layout.tool_panel.height - padding;
    const float tab_height = metrics.workspace_tab_font_size + gap * 1.55F;
    const float item_height = metrics.workspace_tool_font_size + gap * 1.05F;
    const float subitem_indent = gap * 3.0F;

    layout.tool_header = Rectangle{
        tool_x,
        layout.tool_panel.y + padding,
        tool_width,
        tab_height,
    };

    constexpr std::array<WorkspaceViewMode, 2> modes{
        WorkspaceViewMode::kMap2D,
        WorkspaceViewMode::kWorld3D,
    };
    const float mode_font_size = metrics.workspace_tab_font_size;
    const float mode_spacing = FontSpacing(mode_font_size);
    const float mode_gap = std::max(2.0F, border);
    const float mode_width = std::max(1.0F, (tool_width - mode_gap) * 0.5F);
    for (std::size_t index = 0; index < modes.size(); ++index) {
        const WorkspaceViewMode mode = modes[index];
        const Rectangle bounds{
            tool_x + static_cast<float>(index) * (mode_width + mode_gap),
            layout.tool_header.y,
            mode_width,
            tab_height,
        };
        const std::string label = WorkspaceViewModeLabel(mode, labels);
        const Vector2 label_size = Measure(
            fonts.text,
            label,
            mode_font_size,
            mode_spacing);
        const bool is_3d = mode == WorkspaceViewMode::kWorld3D;
        layout.mode_buttons.push_back(WorkspaceModeButtonBounds{
            mode,
            bounds,
            Vector2{
                bounds.x + (bounds.width - label_size.x) * 0.5F,
                bounds.y + (bounds.height - label_size.y) * 0.5F,
            },
            !is_3d || workspace_state.chunk_meshes.IsValid(),
            workspace_state.show_3d_preview == is_3d,
        });
    }

    const float content_y = layout.tool_header.y + layout.tool_header.height + gap;
    layout.tool_menu = Rectangle{
        tool_x,
        content_y,
        tool_width,
        std::max(1.0F, row_bottom - content_y),
    };
    layout.tool_info = layout.tool_menu;

    const std::vector<WorkspacePanelItemState> selected_items = BuildWorkspacePanelItems(workspace_state);
    const bool text_tree_panel = false;
    const std::vector<TextPanelRow> text_rows;

    layout.panel_total_rows = static_cast<int>(selected_items.size());
    layout.panel_visible_rows = static_cast<int>(std::max(0.0F, std::floor((row_bottom - content_y) / item_height)));
    const int max_scroll_rows = std::max(0, layout.panel_total_rows - layout.panel_visible_rows);
    layout.panel_first_visible_row = std::clamp(workspace_state.menu_scroll_rows, 0, max_scroll_rows);
    layout.panel_can_scroll_up = layout.panel_first_visible_row > 0;
    layout.panel_can_scroll_down = layout.panel_first_visible_row < max_scroll_rows;

    float row_y = content_y;
    layout.panel_items.reserve(static_cast<std::size_t>(std::max(0, layout.panel_visible_rows)));
    if (text_tree_panel) {
        for (int row = layout.panel_first_visible_row; row < layout.panel_total_rows; ++row) {
            if (row_y + item_height > row_bottom) {
                break;
            }
            const TextPanelRow& item = text_rows[static_cast<std::size_t>(row)];
            if (item.kind == TextPanelRowKind::kGroup) {
                const Rectangle item_bounds{
                    tool_x,
                    row_y,
                    tool_width,
                    item_height,
                };
                layout.panel_items.push_back(WorkspacePanelItemBounds{
                    item.item,
                    WorkspacePanelItemKind::kGroup,
                    item.depth,
                    item_bounds,
                    Vector2{
                        item_bounds.x + gap,
                        item_bounds.y + (item_bounds.height - metrics.workspace_tool_font_size) * 0.5F,
                    },
                    true,
                    item.checked,
                });
            }
            row_y += item.kind == TextPanelRowKind::kBlank ? item_height * 0.55F : item_height;
        }
    } else {
        for (int row = layout.panel_first_visible_row; row < layout.panel_total_rows; ++row) {
            if (row_y + item_height > row_bottom) {
                break;
            }
            const WorkspacePanelItemState& item = selected_items[static_cast<std::size_t>(row)];
            const float indent = subitem_indent * static_cast<float>(std::max(0, item.depth));
            const Rectangle item_bounds{
                tool_x,
                row_y,
                tool_width,
                item_height,
            };
            layout.panel_items.push_back(WorkspacePanelItemBounds{
                item.item,
                item.kind,
                item.depth,
                item_bounds,
                Vector2{item_bounds.x + gap + indent, item_bounds.y + (item_bounds.height - metrics.workspace_tool_font_size) * 0.5F},
                item.enabled,
                item.checked,
            });
            row_y += item_height;
        }
    }

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

TileContextMenuLayout BuildTileContextMenuLayout(
    Vector2 anchor,
    const UiFontSet& fonts,
    const UiLayoutCache& layout)
{
    const UiMetrics& metrics = layout.metrics;
    const Rectangle viewport = layout.workspace.map_overview;
    const float font_size = metrics.workspace_tool_font_size;
    const float spacing = FontSpacing(font_size);
    const float padding_x = std::max(10.0F, metrics.workspace_tool_gap * 1.8F);
    const float padding_y = std::max(7.0F, metrics.workspace_tool_gap * 1.2F);
    const float row_height = std::max(34.0F * metrics.scale, font_size + padding_y * 2.0F);
    const float label_width = std::max(
        Measure(fonts.text, "Tile info", font_size, spacing).x,
        Measure(fonts.text, "Go to 3D view", font_size, spacing).x);
    const float width = std::max(190.0F * metrics.scale, label_width + padding_x * 2.0F);
    const float height = row_height * 2.0F;
    const float offset = std::max(8.0F, metrics.workspace_tool_gap * 1.5F);
    const float viewport_margin = std::max(3.0F, metrics.workspace_border_width * 2.0F);

    float x = anchor.x + offset;
    float y = anchor.y + offset;
    if (x + width > viewport.x + viewport.width - viewport_margin) {
        x = anchor.x - width - offset;
    }
    if (y + height > viewport.y + viewport.height - viewport_margin) {
        y = anchor.y - height - offset;
    }
    x = std::clamp(
        x,
        viewport.x + viewport_margin,
        std::max(viewport.x + viewport_margin, viewport.x + viewport.width - width - viewport_margin));
    y = std::clamp(
        y,
        viewport.y + viewport_margin,
        std::max(viewport.y + viewport_margin, viewport.y + viewport.height - height - viewport_margin));

    const Rectangle bounds{x, y, width, height};
    return TileContextMenuLayout{
        bounds,
        Rectangle{x, y, width, row_height},
        Rectangle{x, y + row_height, width, row_height},
    };
}

std::optional<TileContextMenuAction> HitTestTileContextMenu(
    const TileContextMenuLayout& menu,
    Vector2 mouse)
{
    if (CheckCollisionPointRec(mouse, menu.tile_info_bounds)) {
        return TileContextMenuAction::kTileInfo;
    }
    if (CheckCollisionPointRec(mouse, menu.go_to_3d_bounds)) {
        return TileContextMenuAction::kGoTo3DView;
    }
    return std::nullopt;
}

void DrawTileContextMenu(
    Vector2 anchor,
    const UiFontSet& fonts,
    const UiLayoutCache& layout)
{
    const TileContextMenuLayout menu = BuildTileContextMenuLayout(anchor, fonts, layout);
    const UiMetrics& metrics = layout.metrics;
    const float font_size = metrics.workspace_tool_font_size;
    const float spacing = FontSpacing(font_size);
    const float padding_x = std::max(10.0F, metrics.workspace_tool_gap * 1.8F);
    const Vector2 mouse = GetMousePosition();
    const std::optional<TileContextMenuAction> hovered = HitTestTileContextMenu(menu, mouse);

    DrawRectangleRounded(menu.bounds, 0.06F, 6, kEditorTooltipBackground);
    DrawRectangleRoundedLinesEx(
        menu.bounds,
        0.06F,
        6,
        std::max(1.0F, metrics.workspace_border_width),
        kEditorTooltipBorder);

    const std::array<std::pair<TileContextMenuAction, std::pair<Rectangle, std::string_view>>, 2> items{{
        {TileContextMenuAction::kTileInfo, {menu.tile_info_bounds, "Tile info"}},
        {TileContextMenuAction::kGoTo3DView, {menu.go_to_3d_bounds, "Go to 3D view"}},
    }};
    for (const auto& [action, item] : items) {
        const Rectangle item_bounds = item.first;
        const std::string_view label = item.second;
        const bool is_hovered = hovered.has_value() && *hovered == action;
        if (is_hovered) {
            DrawRectangleRec(item_bounds, Color{66, 60, 42, 245});
        }
        const Vector2 text_size = Measure(fonts.text, std::string(label), font_size, spacing);
        DrawTextEx(
            fonts.text,
            std::string(label).c_str(),
            Vector2{
                item_bounds.x + padding_x,
                item_bounds.y + (item_bounds.height - text_size.y) * 0.5F,
            },
            font_size,
            spacing,
            is_hovered ? kAccent : kText);
    }

    DrawLineEx(
        Vector2{menu.bounds.x, menu.go_to_3d_bounds.y},
        Vector2{menu.bounds.x + menu.bounds.width, menu.go_to_3d_bounds.y},
        1.0F,
        Color{92, 98, 112, 220});
}

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
    metrics.workspace_tab_font_size = Scaled(kBaseWorkspaceToolFontSize + 2.0F, text_scale, 15.0F, 29.0F);
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
    const WorkspaceState& workspace,
    FreeFlyCameraStatus camera_status)
{
    UiLayoutCache layout;
    layout.metrics = CalculateUiMetrics(window, config);
    layout.main_menu = BuildMainMenuLayout(menu, fonts, layout.metrics);
    layout.placeholder = BuildPlaceholderLayout(fonts, layout.metrics, labels);
    layout.workspace = BuildWorkspaceLayout(fonts, layout.metrics, workspace, camera_status, labels);
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




void DrawWorkspaceModeButton(
    Font font,
    const WorkspaceModeButtonBounds& button,
    const UiLabels& labels,
    float font_size,
    float spacing)
{
    const Color background = button.selected
        ? Color{24, 58, 66, 255}
        : Color{7, 103, 132, 255};
    const Color border = button.selected ? kAccent : kEditorBorder;
    const Color text = !button.enabled
        ? Color{74, 138, 154, 255}
        : (button.selected ? kAccent : kEditorPanelText);

    DrawRectangleRec(button.bounds, background);
    DrawRectangleLinesEx(button.bounds, 1.0F, border);
    const std::string label = WorkspaceViewModeLabel(button.mode, labels);
    DrawTextEx(
        font,
        label.c_str(),
        button.text_position,
        font_size,
        spacing,
        text);
}

void DrawWorkspaceTooltip(
    Font font,
    const std::string& text,
    Vector2 mouse,
    const UiMetrics& metrics,
    float font_size,
    float spacing)
{
    if (text.empty()) {
        return;
    }

    const float padding_x = std::max(8.0F, metrics.workspace_tool_gap * 1.5F);
    const float padding_y = std::max(6.0F, metrics.workspace_tool_gap);
    const Vector2 text_size = Measure(font, text, font_size, spacing);
    const float max_width = static_cast<float>(metrics.window_width) - padding_x * 2.0F - 8.0F;
    const float tooltip_width = std::min(text_size.x + padding_x * 2.0F, max_width);
    const float tooltip_height = text_size.y + padding_y * 2.0F;
    float x = mouse.x + 14.0F;
    float y = mouse.y + 18.0F;

    if (x + tooltip_width > static_cast<float>(metrics.window_width) - 4.0F) {
        x = mouse.x - tooltip_width - 14.0F;
    }
    if (y + tooltip_height > static_cast<float>(metrics.window_height) - 4.0F) {
        y = mouse.y - tooltip_height - 14.0F;
    }
    x = std::clamp(x, 4.0F, std::max(4.0F, static_cast<float>(metrics.window_width) - tooltip_width - 4.0F));
    y = std::clamp(y, 4.0F, std::max(4.0F, static_cast<float>(metrics.window_height) - tooltip_height - 4.0F));

    const Rectangle bounds{x, y, tooltip_width, tooltip_height};
    DrawRectangleRounded(bounds, 0.08F, 8, kEditorTooltipBackground);
    DrawRectangleRoundedLinesEx(bounds, 0.08F, 8, 1.0F, kEditorTooltipBorder);
    DrawTextEx(
        font,
        text.c_str(),
        Vector2{bounds.x + padding_x, bounds.y + padding_y},
        font_size,
        spacing,
        kEditorViewportText);
}


struct StatsOverlaySection {
    std::string title;
    std::vector<std::pair<std::string, std::string>> rows;
};

[[nodiscard]] std::pair<std::string, std::string> SplitStatsValue(std::string_view line)
{
    const std::size_t colon = line.find(':');
    if (colon == std::string_view::npos) {
        return {std::string(line), {}};
    }
    std::string key(line.substr(0, colon));
    std::string value(line.substr(colon + 1));
    const std::size_t first = value.find_first_not_of(' ');
    if (first != std::string::npos) {
        value.erase(0, first);
    }
    return {std::move(key), std::move(value)};
}

[[nodiscard]] std::vector<StatsOverlaySection> BuildStatsOverlaySections(
    const WorkspaceState& workspace,
    const Map2DView* map_2d_view,
    FreeFlyCameraStatus camera_status)
{
    std::vector<StatsOverlaySection> sections;
    auto add = [&sections](std::string title,
                   std::initializer_list<std::pair<std::string, std::string>> rows) {
        StatsOverlaySection section;
        section.title = std::move(title);
        section.rows.assign(rows.begin(), rows.end());
        sections.push_back(std::move(section));
    };

    add("Map", {
        {"Size", std::to_string(workspace.runtime_map.info.width) + " x " + std::to_string(workspace.runtime_map.info.height)},
        {"Tiles", std::to_string(workspace.runtime_map.info.width * workspace.runtime_map.info.height)},
        {"Elevation", std::to_string(workspace.runtime_map.info.levels.has_value() ? workspace.runtime_map.info.levels->min : 0) + " .. " + std::to_string(workspace.runtime_map.info.levels.has_value() ? workspace.runtime_map.info.levels->max : 0)},
        {"Walkable", std::to_string(workspace.runtime_map.info.width * workspace.runtime_map.info.height - workspace.runtime_map.info.blocked_cells)},
        {"Blocked", std::to_string(workspace.runtime_map.info.blocked_cells)},
        {"Objects", std::to_string(workspace.runtime_map.runtime_objects.size())},
        {"Vegetation", std::to_string(workspace.runtime_map.info.vegetation_markers)},
        {"Places", std::to_string(workspace.runtime_map.places.size())},
        {"Markers", std::to_string(workspace.runtime_map.markers.size())},
    });

    add("Ruin Height", {
        {"Source", workspace.runtime_map.info.structure_height_loaded ? "map" : "legacy zero"},
        {"Wall tiles", std::to_string(workspace.runtime_map.info.structure_tiles)},
        {"Height 1", std::to_string(workspace.runtime_map.info.structure_height_1)},
        {"Height 2", std::to_string(workspace.runtime_map.info.structure_height_2)},
        {"Height 3", std::to_string(workspace.runtime_map.info.structure_height_3)},
    });

    if (!workspace.show_3d_preview) {
        const Map2DViewStatus status = map_2d_view != nullptr ? map_2d_view->Status() : Map2DViewStatus{};
        add("2D View", {
            {"Layer", std::string(Map2DBaseLayerLabel(workspace.map_2d_base_layer))},
            {"Zoom", CompactFloat(status.pixels_per_tile) + " px/tile"},
            {"Fit zoom", CompactFloat(status.fit_pixels_per_tile) + " px/tile"},
            {"Center", CompactFloat(status.center_tile.x) + ", " + CompactFloat(status.center_tile.y)},
            {"Hover", status.hover_tile_valid ? TileCoordText(status.hover_tile) : "none"},
            {"Selected", workspace.selected_tile.IsValid() ? TileCoordText(workspace.selected_tile.tile) : "none"},
            {"Panning", status.panning ? "yes" : "no"},
            {"FPS", std::to_string(GetFPS())},
        });
        add("2D Overlays", {
            {"Grid", workspace.show_grid_layer ? "on" : "off"},
            {"Chunks", workspace.show_2d_chunks ? "on" : "off"},
            {"VXMAP regions", workspace.show_2d_vxmap_regions ? "on" : "off"},
            {"Start / Goal", workspace.show_2d_start_goal ? "on" : "off"},
            {"Objects", workspace.show_2d_objects ? "on" : "off"},
            {"Vegetation", workspace.show_2d_vegetation ? "on" : "off"},
            {"Places", workspace.show_2d_places ? "on" : "off"},
            {"Markers", workspace.show_2d_markers ? "on" : "off"},
        });
        add("Runtime", {
            {"Source", workspace.runtime_map.info.runtime_binary_loaded ? "VXMAP" : "JSON"},
            {"Texture", std::to_string(workspace.runtime_map.info.width) + " x " + std::to_string(workspace.runtime_map.info.height)},
            {"Chunk size", std::to_string(workspace.chunk_grid.info.chunk_size_x) + " x " + std::to_string(workspace.chunk_grid.info.chunk_size_y)},
            {"Chunks", std::to_string(workspace.chunk_grid.info.chunks_x) + " x " + std::to_string(workspace.chunk_grid.info.chunks_y)},
            {"Transitions", std::to_string(workspace.transition_features.stats.total)},
        });
        return sections;
    }

    add("Camera", {
        {"State", camera_status.cursor_captured ? "captured" : "free"},
        {"Yaw / Pitch", CompactFloat(camera_status.yaw_degrees) + " / " + CompactFloat(camera_status.pitch_degrees)},
        {"Position", CompactVector(camera_status.position)},
        {"Speed", CompactFloat(camera_status.speed)},
    });
    add("Visibility", {
        {"Mode", VisibilityModeLabel(workspace.visibility_mode)},
        {"Resident", std::to_string(workspace.visibility_stats.resident_chunks)},
        {"Visible / Fade", std::to_string(workspace.visibility_stats.visible_chunks) + " / " + std::to_string(workspace.visibility_stats.fade_chunks)},
        {"Hidden", std::to_string(workspace.visibility_stats.hidden_chunks)},
        {"Models", std::to_string(workspace.visibility_stats.drawn_models) + " / " + std::to_string(workspace.visibility_stats.resident_models)},
        {"Faces", std::to_string(workspace.visibility_stats.drawn_faces) + " / " + std::to_string(workspace.visibility_stats.total_faces)},
        {"Radius / Fade", std::to_string(workspace.visibility_radius_chunks) + " / " + std::to_string(workspace.visibility_fade_ring_chunks)},
    });
    add("Mesh", {
        {"Mode", std::string(ToString(workspace.mesh_mode))},
        {"Chunk", std::to_string(workspace.chunk_grid.info.chunk_size_x)},
        {"Chunks", std::to_string(workspace.chunk_grid.info.chunks_x) + " x " + std::to_string(workspace.chunk_grid.info.chunks_y)},
        {"Faces", std::to_string(workspace.mesh_stats.active_faces)},
        {"Vertices", std::to_string(workspace.mesh_stats.active_vertices)},
        {"Indices", std::to_string(workspace.mesh_stats.active_indices)},
        {"Models", std::to_string(workspace.mesh_stats.draw_models)},
        {"Saved", CompactPercent(workspace.mesh_stats.ActiveReductionRatio())},
    });
    add("Mesh Comparison", {
        {"Simple", std::to_string(workspace.mesh_stats.simple_faces)},
        {"Greedy", std::to_string(workspace.mesh_stats.greedy_faces)},
        {"Terrain raw", std::to_string(workspace.mesh_stats.terrain_raw_top_faces + workspace.mesh_stats.terrain_raw_wall_faces)},
        {"Terrain merged", std::to_string(workspace.mesh_stats.terrain_faces)},
        {"Raw top / wall", std::to_string(workspace.mesh_stats.terrain_raw_top_faces) + " / " + std::to_string(workspace.mesh_stats.terrain_raw_wall_faces)},
        {"Top / wall / cliff", std::to_string(workspace.mesh_stats.terrain_top_faces) + " / " + std::to_string(workspace.mesh_stats.terrain_wall_faces) + " / " + std::to_string(workspace.mesh_stats.terrain_cliff_faces)},
    });
    add("Transitions", {
        {"Total", std::to_string(workspace.transition_features.stats.total)},
        {"Ramp / Stair", std::to_string(workspace.transition_features.stats.ramps) + " / " + std::to_string(workspace.transition_features.stats.stairs)},
        {"Bridge / Drop", std::to_string(workspace.transition_features.stats.bridges) + " / " + std::to_string(workspace.transition_features.stats.drops)},
        {"Passable", std::to_string(workspace.transition_features.stats.passable)},
        {"Blocked", std::to_string(workspace.transition_features.stats.blocked)},
    });
    add("Path Probe", {
        {"Status", workspace.path_probe.IsValid() ? "ready" : "not run"},
        {"Visited", std::to_string(workspace.path_probe.stats.visited_nodes)},
        {"Expanded", std::to_string(workspace.path_probe.stats.expanded_nodes)},
        {"Blocked edges", std::to_string(workspace.path_probe.stats.blocked_edges)},
    });
    add("Validation", {
        {"Mode", ValidationModeLabel(workspace.validation_mode)},
        {"Status", ValidationStatusLabel(workspace.passability_validation_status)},
        {"Edges", std::to_string(workspace.passability_validation.stats.checked_edges)},
        {"Invalid", std::to_string(workspace.passability_validation.stats.invalid_transitions)},
        {"Drops", std::to_string(workspace.passability_validation.stats.suspicious_drops)},
        {"Isolated", std::to_string(workspace.passability_validation.stats.isolated_tiles)},
    });
    add("Streaming", {
        {"Built", std::to_string(workspace.progressive_chunks_built) + " / " + std::to_string(workspace.progressive_chunks_total)},
        {"Pending", std::to_string(workspace.progressive_chunks_total - workspace.progressive_chunks_built)},
        {"FPS", std::to_string(GetFPS())},
    });
    return sections;
}

struct StatsOverlayGeometry {
    Rectangle panel;
    Rectangle content;
    float padding = 0.0F;
    float title_font_size = 0.0F;
    float body_font_size = 0.0F;
    float footer_font_size = 0.0F;
    float line_height = 0.0F;
    float column_gap = 0.0F;
    float section_gap = 0.0F;
};

[[nodiscard]] StatsOverlayGeometry BuildStatsOverlayGeometry(const UiLayoutCache& layout)
{
    const UiMetrics& metrics = layout.metrics;
    const SelectionInfoOverlayGeometry info_geometry =
        BuildSelectionInfoOverlayGeometry(layout);

    StatsOverlayGeometry geometry;
    geometry.panel = info_geometry.panel;
    geometry.content = info_geometry.content;
    geometry.padding = info_geometry.padding;
    geometry.title_font_size = info_geometry.title_font_size;
    geometry.body_font_size = info_geometry.body_font_size;
    geometry.footer_font_size = info_geometry.footer_font_size;
    geometry.line_height = info_geometry.line_height;
    geometry.column_gap = std::max(14.0F, 18.0F * metrics.scale);
    geometry.section_gap = std::max(6.0F, 8.0F * metrics.scale);
    return geometry;
}

[[nodiscard]] float StatsOverlayContentHeight(
    const std::vector<StatsOverlaySection>& sections,
    const StatsOverlayGeometry& geometry)
{
    float height = 0.0F;
    for (std::size_t index = 0; index < sections.size(); index += 2U) {
        const std::size_t left_rows = sections[index].rows.size();
        const std::size_t right_rows = index + 1U < sections.size()
            ? sections[index + 1U].rows.size()
            : 0U;
        height += static_cast<float>(std::max(left_rows, right_rows) + 1U)
            * geometry.line_height;
        height += geometry.section_gap;
    }
    return height;
}

int StatsOverlayMaxScrollRows(
    const WorkspaceState& workspace,
    const Map2DView* map_2d_view,
    FreeFlyCameraStatus camera_status,
    const UiLayoutCache& layout)
{
    const StatsOverlayGeometry geometry = BuildStatsOverlayGeometry(layout);
    const auto sections = BuildStatsOverlaySections(
        workspace,
        map_2d_view,
        camera_status);
    const float overflow = std::max(
        0.0F,
        StatsOverlayContentHeight(sections, geometry) - geometry.content.height);
    return static_cast<int>(std::ceil(overflow / geometry.line_height));
}

void DrawStatsOverlay(
    const WorkspaceState& workspace,
    const Map2DView* map_2d_view,
    FreeFlyCameraStatus camera_status,
    int first_visible_row,
    const UiFontSet& fonts,
    const UiLayoutCache& layout)
{
    const UiMetrics& metrics = layout.metrics;
    const StatsOverlayGeometry geometry = BuildStatsOverlayGeometry(layout);
    const auto sections = BuildStatsOverlaySections(
        workspace,
        map_2d_view,
        camera_status);
    const int max_scroll = StatsOverlayMaxScrollRows(
        workspace,
        map_2d_view,
        camera_status,
        layout);
    const int scroll_row = std::clamp(first_visible_row, 0, max_scroll);
    const float scroll_y = static_cast<float>(scroll_row)
        * geometry.line_height;

    DrawRectangle(0, 0, metrics.window_width, metrics.window_height, kModalDim);
    DrawRectangleRounded(geometry.panel, 0.04F, 8, kPanel);
    DrawRectangleRoundedLinesEx(
        geometry.panel,
        0.04F,
        8,
        std::max(1.0F, metrics.modal_border_width),
        kPanelBorder);

    const std::string title = workspace.show_3d_preview
        ? "[ Stats - 3D ]"
        : "[ Stats - 2D ]";
    DrawTextEx(
        fonts.text,
        title.c_str(),
        Vector2{
            geometry.panel.x + geometry.padding,
            geometry.panel.y + geometry.padding,
        },
        geometry.title_font_size,
        FontSpacing(geometry.title_font_size),
        kSelectedText);

    BeginScissorMode(
        static_cast<int>(std::floor(geometry.content.x)),
        static_cast<int>(std::floor(geometry.content.y)),
        static_cast<int>(std::ceil(geometry.content.width)),
        static_cast<int>(std::ceil(geometry.content.height)));

    const float column_width =
        (geometry.content.width - geometry.column_gap) * 0.5F;
    float y = geometry.content.y - scroll_y;
    for (std::size_t index = 0; index < sections.size(); index += 2U) {
        const std::size_t left_rows = sections[index].rows.size();
        const std::size_t right_rows = index + 1U < sections.size()
            ? sections[index + 1U].rows.size()
            : 0U;
        const float group_height =
            static_cast<float>(std::max(left_rows, right_rows) + 1U)
                * geometry.line_height
            + geometry.section_gap;

        for (int column = 0; column < 2; ++column) {
            const std::size_t section_index =
                index + static_cast<std::size_t>(column);
            if (section_index >= sections.size()) {
                continue;
            }

            const float x = geometry.content.x
                + static_cast<float>(column)
                    * (column_width + geometry.column_gap);
            const StatsOverlaySection& section = sections[section_index];
            float row_y = y;
            DrawTextEx(
                fonts.text,
                section.title.c_str(),
                Vector2{x, row_y},
                geometry.body_font_size,
                FontSpacing(geometry.body_font_size),
                kAccent);
            row_y += geometry.line_height;

            const float value_x = x + column_width * 0.56F;
            for (const auto& [key, value] : section.rows) {
                DrawTextEx(
                    fonts.text,
                    key.c_str(),
                    Vector2{x, row_y},
                    geometry.body_font_size,
                    FontSpacing(geometry.body_font_size),
                    kMutedText);
                DrawTextEx(
                    fonts.text,
                    value.c_str(),
                    Vector2{value_x, row_y},
                    geometry.body_font_size,
                    FontSpacing(geometry.body_font_size),
                    kText);
                row_y += geometry.line_height;
            }
        }
        y += group_height;
    }
    EndScissorMode();

    const float footer_y = geometry.panel.y + geometry.panel.height
        - geometry.padding - geometry.footer_font_size;
    const std::string close_hint = "Esc / S  Close";
    DrawTextEx(
        fonts.text,
        close_hint.c_str(),
        Vector2{geometry.panel.x + geometry.padding, footer_y},
        geometry.footer_font_size,
        FontSpacing(geometry.footer_font_size),
        kMutedText);

    if (max_scroll > 0) {
        const std::string scroll_hint = "Wheel / PgUp / PgDn";
        const float footer_spacing = FontSpacing(geometry.footer_font_size);
        const Vector2 hint_size = Measure(
            fonts.text,
            scroll_hint,
            geometry.footer_font_size,
            footer_spacing);
        DrawTextEx(
            fonts.text,
            scroll_hint.c_str(),
            Vector2{
                geometry.panel.x + geometry.panel.width
                    - geometry.padding - hint_size.x,
                footer_y,
            },
            geometry.footer_font_size,
            footer_spacing,
            kMutedText);

        if (scroll_row > 0) {
            const std::string more_hint = "^ more";
            const Vector2 more_size = Measure(
                fonts.text,
                more_hint,
                geometry.footer_font_size,
                footer_spacing);
            DrawTextEx(
                fonts.text,
                more_hint.c_str(),
                Vector2{
                    geometry.content.x + geometry.content.width - more_size.x,
                    geometry.content.y,
                },
                geometry.footer_font_size,
                footer_spacing,
                kAccent);
        }
        if (scroll_row < max_scroll) {
            const std::string more_hint = "v more";
            const Vector2 more_size = Measure(
                fonts.text,
                more_hint,
                geometry.footer_font_size,
                footer_spacing);
            DrawTextEx(
                fonts.text,
                more_hint.c_str(),
                Vector2{
                    geometry.content.x + geometry.content.width - more_size.x,
                    geometry.content.y + geometry.content.height
                        - geometry.footer_font_size,
                },
                geometry.footer_font_size,
                footer_spacing,
                kAccent);
        }
    }
}

int HelpOverlayMaxScrollRows(const UiLayoutCache& layout)
{
    const SelectionInfoOverlayGeometry geometry =
        BuildSelectionInfoOverlayGeometry(layout);
    const int total_rows = static_cast<int>(BuildHelpPanelLines().size());
    return std::max(0, total_rows - geometry.visible_rows);
}

void DrawHelpOverlay(
    int first_visible_row,
    const UiFontSet& fonts,
    const UiLayoutCache& layout)
{
    const UiMetrics& metrics = layout.metrics;
    const SelectionInfoOverlayGeometry geometry =
        BuildSelectionInfoOverlayGeometry(layout);
    const std::vector<std::string> lines = BuildHelpPanelLines();
    const int max_scroll_rows = std::max(
        0,
        static_cast<int>(lines.size()) - geometry.visible_rows);
    const int first_row = std::clamp(
        first_visible_row,
        0,
        max_scroll_rows);

    DrawRectangle(0, 0, metrics.window_width, metrics.window_height, kModalDim);
    DrawRectangleRounded(geometry.panel, 0.04F, 8, kPanel);
    DrawRectangleRoundedLinesEx(
        geometry.panel,
        0.04F,
        8,
        std::max(1.0F, metrics.modal_border_width),
        kPanelBorder);

    const std::string title = "[ Help ]";
    DrawTextEx(
        fonts.text,
        title.c_str(),
        Vector2{
            geometry.panel.x + geometry.padding,
            geometry.panel.y + geometry.padding,
        },
        geometry.title_font_size,
        FontSpacing(geometry.title_font_size),
        kSelectedText);

    BeginScissorMode(
        static_cast<int>(std::floor(geometry.content.x)),
        static_cast<int>(std::floor(geometry.content.y)),
        static_cast<int>(std::ceil(geometry.content.width)),
        static_cast<int>(std::ceil(geometry.content.height)));
    const float body_spacing = FontSpacing(geometry.body_font_size);
    float hotkey_column_width = 0.0F;
    for (const std::string& line : lines) {
        const std::optional<HelpControlLine> control =
            ParseHelpControlLine(line);
        if (!control.has_value()) {
            continue;
        }
        hotkey_column_width = std::max(
            hotkey_column_width,
            Measure(
                fonts.text,
                std::string(control->hotkey),
                geometry.body_font_size,
                body_spacing).x);
    }

    const float column_gap = std::max(18.0F, geometry.padding * 0.55F);
    const float action_x = geometry.content.x + hotkey_column_width + column_gap;

    float line_y = geometry.content.y;
    const int last_row = std::min(
        static_cast<int>(lines.size()),
        first_row + geometry.visible_rows);
    for (int index = first_row; index < last_row; ++index) {
        const std::string& line = lines[static_cast<std::size_t>(index)];
        if (line.empty()) {
            line_y += geometry.line_height;
            continue;
        }

        const std::optional<HelpControlLine> control =
            ParseHelpControlLine(line);
        if (!control.has_value()) {
            DrawTextEx(
                fonts.text,
                line.c_str(),
                Vector2{geometry.content.x, line_y},
                geometry.body_font_size,
                body_spacing,
                kAccent);
        } else {
            const std::string hotkey(control->hotkey);
            DrawTextEx(
                fonts.text,
                hotkey.c_str(),
                Vector2{geometry.content.x, line_y},
                geometry.body_font_size,
                body_spacing,
                kHelpHotkey);

            if (!control->action.empty()) {
                const std::string action(control->action);
                DrawTextEx(
                    fonts.text,
                    action.c_str(),
                    Vector2{action_x, line_y},
                    geometry.body_font_size,
                    body_spacing,
                    kText);
            }
        }
        line_y += geometry.line_height;
    }
    EndScissorMode();

    const float footer_y = geometry.panel.y + geometry.panel.height
        - geometry.padding - geometry.footer_font_size;
    const std::string close_hint = "Esc / F1  Close";
    DrawTextEx(
        fonts.text,
        close_hint.c_str(),
        Vector2{geometry.panel.x + geometry.padding, footer_y},
        geometry.footer_font_size,
        FontSpacing(geometry.footer_font_size),
        kMutedText);

    if (max_scroll_rows > 0) {
        const std::string scroll_hint = "Wheel / PgUp / PgDn";
        const float footer_spacing = FontSpacing(geometry.footer_font_size);
        const Vector2 hint_size = Measure(
            fonts.text,
            scroll_hint,
            geometry.footer_font_size,
            footer_spacing);
        DrawTextEx(
            fonts.text,
            scroll_hint.c_str(),
            Vector2{
                geometry.panel.x + geometry.panel.width
                    - geometry.padding - hint_size.x,
                footer_y,
            },
            geometry.footer_font_size,
            footer_spacing,
            kMutedText);

        if (first_row > 0) {
            const std::string more_hint = "^ more";
            const Vector2 more_size = Measure(
                fonts.text,
                more_hint,
                geometry.footer_font_size,
                footer_spacing);
            DrawTextEx(
                fonts.text,
                more_hint.c_str(),
                Vector2{
                    geometry.content.x + geometry.content.width - more_size.x,
                    geometry.content.y,
                },
                geometry.footer_font_size,
                footer_spacing,
                kAccent);
        }
        if (first_row < max_scroll_rows) {
            const std::string more_hint = "v more";
            const Vector2 more_size = Measure(
                fonts.text,
                more_hint,
                geometry.footer_font_size,
                footer_spacing);
            DrawTextEx(
                fonts.text,
                more_hint.c_str(),
                Vector2{
                    geometry.content.x + geometry.content.width - more_size.x,
                    geometry.content.y + geometry.content.height
                        - geometry.footer_font_size,
                },
                geometry.footer_font_size,
                footer_spacing,
                kAccent);
        }
    }
}

int SelectionInfoOverlayMaxScrollRows(
    const WorkspaceState& workspace,
    const UiLayoutCache& layout)
{
    const SelectionInfoOverlayGeometry geometry = BuildSelectionInfoOverlayGeometry(layout);
    const int total_rows = static_cast<int>(BuildInspectPanelLines(workspace).size());
    return std::max(0, total_rows - geometry.visible_rows);
}

void DrawSelectionInfoOverlay(
    const WorkspaceState& workspace,
    int first_visible_row,
    const UiFontSet& fonts,
    const UiLayoutCache& layout)
{
    const UiMetrics& metrics = layout.metrics;
    const SelectionInfoOverlayGeometry geometry = BuildSelectionInfoOverlayGeometry(layout);
    const std::vector<std::string> lines = BuildInspectPanelLines(workspace);
    const int max_scroll_rows = std::max(
        0,
        static_cast<int>(lines.size()) - geometry.visible_rows);
    const int first_row = std::clamp(first_visible_row, 0, max_scroll_rows);

    DrawRectangle(0, 0, metrics.window_width, metrics.window_height, kModalDim);
    DrawRectangleRounded(geometry.panel, 0.04F, 8, kPanel);
    DrawRectangleRoundedLinesEx(
        geometry.panel,
        0.04F,
        8,
        std::max(1.0F, metrics.modal_border_width),
        kPanelBorder);

    const std::string title = "[ Selection Info ]";
    DrawTextEx(
        fonts.text,
        title.c_str(),
        Vector2{geometry.panel.x + geometry.padding, geometry.panel.y + geometry.padding},
        geometry.title_font_size,
        FontSpacing(geometry.title_font_size),
        kSelectedText);

    BeginScissorMode(
        static_cast<int>(std::floor(geometry.content.x)),
        static_cast<int>(std::floor(geometry.content.y)),
        static_cast<int>(std::ceil(geometry.content.width)),
        static_cast<int>(std::ceil(geometry.content.height)));
    float line_y = geometry.content.y;
    const int last_row = std::min(
        static_cast<int>(lines.size()),
        first_row + geometry.visible_rows);
    for (int index = first_row; index < last_row; ++index) {
        const std::string& line = lines[static_cast<std::size_t>(index)];
        if (!line.empty()) {
            const bool section_header = line.rfind("  ", 0) != 0;
            DrawTextEx(
                fonts.text,
                line.c_str(),
                Vector2{geometry.content.x, line_y},
                geometry.body_font_size,
                FontSpacing(geometry.body_font_size),
                section_header ? kAccent : kText);
        }
        line_y += geometry.line_height;
    }
    EndScissorMode();

    const float footer_y = geometry.panel.y + geometry.panel.height
        - geometry.padding - geometry.footer_font_size;
    const std::string close_hint = "Esc  Close";
    DrawTextEx(
        fonts.text,
        close_hint.c_str(),
        Vector2{geometry.panel.x + geometry.padding, footer_y},
        geometry.footer_font_size,
        FontSpacing(geometry.footer_font_size),
        kMutedText);

    if (max_scroll_rows > 0) {
        const std::string scroll_hint = "Wheel / PgUp / PgDn";
        const float footer_spacing = FontSpacing(geometry.footer_font_size);
        const Vector2 hint_size = Measure(
            fonts.text,
            scroll_hint,
            geometry.footer_font_size,
            footer_spacing);
        DrawTextEx(
            fonts.text,
            scroll_hint.c_str(),
            Vector2{
                geometry.panel.x + geometry.panel.width - geometry.padding - hint_size.x,
                footer_y,
            },
            geometry.footer_font_size,
            footer_spacing,
            kMutedText);

        if (first_row > 0) {
            DrawTextEx(
                fonts.text,
                "^ more",
                Vector2{
                    geometry.content.x + geometry.content.width
                        - Measure(
                              fonts.text,
                              "^ more",
                              geometry.footer_font_size,
                              footer_spacing)
                              .x,
                    geometry.content.y,
                },
                geometry.footer_font_size,
                footer_spacing,
                kAccent);
        }
        if (first_row < max_scroll_rows) {
            const std::string more_hint = "v more";
            const Vector2 more_size = Measure(
                fonts.text,
                more_hint,
                geometry.footer_font_size,
                footer_spacing);
            DrawTextEx(
                fonts.text,
                more_hint.c_str(),
                Vector2{
                    geometry.content.x + geometry.content.width - more_size.x,
                    geometry.content.y + geometry.content.height - geometry.footer_font_size,
                },
                geometry.footer_font_size,
                footer_spacing,
                kAccent);
        }
    }
}

void DrawWorkspace(
    const WorkspaceState& workspace_state,
    const Map2DView* map_2d_view,
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
    const float mode_spacing = FontSpacing(metrics.workspace_tab_font_size);
    for (const WorkspaceModeButtonBounds& button : workspace.mode_buttons) {
        DrawWorkspaceModeButton(
            fonts.text,
            button,
            labels,
            metrics.workspace_tab_font_size,
            mode_spacing);
    }

    if (workspace_state.selected_panel_tab == WorkspacePanelTab::kMenu) {
        const float marker_column_width = Measure(
            fonts.text,
            "[x] ",
            metrics.workspace_tool_font_size,
            spacing)
            .x;
        for (const auto& item_bounds : workspace.panel_items) {
            const WorkspacePanelItemState item{
                item_bounds.item,
                item_bounds.kind,
                item_bounds.depth,
                item_bounds.enabled,
                item_bounds.checked,
            };
            const std::string marker = WorkspacePanelItemMarker(item);
            const std::string label = WorkspacePanelItemLabel(item.item, workspace_state, labels);
            const bool is_group = item_bounds.kind == WorkspacePanelItemKind::kGroup;
            const Color color = is_group
                ? kEditorViewportText
                : (!item_bounds.enabled ? Color{74, 138, 154, 255} : (item_bounds.checked ? kAccent : kEditorPanelText));
            DrawTextEx(
                fonts.text,
                marker.c_str(),
                item_bounds.text_position,
                metrics.workspace_tool_font_size,
                spacing,
                color);
            DrawTextEx(
                fonts.text,
                label.c_str(),
                Vector2{item_bounds.text_position.x + marker_column_width, item_bounds.text_position.y},
                metrics.workspace_tool_font_size,
                spacing,
                color);
        }
        if (workspace.panel_can_scroll_up || workspace.panel_can_scroll_down) {
            const float hint_size = std::max(10.0F, metrics.workspace_status_font_size - 2.0F);
            const float hint_spacing = FontSpacing(hint_size);
            if (workspace.panel_can_scroll_up) {
                DrawTextEx(
                    fonts.text,
                    "^ more",
                    Vector2{workspace.tool_menu.x + metrics.workspace_tool_gap, workspace.tool_menu.y},
                    hint_size,
                    hint_spacing,
                    kAccent);
            }
            if (workspace.panel_can_scroll_down) {
                DrawTextEx(
                    fonts.text,
                    "v more",
                    Vector2{
                        workspace.tool_menu.x + metrics.workspace_tool_gap,
                        workspace.tool_menu.y + workspace.tool_menu.height - hint_size - metrics.workspace_tool_gap,
                    },
                    hint_size,
                    hint_spacing,
                    kAccent);
            }
        }
    } else if (workspace_state.selected_panel_tab == WorkspacePanelTab::kStats) {
        const std::vector<TextPanelRow> rows = BuildStatsTextPanelRows(workspace_state, camera_status, labels);
        const float marker_column_width = Measure(
            fonts.text,
            "[x] ",
            metrics.workspace_tool_font_size,
            spacing)
            .x;
        const float item_height = metrics.workspace_tool_font_size + metrics.workspace_tool_gap * 1.05F;
        const float indent_step = metrics.workspace_tool_gap * 1.35F;
        const float text_panel_left_padding = metrics.workspace_tool_gap * 0.35F;
        const float text_panel_right = workspace.tool_menu.x + workspace.tool_menu.width;
        const Vector2 mouse = GetMousePosition();
        std::string hovered_overflow_text;
        const float bottom = workspace.tool_menu.y + workspace.tool_menu.height;
        float row_y = workspace.tool_menu.y;

        for (int index = workspace.panel_first_visible_row; index < static_cast<int>(rows.size()); ++index) {
            const TextPanelRow& row = rows[static_cast<std::size_t>(index)];
            if (row.kind == TextPanelRowKind::kBlank) {
                row_y += item_height * 0.55F;
                continue;
            }
            if (row_y + item_height > bottom) {
                break;
            }

            const bool group = row.kind == TextPanelRowKind::kGroup;
            const Color color = group ? kEditorViewportText : kEditorPanelText;
            const std::string marker = group
                ? WorkspacePanelItemMarker(WorkspacePanelItemState{
                      row.item,
                      WorkspacePanelItemKind::kGroup,
                      row.depth,
                      true,
                      row.checked,
                  })
                : "";
            const float indent = indent_step * static_cast<float>(std::max(0, row.depth));
            const Vector2 marker_position{
                workspace.tool_menu.x + text_panel_left_padding + indent,
                row_y + (item_height - metrics.workspace_tool_font_size) * 0.5F,
            };
            const Vector2 label_position{
                marker_position.x + marker_column_width,
                marker_position.y,
            };

            if (!marker.empty()) {
                DrawTextEx(
                    fonts.text,
                    marker.c_str(),
                    marker_position,
                    metrics.workspace_tool_font_size,
                    spacing,
                    color);
            }
            DrawTextEx(
                fonts.text,
                row.label.c_str(),
                label_position,
                metrics.workspace_tool_font_size,
                spacing,
                color);

            const Rectangle row_bounds{workspace.tool_menu.x, row_y, workspace.tool_menu.width, item_height};
            const float label_width = Measure(fonts.text, row.label, metrics.workspace_tool_font_size, spacing).x;
            if (label_position.x + label_width > text_panel_right && CheckCollisionPointRec(mouse, row_bounds)) {
                hovered_overflow_text = row.label;
            }
            row_y += item_height;
        }
        if (workspace.panel_can_scroll_up || workspace.panel_can_scroll_down) {
            const float hint_size = std::max(10.0F, metrics.workspace_status_font_size - 2.0F);
            const float hint_spacing = FontSpacing(hint_size);
            if (workspace.panel_can_scroll_up) {
                DrawTextEx(
                    fonts.text,
                    "^ more",
                    Vector2{workspace.tool_menu.x + metrics.workspace_tool_gap, workspace.tool_menu.y},
                    hint_size,
                    hint_spacing,
                    kAccent);
            }
            if (workspace.panel_can_scroll_down) {
                DrawTextEx(
                    fonts.text,
                    "v more",
                    Vector2{
                        workspace.tool_menu.x + metrics.workspace_tool_gap,
                        workspace.tool_menu.y + workspace.tool_menu.height - hint_size - metrics.workspace_tool_gap,
                    },
                    hint_size,
                    hint_spacing,
                    kAccent);
            }
        }
        if (!hovered_overflow_text.empty()) {
            DrawWorkspaceTooltip(
                fonts.text,
                hovered_overflow_text,
                mouse,
                metrics,
                metrics.workspace_tool_font_size,
                spacing);
        }
    } else {
        const std::vector<std::string> lines = BuildHelpPanelLines();

        const float compact_size = std::max(10.0F, metrics.workspace_status_font_size - 1.0F);
        const float compact_spacing = FontSpacing(compact_size);
        const float bottom = workspace.tool_menu.y + workspace.tool_menu.height;
        float line_y = workspace.tool_menu.y;
        for (const auto& line : lines) {
            if (line_y + compact_size > bottom) {
                break;
            }
            const bool section_header = !line.empty() && line.find("  ") != 0;
            DrawTextEx(
                fonts.text,
                line.c_str(),
                Vector2{workspace.tool_menu.x + (line.empty() ? 0.0F : metrics.workspace_tool_gap), line_y},
                compact_size,
                compact_spacing,
                section_header ? kEditorViewportText : kEditorPanelText);
            line_y += compact_size + metrics.workspace_tool_gap * (line.empty() ? 0.75F : 0.35F);
        }
    }

    if (workspace_state.show_3d_preview && mesh_preview != nullptr && preview_camera != nullptr && mesh_preview->IsUploaded()) {
        DrawRectangleRec(workspace.map_overview, Color{18, 22, 24, 255});
        const RaylibChunkMeshDebugOverlayOptions overlays{
            workspace_state.show_3d_chunk_bounds,
            workspace_state.show_3d_world_grid,
            workspace_state.show_3d_collision_overlay,
            workspace_state.show_3d_height_overlay,
            workspace_state.show_3d_object_trees,
            workspace_state.show_3d_object_bushes,
            workspace_state.show_3d_object_reeds,
            workspace_state.show_3d_object_ruins,
            workspace_state.show_3d_object_cover,
            workspace_state.show_3d_object_loot,
            workspace_state.show_3d_object_structures,
            workspace_state.show_3d_object_trenches,
            workspace_state.show_3d_object_unknown,
        };
        RaylibChunkVisibilityMode raylib_visibility_mode = RaylibChunkVisibilityMode::kAllChunks;
        if (workspace_state.visibility_mode == WorkspaceVisibilityMode::kRadiusFade) {
            raylib_visibility_mode = RaylibChunkVisibilityMode::kRadiusFade;
        } else if (workspace_state.visibility_mode == WorkspaceVisibilityMode::kHardCull) {
            raylib_visibility_mode = RaylibChunkVisibilityMode::kHardCull;
        } else if (workspace_state.visibility_mode == WorkspaceVisibilityMode::kFrustumCull) {
            raylib_visibility_mode = RaylibChunkVisibilityMode::kFrustumCull;
        }
        const RaylibChunkVisibilityOptions visibility{
            raylib_visibility_mode,
            workspace_state.visibility_radius_chunks,
            workspace_state.visibility_fade_ring_chunks,
            workspace_state.show_3d_hidden_chunk_bounds,
            workspace.map_overview.width > 1.0F && workspace.map_overview.height > 1.0F
                ? workspace.map_overview.width / workspace.map_overview.height
                : 1.0F,
        };
        mesh_preview->Draw(
            workspace.map_overview,
            workspace_state.chunk_meshes,
            *preview_camera,
            &workspace_state.runtime_map,
            &workspace_state.chunk_grid,
            overlays,
            visibility,
            RaylibTerrainPassOptions{
                workspace_state.show_terrain_tops,
                workspace_state.show_terrain_walls,
                workspace_state.show_terrain_cliffs,
            },
            &workspace_state.transition_features,
            RaylibTransitionOverlayOptions{
                workspace_state.show_transition_overlay,
                workspace_state.show_transition_ramps,
                workspace_state.show_transition_stairs,
                workspace_state.show_transition_bridges,
                workspace_state.show_transition_drops,
            },
            RaylibTileSelectionOverlayOptions{
                workspace_state.selected_tile.IsValid(),
                workspace_state.selected_tile.tile,
            },
            workspace_state.movement_probe.IsValid() ? &workspace_state.movement_probe : nullptr,
            RaylibMovementProbeOverlayOptions{
                workspace_state.show_movement_probe,
            },
            workspace_state.path_probe.IsValid() ? &workspace_state.path_probe : nullptr,
            RaylibPathProbeOverlayOptions{
                workspace_state.show_path_overlay,
                workspace_state.show_path_visited,
            },
            workspace_state.passability_validation.IsValid() ? &workspace_state.passability_validation : nullptr,
            RaylibPassabilityValidationOverlayOptions{
                workspace_state.show_passability_issues,
                workspace_state.show_passability_invalid_transitions,
                workspace_state.show_passability_blocked_transitions,
                workspace_state.show_passability_suspicious_drops,
                workspace_state.show_passability_isolated_tiles,
            });
        DrawRectangleLinesEx(workspace.map_overview, metrics.workspace_border_width, Color{235, 235, 220, 255});
    } else {
        if (map_2d_view != nullptr && map_2d_view->IsLoaded()) {
            Map2DOverlayOptions overlays;
            overlays.show_grid = workspace_state.show_grid_layer;
            overlays.show_chunks = workspace_state.show_2d_chunks;
            overlays.chunk_size_x = workspace_state.chunk_grid.info.chunk_size_x;
            overlays.chunk_size_y = workspace_state.chunk_grid.info.chunk_size_y;
            overlays.show_vxmap_regions = workspace_state.show_2d_vxmap_regions;
            overlays.vxmap_region_size_tiles =
                workspace_state.map.runtime_binary.region_size_tiles;
            overlays.show_start_goal = workspace_state.show_2d_start_goal;
            overlays.start = workspace_state.runtime_map.info.start;
            overlays.goal = workspace_state.runtime_map.info.goal;
            overlays.show_objects = workspace_state.show_2d_objects;
            overlays.objects = workspace_state.runtime_map.runtime_objects;
            overlays.show_vegetation = workspace_state.show_2d_vegetation;
            overlays.vegetation_markers = workspace_state.runtime_map.object_markers;
            overlays.show_places = workspace_state.show_2d_places;
            overlays.places = workspace_state.runtime_map.places;
            overlays.show_markers = workspace_state.show_2d_markers;
            overlays.markers = workspace_state.runtime_map.markers;
            overlays.selection = workspace_state.selected_tile.IsValid()
                ? std::optional<TileCoord>{workspace_state.selected_tile.tile}
                : std::nullopt;
            map_2d_view->Draw(
                workspace.map_overview,
                workspace_state.map_2d_base_layer,
                overlays);
            DrawMap2DLegend(
                fonts.text,
                workspace.map_overview,
                workspace_state.map_2d_base_layer,
                std::max(12.0F, metrics.workspace_status_font_size * 0.80F));
        }
        if (map_2d_view == nullptr || !map_2d_view->IsLoaded()) {
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

    const Map2DViewStatus map_2d_status = map_2d_view != nullptr
        ? map_2d_view->Status()
        : Map2DViewStatus{};
    const std::string status = CompactStatusText(workspace_state, map_2d_status, labels);
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
    const ProcessMemoryInfo& memory,
    std::string_view version)
{
    const UiMetrics& metrics = layout.metrics;
    const Font font = fonts.text;
    const std::string text = "v" + std::string(version) + " | " + labels.fps_label + ": " + std::to_string(GetFPS())
        + " | " + labels.memory_label + ": "
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
    lines[7] = labels.debug_workspace_tool + ": " + std::string(ToString(workspace.selected_panel_tab));
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
