#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include "engine/scene/brick.hpp"
#include "math/ivec.hpp"

namespace RAGE {
    /**
     * One node of a Sparse Voxel DAG.
     *
     * Octree node with eight children. Each child is an opaque `uint32_t`:
     *   - At the deepest level (level 0), children are `BrickHandle`s into the brick pool —
     *     `kEmptyBrick` (= 0) means "this octant is empty".
     *   - At every higher level, children are indices into the node array — index 0 is
     *     reserved as the "empty sub-tree" sentinel, and 1..N point at concrete nodes.
     *
     * The caller knows which level a given node lives at from the SVDAG's `levels` field
     * and the descent depth; we don't tag nodes individually so the layout stays a flat
     * 32-byte POD.
     *
     * Sized at 8 × 4 = 32 bytes. Easily uploaded to a GPU SSBO as a contiguous array.
     */
    struct SvdagNode {
        uint32_t children[8]{};
    };
    static_assert(sizeof(SvdagNode) == 32, "SvdagNode must be exactly 32 bytes");

    /** Reserved "empty subtree" sentinel for inner-node child slots. */
    inline constexpr uint32_t kEmptySvdagNode = 0u;

    // Load-bearing coincidence: the empty-brick sentinel at leaf level and the
    // empty-subtree sentinel at inner levels share the literal value `0u`, which is
    // how cross-level dedup collapses both into a single physical node (index 0). If
    // this ever diverges, the builder's `nodeIsEmpty` check and the shader's
    // `current == 0u` early-exit both silently misclassify entire subtrees.
    static_assert(kEmptyBrick.id == kEmptySvdagNode,
                  "SVDAG cross-level dedup requires the empty-brick and empty-subtree sentinels to share a value");

    /**
     * Build output: the deduplicated octree-as-DAG over a brick grid.
     *
     * `nodes` is the flat array of unique nodes (with index 0 reserved as the "empty
     * subtree" sentinel). `rootIndex` points to the top node — descend from here in the
     * shader. `levels` is the depth of the tree (= log2 of the padded cube dimension):
     * each step downward halves the spatial extent each node covers.
     */
    struct Svdag {
        std::vector<SvdagNode> nodes;
        uint32_t rootIndex = 0u;
        int32_t levels = 0;
        int32_t paddedDim = 0;       // power-of-two cube edge (in bricks) the tree covers
    };

    /**
     * Build an SVDAG from a flat brick-handle grid.
     *
     * `gridHandles` is a `gridDims.x * gridDims.y * gridDims.z` array (z-major, y-mid,
     * x-fastest — matches the world volume's de-wrapped handle order). The grid is padded with
     * `kEmptyBrick` to the smallest power-of-two cube enclosing it before octree
     * construction; the padded regions dedup down to a single "empty subtree" node at
     * every level.
     *
     * Returns an empty SVDAG (no nodes) when the input grid has zero volume.
     *
     * Content-hash deduplication runs at every level — at the leaf level on 8-tuples of
     * brick handles, and at every inner level on 8-tuples of child node indices. Empty
     * subtrees are folded under the `kEmptySvdagNode` sentinel.
     */
    Svdag buildSvdag(std::span<const BrickHandle> gridHandles, IVec3 gridDims);

    /** Total bytes the SVDAG would occupy in a GPU buffer (excluding `rootIndex` / metadata). */
    inline size_t svdagBytes(const Svdag &svdag) { return svdag.nodes.size() * sizeof(SvdagNode); }
}
