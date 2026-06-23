#include "vox3d/render/chunk_visibility.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace vox3d {
namespace {

[[nodiscard]] bool IsFinite(float value)
{
    return std::isfinite(value);
}

[[nodiscard]] Vec3f PositiveVertex(const Aabb3f& bounds, Vec3f normal)
{
    return Vec3f{
        normal.x >= 0.0F ? bounds.max.x : bounds.min.x,
        normal.y >= 0.0F ? bounds.max.y : bounds.min.y,
        normal.z >= 0.0F ? bounds.max.z : bounds.min.z,
    };
}

[[nodiscard]] ChunkVisibilityClass ClassifyRadius(
    ChunkCoord chunk,
    ChunkCoord camera_chunk,
    ChunkVisibilityMode mode,
    int radius_chunks,
    int fade_ring_chunks)
{
    const int radius = std::max(0, radius_chunks);
    const int fade_ring = std::max(0, fade_ring_chunks);
    const int distance = std::max(
        std::abs(chunk.x - camera_chunk.x),
        std::abs(chunk.y - camera_chunk.y));

    if (distance <= radius) {
        return ChunkVisibilityClass::kVisible;
    }
    if (mode == ChunkVisibilityMode::kRadiusFade && distance <= radius + fade_ring) {
        return ChunkVisibilityClass::kFade;
    }
    return ChunkVisibilityClass::kHidden;
}

[[nodiscard]] ChunkVisibilityClass ClassifyItem(
    const ChunkVisibilityItem& item,
    const ChunkVisibilityOptions& options)
{
    switch (options.mode) {
        case ChunkVisibilityMode::kAllChunks:
            return ChunkVisibilityClass::kVisible;
        case ChunkVisibilityMode::kRadiusFade:
        case ChunkVisibilityMode::kHardCull:
            return ClassifyRadius(
                item.coord,
                options.camera_chunk,
                options.mode,
                options.radius_chunks,
                options.fade_ring_chunks);
        case ChunkVisibilityMode::kFrustumCull:
            if (!options.frustum.IsValid() || !item.bounds.IsValid()) {
                return ChunkVisibilityClass::kVisible;
            }
            return options.frustum.Intersects(item.bounds)
                ? ChunkVisibilityClass::kVisible
                : ChunkVisibilityClass::kHidden;
    }
    return ChunkVisibilityClass::kVisible;
}

void AccumulateEntry(ChunkVisibilityReport& report, const ChunkVisibilityEntry& entry)
{
    ++report.resident_chunks;
    report.total_faces += entry.faces;

    if (entry.visibility_class == ChunkVisibilityClass::kHidden) {
        ++report.hidden_chunks;
        ++report.culled_models;
        report.culled_faces += entry.faces;
        return;
    }

    ++report.drawn_models;
    report.drawn_faces += entry.faces;
    if (entry.visibility_class == ChunkVisibilityClass::kFade) {
        ++report.fade_chunks;
    } else {
        ++report.visible_chunks;
    }
}

}  // namespace

bool Aabb3f::IsValid() const
{
    return IsFinite(min.x) && IsFinite(min.y) && IsFinite(min.z)
        && IsFinite(max.x) && IsFinite(max.y) && IsFinite(max.z)
        && min.x <= max.x && min.y <= max.y && min.z <= max.z;
}

float Plane3f::SignedDistance(Vec3f point) const
{
    return normal.x * point.x + normal.y * point.y + normal.z * point.z + distance;
}

bool Frustum3f::IsValid() const
{
    return valid;
}

bool Frustum3f::Intersects(Aabb3f bounds) const
{
    if (!IsValid() || !bounds.IsValid()) {
        return true;
    }

    for (const Plane3f& plane : planes) {
        if (plane.SignedDistance(PositiveVertex(bounds, plane.normal)) < 0.0F) {
            return false;
        }
    }
    return true;
}

bool ChunkVisibilityReport::IsValid() const
{
    return resident_chunks > 0 && resident_chunks == drawn_models + culled_models;
}

double ChunkVisibilityReport::DrawSavedRatio() const
{
    return resident_chunks == 0 ? 0.0 : static_cast<double>(culled_models) / static_cast<double>(resident_chunks);
}

double ChunkVisibilityReport::FaceSavedRatio() const
{
    return total_faces == 0 ? 0.0 : static_cast<double>(culled_faces) / static_cast<double>(total_faces);
}

std::string_view ToString(ChunkVisibilityMode mode)
{
    switch (mode) {
        case ChunkVisibilityMode::kAllChunks:
            return "all_chunks";
        case ChunkVisibilityMode::kRadiusFade:
            return "radius_fade";
        case ChunkVisibilityMode::kHardCull:
            return "hard_cull";
        case ChunkVisibilityMode::kFrustumCull:
            return "frustum_cull";
    }
    return "unknown";
}

std::string_view ToString(ChunkVisibilityClass visibility_class)
{
    switch (visibility_class) {
        case ChunkVisibilityClass::kVisible:
            return "visible";
        case ChunkVisibilityClass::kFade:
            return "fade";
        case ChunkVisibilityClass::kHidden:
            return "hidden";
    }
    return "unknown";
}

ChunkVisibilityReport BuildChunkVisibility(
    std::span<const ChunkVisibilityItem> items,
    const ChunkVisibilityOptions& options)
{
    ChunkVisibilityReport report;
    report.mode = options.mode;
    report.radius_chunks = std::max(0, options.radius_chunks);
    report.fade_ring_chunks = std::max(0, options.fade_ring_chunks);
    report.entries.reserve(items.size());

    for (const ChunkVisibilityItem& item : items) {
        ChunkVisibilityEntry entry;
        entry.coord = item.coord;
        entry.faces = item.faces;
        entry.visibility_class = ClassifyItem(item, options);
        AccumulateEntry(report, entry);
        report.entries.push_back(entry);
    }

    return report;
}

std::string ToLogString(const ChunkVisibilityReport& report)
{
    std::ostringstream out;
    out << "mode=" << ToString(report.mode);
    out << " radius=" << report.radius_chunks;
    out << " fade_ring=" << report.fade_ring_chunks;
    out << " resident=" << report.resident_chunks;
    out << " visible=" << report.visible_chunks;
    out << " fade=" << report.fade_chunks;
    out << " hidden=" << report.hidden_chunks;
    out << " drawn_models=" << report.drawn_models << '/' << report.resident_chunks;
    out << " faces=" << report.drawn_faces << '/' << report.total_faces;
    out << " draw_saved=" << std::fixed << std::setprecision(1) << report.DrawSavedRatio() * 100.0 << '%';
    out << " face_saved=" << std::fixed << std::setprecision(1) << report.FaceSavedRatio() * 100.0 << '%';
    return out.str();
}

}  // namespace vox3d
