#include "ui_draw.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
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

[[nodiscard]] std::string ColorModeLabel(WorkspaceColorMode mode)
{
    switch (mode) {
        case WorkspaceColorMode::kMaterial:
            return "material";
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

[[nodiscard]] std::string PathStatusCompactText(const WorkspaceState& workspace_state)
{
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

[[nodiscard]] std::string CompactStatusText(const WorkspaceState& workspace_state, const UiLabels& labels)
{
    const std::string preview_mode = workspace_state.show_3d_preview ? "3D" : "2D";
    if (!workspace_state.chunk_meshes.IsValid()) {
        return labels.workspace_status_ready + " | " + preview_mode + " | " + MapStatusLabel(workspace_state.map, labels);
    }
    if (workspace_state.show_3d_preview && workspace_state.visibility_stats.resident_chunks > 0) {
        return labels.workspace_status_ready + " | " + preview_mode + " | " + MeshModeLabel(workspace_state.mesh_mode)
            + " | " + ColorModeLabel(workspace_state.color_mode)
            + " | " + VisibilityModeLabel(workspace_state.visibility_mode)
            + " | Visible " + std::to_string(workspace_state.visibility_stats.visible_chunks) + "/"
            + std::to_string(workspace_state.visibility_stats.resident_chunks)
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

enum class StatsPanelRowKind {
    kBlank,
    kGroup,
    kValue,
};

struct StatsPanelRow {
    StatsPanelRowKind kind = StatsPanelRowKind::kBlank;
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

[[nodiscard]] std::string TrimStatsIndent(std::string_view line)
{
    const std::size_t first = line.find_first_not_of(' ');
    if (first == std::string_view::npos) {
        return {};
    }
    return std::string(line.substr(first));
}

[[nodiscard]] std::vector<StatsPanelRow> BuildStatsPanelRows(
    const WorkspaceState& workspace_state,
    FreeFlyCameraStatus camera_status,
    const UiLabels& labels)
{
    const std::vector<std::string> lines = BuildStatsPanelLines(workspace_state, camera_status, labels);
    std::vector<StatsPanelRow> rows;
    rows.reserve(lines.size());

    WorkspacePanelItem current_group = WorkspacePanelItem::kStatsMapGroup;
    bool current_group_expanded = true;

    for (const std::string& line : lines) {
        if (line.empty()) {
            if (current_group_expanded) {
                rows.push_back(StatsPanelRow{StatsPanelRowKind::kBlank, current_group, 0, {}, false});
            }
            continue;
        }

        const bool value_row = line.size() >= 2 && line[0] == ' ' && line[1] == ' ';
        if (!value_row) {
            current_group = StatsGroupItemForLabel(line);
            current_group_expanded = !IsWorkspacePanelGroupCollapsed(workspace_state, current_group);
            rows.push_back(StatsPanelRow{
                StatsPanelRowKind::kGroup,
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

        rows.push_back(StatsPanelRow{
            StatsPanelRowKind::kValue,
            current_group,
            1,
            TrimStatsIndent(line),
            false,
        });
    }

    return rows;
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

[[nodiscard]] std::vector<std::string> BuildInspectPanelLines(const WorkspaceState& workspace_state)
{
    std::vector<std::string> lines;
    lines.push_back("Selection");
    if (!workspace_state.selected_tile.IsValid()) {
        lines.push_back("  Tile: none");
        lines.push_back("  Chunk: none");
        lines.push_back("  Transitions: none");
        lines.push_back("");
        lines.push_back("Click terrain in 3D preview");
        lines.push_back("to inspect a tile.");
        return lines;
    }

    const TileInspectResult& tile = workspace_state.selected_tile;
    lines.push_back("  Tile: " + TileCoordText(tile.tile));
    lines.push_back("  Terrain: " + tile.terrain);
    lines.push_back("  Elevation: " + std::to_string(tile.elevation));
    lines.push_back("  Collision: " + std::string(tile.blocked ? "blocked" : "free"));
    if (tile.chunk_found) {
        lines.push_back("  Chunk: " + TileCoordText(TileCoord{tile.chunk.x, tile.chunk.y}));
        lines.push_back("  Bounds: " + ChunkBoundsText(tile.chunk_bounds));
    } else {
        lines.push_back("  Chunk: none");
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

[[nodiscard]] std::vector<std::string> BuildHelpPanelLines()
{
    return {
        "Hotkeys",
        "  F2   Release mouse",
        "  F3   2D / 3D",
        "  F8   Mesh mode",
        "  F9   Chunk size",
        "  F10  Dirty rebuild probe",
        "  F11  Color mode",
        "  F12  Visibility mode",
        "  T    Toggle transitions",
        "  M    Toggle movement probe",
        "  V    Toggle passability issues",
        "  P    Start two-click path pick",
        "  LMB  Pick start / goal in path pick",
        "  X    Clear path probe",
        "  RMB  Cancel path pick / release mouse",
        "  Menu Validation -> Run Validation",
        "  LMB  Pick tile in Select mode",
        "  F    Fit view",
        "  R    Reset camera",
        "  Esc  Release mouse first, then exit",
        "",
        "3D Camera",
        "  Click viewport to capture mouse",
        "  WASD move, Q/E down/up",
        "  Shift fast, Ctrl slow",
        "  Wheel dolly",
    };
}

[[nodiscard]] std::string WorkspacePanelTabLabel(WorkspacePanelTab tab)
{
    switch (tab) {
        case WorkspacePanelTab::kMenu:
            return "View";
        case WorkspacePanelTab::kStats:
            return "Stats";
        case WorkspacePanelTab::kInspect:
            return "Info";
        case WorkspacePanelTab::kHelp:
            return "Help";
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


[[nodiscard]] std::string WorkspacePanelItemLabel(WorkspacePanelItem item, const UiLabels& labels)
{
    switch (item) {
        case WorkspacePanelItem::kMenuModeGroup:
            return "Mode";
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
            return "View";
        case WorkspacePanelItem::kViewFitMap:
            return labels.workspace_subitem_fit_view;
        case WorkspacePanelItem::kViewResetView:
            return labels.workspace_subitem_reset_view;
        case WorkspacePanelItem::k3DCaptureMouse:
            return "Capture Mouse";
        case WorkspacePanelItem::k3DReleaseMouse:
            return "Release Mouse";
        case WorkspacePanelItem::k3DRenderGroup:
            return "Display";
        case WorkspacePanelItem::kRenderTerrainMesh:
            return "Terrain Mesh";
        case WorkspacePanelItem::k3DColorModeGroup:
            return "Color";
        case WorkspacePanelItem::k3DColorMaterial:
            return "Material";
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
        case WorkspacePanelItem::k3DPathToolSelect:
            return "Cancel Pick";
        case WorkspacePanelItem::k3DPathToolPickStart:
            return "Pick Start";
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


[[nodiscard]] WorkspaceLayout BuildWorkspaceLayout(
    const UiFontSet& fonts,
    const UiMetrics& metrics,
    const WorkspaceState& workspace_state,
    FreeFlyCameraStatus camera_status,
    const UiLabels& labels)
{
    WorkspaceLayout layout;
    layout.panel_tabs.reserve(3);

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
    const float tab_height = metrics.workspace_tool_font_size + gap * 1.45F;
    const float item_height = metrics.workspace_tool_font_size + gap * 1.05F;
    const float subitem_indent = gap * 3.0F;

    layout.tool_header = Rectangle{
        tool_x,
        layout.tool_panel.y + padding,
        tool_width,
        tab_height,
    };

    constexpr std::array<WorkspacePanelTab, 3> tabs{
        WorkspacePanelTab::kMenu,
        WorkspacePanelTab::kStats,
        WorkspacePanelTab::kInspect,
    };
    const float tab_spacing = Measure(
        fonts.text,
        "  ",
        metrics.workspace_tool_font_size,
        FontSpacing(metrics.workspace_tool_font_size))
                                  .x;
    float tab_x = tool_x;
    for (const WorkspacePanelTab tab : tabs) {
        const std::string tab_label = "[" + WorkspacePanelTabLabel(tab) + "]";
        const Vector2 label_size = Measure(
            fonts.text,
            tab_label,
            metrics.workspace_tool_font_size,
            FontSpacing(metrics.workspace_tool_font_size));
        const Rectangle bounds{
            tab_x,
            layout.tool_header.y,
            label_size.x,
            tab_height,
        };
        layout.panel_tabs.push_back(WorkspacePanelTabBounds{
            tab,
            bounds,
            Vector2{bounds.x, bounds.y + (bounds.height - metrics.workspace_tool_font_size) * 0.5F},
        });
        tab_x += label_size.x + tab_spacing;
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
    const std::vector<StatsPanelRow> stats_rows = workspace_state.selected_panel_tab == WorkspacePanelTab::kStats
        ? BuildStatsPanelRows(workspace_state, camera_status, labels)
        : std::vector<StatsPanelRow>{};

    layout.panel_total_rows = workspace_state.selected_panel_tab == WorkspacePanelTab::kStats
        ? static_cast<int>(stats_rows.size())
        : static_cast<int>(selected_items.size());
    layout.panel_visible_rows = static_cast<int>(std::max(0.0F, std::floor((row_bottom - content_y) / item_height)));
    const int max_scroll_rows = std::max(0, layout.panel_total_rows - layout.panel_visible_rows);
    layout.panel_first_visible_row = std::clamp(workspace_state.menu_scroll_rows, 0, max_scroll_rows);
    layout.panel_can_scroll_up = layout.panel_first_visible_row > 0;
    layout.panel_can_scroll_down = layout.panel_first_visible_row < max_scroll_rows;

    float row_y = content_y;
    layout.panel_items.reserve(static_cast<std::size_t>(std::max(0, layout.panel_visible_rows)));
    if (workspace_state.selected_panel_tab == WorkspacePanelTab::kStats) {
        for (int row = layout.panel_first_visible_row; row < layout.panel_total_rows; ++row) {
            if (row_y + item_height > row_bottom) {
                break;
            }
            const StatsPanelRow& item = stats_rows[static_cast<std::size_t>(row)];
            if (item.kind == StatsPanelRowKind::kGroup) {
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
            row_y += item.kind == StatsPanelRowKind::kBlank ? item_height * 0.55F : item_height;
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
    for (const auto& tab_bounds : workspace.panel_tabs) {
        const bool selected = workspace_state.selected_panel_tab == tab_bounds.tab;
        const std::string tab_label = "[" + WorkspacePanelTabLabel(tab_bounds.tab) + "]";
        const Color tab_color = selected ? kAccent : kEditorViewportText;
        DrawTextEx(
            fonts.text,
            tab_label.c_str(),
            tab_bounds.text_position,
            metrics.workspace_tool_font_size,
            spacing,
            tab_color);
        if (selected) {
            DrawTextEx(
                fonts.text,
                tab_label.c_str(),
                Vector2{tab_bounds.text_position.x + 1.0F, tab_bounds.text_position.y},
                metrics.workspace_tool_font_size,
                spacing,
                tab_color);
        }
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
            const std::string label = WorkspacePanelItemLabel(item.item, labels);
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
        const std::vector<StatsPanelRow> rows = BuildStatsPanelRows(workspace_state, camera_status, labels);
        const float marker_column_width = Measure(
            fonts.text,
            "[x] ",
            metrics.workspace_tool_font_size,
            spacing)
            .x;
        const float item_height = metrics.workspace_tool_font_size + metrics.workspace_tool_gap * 1.05F;
        const float indent_step = metrics.workspace_tool_gap * 3.0F;
        const float bottom = workspace.tool_menu.y + workspace.tool_menu.height;
        float row_y = workspace.tool_menu.y;

        for (int index = workspace.panel_first_visible_row; index < static_cast<int>(rows.size()); ++index) {
            const StatsPanelRow& row = rows[static_cast<std::size_t>(index)];
            if (row.kind == StatsPanelRowKind::kBlank) {
                row_y += item_height * 0.55F;
                continue;
            }
            if (row_y + item_height > bottom) {
                break;
            }

            const bool group = row.kind == StatsPanelRowKind::kGroup;
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
                workspace.tool_menu.x + metrics.workspace_tool_gap + indent,
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
    } else {
        std::vector<std::string> lines;
        if (workspace_state.selected_panel_tab == WorkspacePanelTab::kInspect) {
            lines = BuildInspectPanelLines(workspace_state);
        } else {
            lines = BuildHelpPanelLines();
        }

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

    const std::string status = CompactStatusText(workspace_state, labels);
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
