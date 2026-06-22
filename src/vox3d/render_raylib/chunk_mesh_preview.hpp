#pragma once

#include "vox3d/mesh/mesh_data.hpp"

#include <raylib.h>

#include <cstdint>
#include <string>
#include <vector>

namespace vox3d {

/**
 * @brief Upload statistics for the raylib chunk-mesh preview renderer.
 */
struct RaylibChunkMeshPreviewStats {
    bool uploaded = false;
    std::uint64_t models = 0;
    std::uint64_t faces = 0;
    std::uint64_t vertices = 0;
    std::uint64_t indices = 0;
    std::uint64_t skipped_chunks = 0;

    /**
     * @brief Returns true when at least one renderer model is ready to draw.
     *
     * @return True if GPU resources were uploaded successfully.
     */
    [[nodiscard]] bool IsValid() const;
};

/**
 * @brief Raylib-backed debug preview renderer for generated chunk meshes.
 *
 * The renderer owns raylib Model resources created from renderer-independent
 * ChunkMeshData. Upload and unload must happen while a raylib window/context is
 * alive. The class does not own source map, voxel, or chunk data.
 */
class RaylibChunkMeshPreview {
public:
    /**
     * @brief Releases any uploaded preview resources.
     */
    ~RaylibChunkMeshPreview();

    RaylibChunkMeshPreview() = default;
    RaylibChunkMeshPreview(const RaylibChunkMeshPreview&) = delete;
    RaylibChunkMeshPreview& operator=(const RaylibChunkMeshPreview&) = delete;
    RaylibChunkMeshPreview(RaylibChunkMeshPreview&&) = delete;
    RaylibChunkMeshPreview& operator=(RaylibChunkMeshPreview&&) = delete;

    /**
     * @brief Uploads chunk mesh data into raylib Model resources.
     *
     * Existing preview resources are unloaded before the new upload attempt.
     * Chunks that exceed raylib's 16-bit mesh index limit are skipped and
     * reported in the resulting stats.
     *
     * @param build_result Renderer-independent chunk mesh data.
     * @return True if at least one non-empty chunk model was uploaded.
     */
    [[nodiscard]] bool Upload(const ChunkMeshBuildResult& build_result);

    /**
     * @brief Draws uploaded chunk mesh models inside the viewport rectangle.
     *
     * @param viewport Screen-space viewport rectangle.
     * @param build_result Original mesh build summary used for map dimensions.
     * @param camera Camera used for the 3D preview draw pass.
     */
    void Draw(Rectangle viewport, const ChunkMeshBuildResult& build_result, const Camera3D& camera) const;

    /**
     * @brief Releases uploaded raylib Model resources.
     */
    void Unload();

    /**
     * @brief Returns true when preview resources are available.
     *
     * @return True if one or more models are uploaded.
     */
    [[nodiscard]] bool IsUploaded() const;

    /**
     * @brief Returns current upload statistics.
     *
     * @return Upload statistics for diagnostics and UI display.
     */
    [[nodiscard]] const RaylibChunkMeshPreviewStats& Stats() const;

private:
    std::vector<Model> models_;
    RaylibChunkMeshPreviewStats stats_;
};

/**
 * @brief Builds a compact stable log string for raylib preview diagnostics.
 *
 * @param stats Raylib preview upload statistics.
 * @return Compact human-readable summary.
 */
[[nodiscard]] std::string ToLogString(const RaylibChunkMeshPreviewStats& stats);

}  // namespace vox3d
