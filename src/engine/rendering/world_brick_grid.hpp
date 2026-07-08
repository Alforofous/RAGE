#pragma once

#include <cstddef>
#include <span>
#include <vector>
#include "engine/scene/brick.hpp"
#include "math/ivec.hpp"

namespace RAGE {
    class VoxelData;

    /**
     * Renderer-owned top-level brick grid. Each cell stores either `kEmptyBrick`
     * (empty) or a `BrickHandle` into the shared `BrickPool`. The shader does its
     * outer DDA over the current *window* of valid world cells; non-empty cells are
     * followed into the pool for the inner 8³ DDA.
     *
     * Storage is **toroidal**: allocated once at fixed power-of-two dims (injected by
     * the app), a world cell lives at `world & (dims - 1)` per axis. The window of
     * valid coords slides over the fixed storage as the world streams; a chunk
     * entering on one side reuses the slots a chunk vacated on the other, so nothing
     * ever shifts or reallocates. Callers must clear cells they abandon
     * (`clearChunk` on evict) — the streamer's eviction contract guarantees this.
     *
     * Two write paths:
     *  - `writeChunk` / `clearChunk`: incremental patching driven by streamer
     *    placement events. O(chunk cells).
     *  - `rebuild`: full re-derivation from scene placements — fallback for
     *    non-streamed scenes. Window becomes the placements' AABB (must fit dims).
     */
    struct VoxelDataWorldPlacement {
        const VoxelData *data = nullptr;
        IVec3 worldBrickOrigin{ 0, 0, 0 };   // brick-coord where `data`'s (0,0,0) lands
    };

    class WorldBrickGrid {
    public:
        /// `fixedDims` must be a power of two per axis (wrap is a bitmask); throws otherwise.
        explicit WorldBrickGrid(IVec3 fixedDims);

        /**
         * @brief Set the window of valid world cells (min brick coord + extent in
         *        bricks). Cells outside it read as empty. Extent must fit within the
         *        fixed dims per axis; throws otherwise.
         */
        void setWindow(IVec3 windowMinBrick, IVec3 windowExtent);

        /// Write `data`'s handles into the chunk box at `worldBrickOrigin` (clears the
        /// box first, so holes inside the chunk read as empty).
        void writeChunk(IVec3 worldBrickOrigin, const VoxelData &data);

        /// Zero the chunk box at `worldBrickOrigin` spanning `brickDims` cells.
        void clearChunk(IVec3 worldBrickOrigin, IVec3 brickDims);

        /// Zero the whole storage, write every placement, window = placements' AABB.
        void rebuild(std::span<const VoxelDataWorldPlacement> placements);

        IVec3 fixedDims() const { return fixedDims_; }
        IVec3 windowMinBrick() const { return windowMinBrick_; }
        IVec3 windowExtent() const { return windowExtent_; }

        /// CPU mirror of the shader lookup: window bounds check, then wrapped fetch.
        BrickHandle handleAt(IVec3 worldCell) const;

        /// Full fixed-size storage, in wrapped slot order (uploaded verbatim to GPU).
        std::span<const BrickHandle> handles() const { return handles_; }

    private:
        size_t slotIndex(IVec3 worldCell) const;

        IVec3 fixedDims_{ 0, 0, 0 };
        IVec3 wrapMask_{ 0, 0, 0 };
        IVec3 windowMinBrick_{ 0, 0, 0 };
        IVec3 windowExtent_{ 0, 0, 0 };
        std::vector<BrickHandle> handles_;
    };
}
