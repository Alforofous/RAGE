#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include "math/ivec.hpp"
#include "math/vec.hpp"

namespace RAGE {
    /**
     * GPU-side world-grid parameters bound at set 0 binding 7 (UBO). Mirrors GLSL std140 —
     * two ivec4-aligned slots so the shader can read it as `vec4 origin_cellSize; ivec4
     * dims_pad;`. `dims_pad.x == 0` signals "no world grid this frame; trace casters
     * linearly" so the shader stays correct when the scene has no casters.
     */
    struct WorldGridParams {
        Vec4 origin_cellSize{ 0.0f, 0.0f, 0.0f, 1.0f };
        int32_t dimsX = 0;
        int32_t dimsY = 0;
        int32_t dimsZ = 0;
        int32_t _pad = 0;
    };
    static_assert(sizeof(WorldGridParams) == 32, "WorldGridParams layout must match shader expectation");


    /**
     * One caster's world-space axis-aligned bounding box, in caster-index order.
     *
     * The world grid is built from these alone — no Voxel3D / engine dependency leaks into
     * the builder, so the builder stays pure and unit-testable.
     */
    struct CasterAabb {
        Vec3 worldMin{ 0.0f, 0.0f, 0.0f };
        Vec3 worldMax{ 0.0f, 0.0f, 0.0f };
    };

    /**
     * Geometry of the world grid — the coarse uniform 3D bitmap the outer DDA marches
     * through. One bit per cell: set if *any* caster's world AABB intersects that cell.
     *
     * The grid is sized to enclose every caster's AABB plus one cell of padding on each
     * face. `cellSize` is chosen by the caller (renderer); larger cells = cheaper outer
     * DDA per ray but coarser empty-space classification.
     *
     * Bit layout matches the M1 occupancy mip exactly: cells indexed in (x, y, z) row-major
     * order, packed 8 bits per byte LSB-first. Re-uses `mipBitIndex` / `mipReadBit` math at
     * the shader and CPU side.
     */
    struct WorldGridLayout {
        Vec3 origin{ 0.0f, 0.0f, 0.0f };
        float cellSize = 1.0f;
        IVec3 dims{ 0, 0, 0 };
        size_t byteSize = 0;
    };

    /**
     * Compute the layout that encloses every caster's AABB with `paddingCells` cells of
     * margin on each face. Returns a layout with `byteSize = 0` and `dims = (0,0,0)` if
     * `aabbs` is empty.
     *
     * Layout is deterministic given the same inputs — no allocation, safe to call every
     * frame (which the renderer does).
     */
    WorldGridLayout computeWorldGridLayout(std::span<const CasterAabb> aabbs, float cellSize,
                                            int paddingCells = 1);

    /**
     * Populate the world grid bitmap. The caller owns the output buffer; it must be at
     * least `layout.byteSize` bytes and is overwritten completely (no OR-ing).
     *
     * One bit is set per (caster, cell) pair where the caster's AABB intersects the cell's
     * world-space bounds. Empty input clears the buffer.
     */
    void buildWorldGrid(std::span<const CasterAabb> aabbs, const WorldGridLayout &layout,
                         uint8_t *outBits);
}
