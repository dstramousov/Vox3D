#include "vox3d/mesh/chunk_mesh_cache.hpp"

#include "vox3d/mesh/chunk_mesh_builder.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <sstream>

namespace vox3d {
namespace {

[[nodiscard]] std::uint64_t ExpectedChunkCount(int chunks_x, int chunks_y)
{
    if (chunks_x <= 0 || chunks_y <= 0) {
        return 0;
    }
    return static_cast<std::uint64_t>(chunks_x) * static_cast<std::uint64_t>(chunks_y);
}

[[nodiscard]] std::optional<std::size_t> ChunkIndex(ChunkCoord coord, int chunks_x, int chunks_y)
{
    if (coord.x < 0 || coord.y < 0 || coord.x >= chunks_x || coord.y >= chunks_y) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(coord.y) * static_cast<std::size_t>(chunks_x)
        + static_cast<std::size_t>(coord.x);
}

[[nodiscard]] std::optional<ChunkCoord> TileChunkCoord(TileCoord tile, const ChunkGrid& chunks)
{
    if (!chunks.IsValid() || tile.x < 0 || tile.y < 0 || tile.x >= chunks.info.map_width || tile.y >= chunks.info.map_height) {
        return std::nullopt;
    }
    return ChunkCoord{tile.x / chunks.info.chunk_size_x, tile.y / chunks.info.chunk_size_y};
}

[[nodiscard]] double DeltaRatio(std::uint64_t before, std::uint64_t after)
{
    if (before == 0) {
        return 0.0;
    }
    return (static_cast<double>(after) - static_cast<double>(before)) / static_cast<double>(before);
}

[[nodiscard]] double ReductionRatio(std::uint64_t before, std::uint64_t after)
{
    if (before == 0 || after >= before) {
        return 0.0;
    }
    return static_cast<double>(before - after) / static_cast<double>(before);
}

void AppendPercent(std::ostringstream& out, double ratio)
{
    out.setf(std::ios::fixed);
    out.precision(1);
    out << ratio * 100.0 << '%';
}


void CopyGridShape(const VoxelWorld& world, const ChunkGrid& chunks, ChunkMeshBuildMode mode, ChunkMeshCacheInfo& info)
{
    info.mode = mode;
    info.map_width = chunks.info.map_width;
    info.map_height = chunks.info.map_height;
    info.chunk_size_x = chunks.info.chunk_size_x;
    info.chunk_size_y = chunks.info.chunk_size_y;
    info.chunks_x = chunks.info.chunks_x;
    info.chunks_y = chunks.info.chunks_y;
    info.total_chunks = chunks.info.total_chunks;
    info.levels = world.info.levels;
}

void CopyBuildInfo(const ChunkMeshBuildInfo& source, ChunkMeshCacheInfo& target)
{
    target.mode = source.mode;
    target.map_width = source.map_width;
    target.map_height = source.map_height;
    target.chunk_size_x = source.chunk_size_x;
    target.chunk_size_y = source.chunk_size_y;
    target.chunks_x = source.chunks_x;
    target.chunks_y = source.chunks_y;
    target.total_chunks = source.total_chunks;
    target.levels = source.levels;
    target.non_empty_chunks = source.non_empty_chunks;
    target.faces = source.visible_faces;
    target.vertices = source.vertices;
    target.indices = source.indices;
}

void CopyCacheShape(const ChunkMeshCacheInfo& source, ChunkMeshBuildInfo& target)
{
    target.mode = source.mode;
    target.map_width = source.map_width;
    target.map_height = source.map_height;
    target.chunk_size_x = source.chunk_size_x;
    target.chunk_size_y = source.chunk_size_y;
    target.chunks_x = source.chunks_x;
    target.chunks_y = source.chunks_y;
    target.total_chunks = source.total_chunks;
    target.levels = source.levels;
    target.visible_faces = source.faces;
    target.vertices = source.vertices;
    target.indices = source.indices;
    target.non_empty_chunks = source.non_empty_chunks;
}

void RecalculateCacheCounters(ChunkMeshCache& cache)
{
    cache.info.non_empty_chunks = 0;
    cache.info.faces = 0;
    cache.info.vertices = 0;
    cache.info.indices = 0;
    cache.info.dirty_chunks = cache.DirtyCount();

    for (const ChunkMeshData& mesh : cache.chunks) {
        if (!mesh.faces.empty()) {
            ++cache.info.non_empty_chunks;
        }
        cache.info.faces += static_cast<std::uint64_t>(mesh.faces.size());
        cache.info.vertices += static_cast<std::uint64_t>(mesh.vertices.size());
        cache.info.indices += static_cast<std::uint64_t>(mesh.indices.size());
    }
}

void AppendDiagnostics(const Diagnostics& source, Diagnostics& target)
{
    for (const auto& warning : source.warnings) {
        target.AddWarning(warning);
    }
}

}  // namespace

bool ChunkMeshCacheInfo::IsValid() const
{
    return map_width > 0 && map_height > 0 && chunk_size_x > 0 && chunk_size_y > 0 && chunks_x > 0 && chunks_y > 0
        && static_cast<std::uint64_t>(total_chunks) == ExpectedChunkCount(chunks_x, chunks_y) && levels.has_value()
        && vertices == faces * 4ULL && indices == faces * 6ULL && non_empty_chunks <= static_cast<std::uint64_t>(total_chunks)
        && dirty_chunks <= static_cast<std::uint64_t>(total_chunks);
}

bool ChunkMeshRebuildReport::IsValid() const
{
    return valid && attempted && total_chunks >= 0 && rebuilt_chunks <= static_cast<std::uint64_t>(total_chunks)
        && reused_chunks <= static_cast<std::uint64_t>(total_chunks)
        && rebuilt_chunks + reused_chunks == static_cast<std::uint64_t>(total_chunks)
        && old_vertices == old_faces * 4ULL && new_vertices == new_faces * 4ULL
        && old_indices == old_faces * 6ULL && new_indices == new_faces * 6ULL;
}

double ChunkMeshRebuildReport::SavedRebuildWorkRatio() const
{
    return ReductionRatio(static_cast<std::uint64_t>(total_chunks), rebuilt_chunks);
}

double ChunkMeshRebuildReport::FaceDeltaRatio() const
{
    return DeltaRatio(old_faces, new_faces);
}

bool ChunkMeshCache::IsValid() const
{
    return info.IsValid() && chunks.size() == static_cast<std::size_t>(info.total_chunks)
        && dirty.size() == chunks.size()
        && std::all_of(chunks.begin(), chunks.end(), [](const ChunkMeshData& mesh) {
               return mesh.IsValid();
           });
}

std::uint64_t ChunkMeshCache::DirtyCount() const
{
    return static_cast<std::uint64_t>(std::count(dirty.begin(), dirty.end(), static_cast<std::uint8_t>(1)));
}

const ChunkMeshData* ChunkMeshCache::FindChunk(ChunkCoord coord) const
{
    const std::optional<std::size_t> index = ChunkIndex(coord, info.chunks_x, info.chunks_y);
    if (!index.has_value() || *index >= chunks.size()) {
        return nullptr;
    }
    return &chunks[*index];
}

bool ChunkMeshCache::MarkChunkDirty(ChunkCoord coord)
{
    const std::optional<std::size_t> index = ChunkIndex(coord, info.chunks_x, info.chunks_y);
    if (!index.has_value() || *index >= dirty.size()) {
        return false;
    }
    const bool was_clean = dirty[*index] == 0;
    dirty[*index] = 1;
    info.dirty_chunks = DirtyCount();
    return was_clean;
}

std::uint64_t ChunkMeshCache::MarkTileAndBorderChunksDirty(TileCoord tile, const ChunkGrid& chunks)
{
    const std::optional<ChunkCoord> coord = TileChunkCoord(tile, chunks);
    if (!coord.has_value()) {
        return 0;
    }

    std::uint64_t marked = MarkChunkDirty(*coord) ? 1ULL : 0ULL;
    const ChunkInfo* chunk = chunks.FindChunk(*coord);
    if (chunk == nullptr || !chunk->bounds.IsValid()) {
        return marked;
    }

    if (tile.x == chunk->bounds.min_x) {
        marked += MarkChunkDirty(ChunkCoord{coord->x - 1, coord->y}) ? 1ULL : 0ULL;
    }
    if (tile.x == chunk->bounds.max_x - 1) {
        marked += MarkChunkDirty(ChunkCoord{coord->x + 1, coord->y}) ? 1ULL : 0ULL;
    }
    if (tile.y == chunk->bounds.min_y) {
        marked += MarkChunkDirty(ChunkCoord{coord->x, coord->y - 1}) ? 1ULL : 0ULL;
    }
    if (tile.y == chunk->bounds.max_y - 1) {
        marked += MarkChunkDirty(ChunkCoord{coord->x, coord->y + 1}) ? 1ULL : 0ULL;
    }
    info.dirty_chunks = DirtyCount();
    return marked;
}

void ChunkMeshCache::MarkAllDirty()
{
    std::fill(dirty.begin(), dirty.end(), static_cast<std::uint8_t>(1));
    info.dirty_chunks = DirtyCount();
}

void ChunkMeshCache::ClearDirty()
{
    std::fill(dirty.begin(), dirty.end(), static_cast<std::uint8_t>(0));
    info.dirty_chunks = 0;
}

ChunkMeshCache BuildChunkMeshCache(const VoxelWorld& world, const ChunkGrid& chunks, ChunkMeshBuildMode mode)
{
    ChunkMeshCache cache;
    ChunkMeshBuildResult build = BuildChunkMeshes(world, chunks, mode);
    CopyBuildInfo(build.info, cache.info);
    cache.chunks = std::move(build.chunks);
    cache.dirty.assign(cache.chunks.size(), 0);
    cache.diagnostics = std::move(build.diagnostics);
    RecalculateCacheCounters(cache);

    if (!cache.IsValid()) {
        cache.diagnostics.AddWarning("chunk mesh cache validation failed after build");
    }
    return cache;
}


ChunkMeshCache BuildChunkMeshCacheForSelectedChunks(
    const VoxelWorld& world,
    const ChunkGrid& chunks,
    ChunkMeshBuildMode mode,
    const std::vector<std::uint8_t>& selected_chunks)
{
    ChunkMeshCache cache;

    if (!world.IsValid()) {
        cache.diagnostics.AddWarning("cannot build selected chunk mesh cache from invalid voxel world");
        return cache;
    }
    if (!chunks.IsValid()) {
        cache.diagnostics.AddWarning("cannot build selected chunk mesh cache from invalid chunk grid");
        return cache;
    }
    if (selected_chunks.size() != chunks.chunks.size()) {
        cache.diagnostics.AddWarning("cannot build selected chunk mesh cache because selection size differs from chunk grid");
        return cache;
    }

    CopyGridShape(world, chunks, mode, cache.info);
    cache.chunks.reserve(chunks.chunks.size());
    cache.dirty.assign(chunks.chunks.size(), 0);

    for (std::size_t index = 0; index < chunks.chunks.size(); ++index) {
        const ChunkInfo& chunk = chunks.chunks[index];
        if (selected_chunks[index] == 0U) {
            ChunkMeshData empty_mesh;
            empty_mesh.coord = chunk.coord;
            empty_mesh.bounds = chunk.bounds;
            cache.chunks.push_back(std::move(empty_mesh));
            continue;
        }

        ChunkMeshBuildChunkResult chunk_result = BuildChunkMeshForChunk(world, chunk, mode);
        AppendDiagnostics(chunk_result.diagnostics, cache.diagnostics);
        if (!chunk_result.IsValid()) {
            ChunkMeshData empty_mesh;
            empty_mesh.coord = chunk.coord;
            empty_mesh.bounds = chunk.bounds;
            cache.chunks.push_back(std::move(empty_mesh));
            continue;
        }
        cache.chunks.push_back(std::move(chunk_result.mesh));
    }

    RecalculateCacheCounters(cache);
    if (!cache.IsValid()) {
        cache.diagnostics.AddWarning("selected chunk mesh cache validation failed after build");
    }
    return cache;
}

ChunkMeshRebuildReport RebuildDirtyChunkMeshes(const VoxelWorld& world, const ChunkGrid& chunks, ChunkMeshCache* cache)
{
    ChunkMeshRebuildReport report;
    report.attempted = true;

    if (cache == nullptr) {
        report.diagnostics.AddWarning("cannot rebuild dirty chunk meshes using null cache");
        return report;
    }

    report.mode = cache->info.mode;
    report.total_chunks = cache->info.total_chunks;
    report.dirty_chunks = cache->DirtyCount();
    report.old_faces = cache->info.faces;
    report.old_vertices = cache->info.vertices;
    report.old_indices = cache->info.indices;

    if (!world.IsValid()) {
        report.diagnostics.AddWarning("cannot rebuild dirty chunk meshes from invalid voxel world");
        return report;
    }
    if (!chunks.IsValid()) {
        report.diagnostics.AddWarning("cannot rebuild dirty chunk meshes from invalid chunk grid");
        return report;
    }
    if (!cache->IsValid()) {
        report.diagnostics.AddWarning("cannot rebuild dirty chunk meshes from invalid cache");
        return report;
    }
    if (cache->info.total_chunks != chunks.info.total_chunks || cache->info.chunks_x != chunks.info.chunks_x
        || cache->info.chunks_y != chunks.info.chunks_y) {
        report.diagnostics.AddWarning("cannot rebuild dirty chunk meshes because cache and chunk grid dimensions differ");
        return report;
    }

    for (std::size_t index = 0; index < cache->dirty.size(); ++index) {
        if (cache->dirty[index] == 0) {
            continue;
        }
        if (index >= chunks.chunks.size()) {
            report.diagnostics.AddWarning("dirty chunk index is outside chunk grid");
            continue;
        }

        ChunkMeshBuildChunkResult chunk_result = BuildChunkMeshForChunk(world, chunks.chunks[index], cache->info.mode);
        AppendDiagnostics(chunk_result.diagnostics, report.diagnostics);
        if (!chunk_result.IsValid()) {
            continue;
        }

        cache->chunks[index] = std::move(chunk_result.mesh);
        cache->dirty[index] = 0;
        ++report.rebuilt_chunks;
    }

    RecalculateCacheCounters(*cache);
    report.reused_chunks = static_cast<std::uint64_t>(cache->info.total_chunks) - report.rebuilt_chunks;
    report.new_faces = cache->info.faces;
    report.new_vertices = cache->info.vertices;
    report.new_indices = cache->info.indices;
    report.valid = cache->IsValid();
    AppendDiagnostics(cache->diagnostics, report.diagnostics);
    return report;
}

ChunkMeshBuildResult ToChunkMeshBuildResult(const ChunkMeshCache& cache)
{
    ChunkMeshBuildResult result;
    CopyCacheShape(cache.info, result.info);
    result.chunks = cache.chunks;
    result.diagnostics = cache.diagnostics;
    if (!result.IsValid()) {
        result.diagnostics.AddWarning("chunk mesh build result validation failed after cache conversion");
    }
    return result;
}

std::string ToLogString(const ChunkMeshCache& cache)
{
    std::ostringstream out;
    out << "status=" << (cache.IsValid() ? "loaded" : "invalid");
    out << " mode=" << ToString(cache.info.mode);
    if (cache.info.map_width > 0 && cache.info.map_height > 0) {
        out << " map=" << cache.info.map_width << 'x' << cache.info.map_height;
    }
    if (cache.info.chunk_size_x > 0 && cache.info.chunk_size_y > 0) {
        out << " chunk_size=" << cache.info.chunk_size_x << 'x' << cache.info.chunk_size_y;
    }
    if (cache.info.chunks_x > 0 && cache.info.chunks_y > 0) {
        out << " chunks=" << cache.info.chunks_x << 'x' << cache.info.chunks_y << " total=" << cache.info.total_chunks;
    }
    out << " mesh_chunks=" << cache.chunks.size();
    out << " non_empty=" << cache.info.non_empty_chunks;
    out << " faces=" << cache.info.faces;
    out << " vertices=" << cache.info.vertices;
    out << " indices=" << cache.info.indices;
    out << " dirty=" << cache.info.dirty_chunks;
    if (!cache.diagnostics.warnings.empty()) {
        out << " warnings=" << cache.diagnostics.warnings.size();
    }
    return out.str();
}

std::string ToLogString(const ChunkMeshRebuildReport& report)
{
    std::ostringstream out;
    out << "status=" << (report.IsValid() ? "rebuilt" : "invalid");
    out << " mode=" << ToString(report.mode);
    out << " dirty=" << report.dirty_chunks;
    out << " rebuilt=" << report.rebuilt_chunks;
    out << " reused=" << report.reused_chunks;
    out << " total=" << report.total_chunks;
    out << " saved_work=";
    AppendPercent(out, report.SavedRebuildWorkRatio());
    out << " faces=" << report.old_faces << "->" << report.new_faces;
    out << " face_delta=";
    AppendPercent(out, report.FaceDeltaRatio());
    out << " vertices=" << report.old_vertices << "->" << report.new_vertices;
    out << " indices=" << report.old_indices << "->" << report.new_indices;
    if (!report.diagnostics.warnings.empty()) {
        out << " warnings=" << report.diagnostics.warnings.size();
    }
    return out.str();
}

}  // namespace vox3d
