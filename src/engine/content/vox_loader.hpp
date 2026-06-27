#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>
#include "math/ivec.hpp"

namespace RAGE::Content {
    /**
     * Parsed MagicaVoxel .vox file as a dense color grid.
     *
     * `voxels` is sized `dims.x * dims.y * dims.z` and stores **packed RGBA8 (one uint32 per
     * cell)** — the same encoding the GPU voxel buffer uses, so `Voxel3D::fillFromPackedRGBA8`
     * can `memcpy` straight in without per-cell conversion. Cells that are empty in the
     * source file are `0u` (alpha = 0). Linear index for grid coordinate (x, y, z) follows
     * the Voxel3D convention: `z * dims.x * dims.y + y * dims.x + x`.
     *
     * Axis convention mirrors the .vox file: +X right, +Y forward, +Z up. RAGE conventionally
     * uses +Y up; rotate the owning Node3D (e.g. −90° around X) when you want the model
     * upright in a Y-up scene.
     */
    struct VoxModel {
        IVec3 dims{};
        std::vector<uint32_t> voxels;
    };

    /**
     * Load a MagicaVoxel .vox file from disk.
     *
     * Reads only the first model's SIZE, XYZI, and optional RGBA chunks. Scene-graph, material,
     * animation, and IMAP chunks are skipped silently. Multi-model files yield only the first
     * model — adequate for static asset import. When no RGBA chunk is present, all voxels
     * default to opaque white (sufficient for grey-box geometry; real models should ship a
     * palette, which MagicaVoxel does by default).
     *
     * @throws std::runtime_error on missing file, bad magic, unsupported version (<150),
     *         malformed chunk layout, missing SIZE chunk, or non-positive dimensions.
     */
    VoxModel loadVox(const std::filesystem::path &path);
}
