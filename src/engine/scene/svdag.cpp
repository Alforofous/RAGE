#include "svdag.hpp"

#include <algorithm>
#include <cstring>
#include <unordered_map>

namespace RAGE {
    namespace {
        size_t hashNode(const SvdagNode &n) {
            constexpr size_t kFnvSeed = 14695981039346656037ull;
            constexpr size_t kFnvPrime = 1099511628211ull;
            size_t h = kFnvSeed;
            for (uint32_t c : n.children) {
                h ^= static_cast<size_t>(c);
                h *= kFnvPrime;
            }
            return h;
        }

        bool nodesEqual(const SvdagNode &a, const SvdagNode &b) {
            return std::memcmp(a.children, b.children, sizeof(a.children)) == 0;
        }

        bool nodeIsEmpty(const SvdagNode &n) {
            for (uint32_t c : n.children) {
                if (c != 0u) {
                    return false;
                }
            }
            return true;
        }

        int32_t nextPow2(int32_t v) {
            int32_t p = 1;
            while (p < v) {
                p <<= 1;
            }
            return p;
        }

        size_t cubeIndex(int32_t x, int32_t y, int32_t z, int32_t dim) {
            return (static_cast<size_t>(z) * static_cast<size_t>(dim) * static_cast<size_t>(dim))
                   + (static_cast<size_t>(y) * static_cast<size_t>(dim)) + static_cast<size_t>(x);
        }

        /** Acquire a deduplicated index for `node`. Empty nodes return `kEmptySvdagNode`. */
        uint32_t acquireNode(const SvdagNode &node, std::vector<SvdagNode> &nodes,
                              std::unordered_multimap<size_t, uint32_t> &dedupTable) {
            if (nodeIsEmpty(node)) {
                return kEmptySvdagNode;
            }
            const size_t h = hashNode(node);
            const auto range = dedupTable.equal_range(h);
            for (auto it = range.first; it != range.second; ++it) {
                if (nodesEqual(nodes[it->second], node)) {
                    return it->second;
                }
            }
            const uint32_t idx = static_cast<uint32_t>(nodes.size());
            nodes.push_back(node);
            dedupTable.insert({ h, idx });
            return idx;
        }
    }

    Svdag buildSvdag(std::span<const BrickHandle> gridHandles, IVec3 gridDims) {
        Svdag svdag{};
        if (gridDims.x <= 0 || gridDims.y <= 0 || gridDims.z <= 0 || gridHandles.empty()) {
            return svdag;
        }

        // Pad to the smallest power-of-two cube that encloses the input grid.
        const int32_t paddedDim = nextPow2(std::max({ gridDims.x, gridDims.y, gridDims.z }));
        const int32_t levels = [&]() -> int32_t {
            int32_t n = 0;
            int32_t d = paddedDim;
            while (d > 1) {
                d >>= 1;
                ++n;
            }
            return n;
        }();
        svdag.paddedDim = paddedDim;
        svdag.levels = levels;

        // Reserve index 0 as the empty-subtree sentinel.
        svdag.nodes.emplace_back();   // all-zero children

        std::unordered_multimap<size_t, uint32_t> dedupTable;

        // Level 0 (leaf-parent): children are brick handles. Each node covers 2×2×2 bricks.
        const int32_t lvl0Dim = paddedDim / 2;
        std::vector<uint32_t> currentLevel(static_cast<size_t>(lvl0Dim) * lvl0Dim * lvl0Dim,
                                            kEmptySvdagNode);
        for (int32_t z = 0; z < lvl0Dim; ++z) {
            for (int32_t y = 0; y < lvl0Dim; ++y) {
                for (int32_t x = 0; x < lvl0Dim; ++x) {
                    SvdagNode node{};
                    for (int32_t dz = 0; dz < 2; ++dz) {
                        for (int32_t dy = 0; dy < 2; ++dy) {
                            for (int32_t dx = 0; dx < 2; ++dx) {
                                const int32_t sx = (x * 2) + dx;
                                const int32_t sy = (y * 2) + dy;
                                const int32_t sz = (z * 2) + dz;
                                BrickHandle handle = kEmptyBrick;
                                if (sx < gridDims.x && sy < gridDims.y && sz < gridDims.z) {
                                    handle = gridHandles[(static_cast<size_t>(sz) * gridDims.x * gridDims.y)
                                                          + (static_cast<size_t>(sy) * gridDims.x)
                                                          + static_cast<size_t>(sx)];
                                }
                                node.children[(dz * 4) + (dy * 2) + dx] = handle.id;
                            }
                        }
                    }
                    currentLevel[cubeIndex(x, y, z, lvl0Dim)] =
                        acquireNode(node, svdag.nodes, dedupTable);
                }
            }
        }

        // Inner levels: children are node indices from the level below.
        int32_t currentDim = lvl0Dim;
        while (currentDim > 1) {
            const int32_t parentDim = currentDim / 2;
            std::vector<uint32_t> parentLevel(static_cast<size_t>(parentDim) * parentDim * parentDim,
                                               kEmptySvdagNode);
            for (int32_t z = 0; z < parentDim; ++z) {
                for (int32_t y = 0; y < parentDim; ++y) {
                    for (int32_t x = 0; x < parentDim; ++x) {
                        SvdagNode node{};
                        for (int32_t dz = 0; dz < 2; ++dz) {
                            for (int32_t dy = 0; dy < 2; ++dy) {
                                for (int32_t dx = 0; dx < 2; ++dx) {
                                    node.children[(dz * 4) + (dy * 2) + dx] = currentLevel[cubeIndex(
                                        (x * 2) + dx, (y * 2) + dy, (z * 2) + dz, currentDim)];
                                }
                            }
                        }
                        parentLevel[cubeIndex(x, y, z, parentDim)] =
                            acquireNode(node, svdag.nodes, dedupTable);
                    }
                }
            }
            currentLevel = std::move(parentLevel);
            currentDim = parentDim;
        }

        svdag.rootIndex = currentLevel.empty() ? kEmptySvdagNode : currentLevel[0];
        return svdag;
    }
}
