#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include "engine/scene/brick.hpp"
#include "engine/scene/svdag.hpp"
#include "gpu/vulkan/vulkan_buffer.hpp"
#include "math/ivec.hpp"
#include "math/vec.hpp"

namespace RAGE {
    class VulkanAllocator;

    /**
     * GPU-resident SVDAG cache.
     *
     * Owns two Vulkan buffers — an SSBO of `SvdagNode`s and a UBO of `SvdagParamsUbo`.
     * `update(...)` hashes the source brick-handle grid and only rebuilds + uploads
     * when the source has changed since the last call; static scenes pay one hash
     * walk per frame and nothing else.
     *
     * The renderer constructs one of these and binds the exposed buffers each frame.
     * Lifting the build / upload / capacity-fallback concerns out of `Renderer::render`
     * keeps the SVDAG's lifecycle in one place and turns the "node count exceeds
     * buffer" path from a silent log line into an explicit `UpdateResult` field the
     * caller is forced to read.
     */
    class GpuSvdagCache {
    public:
        /**
         * Outcome of a single `update()` call.
         *   - `rebuilt = true`  → source content changed since last call; svdag()
         *                         has been refreshed and buffers re-uploaded.
         *   - `capacityExceeded = true` → the rebuilt SVDAG has more nodes than the
         *                         GPU buffer can hold; the buffer is left stale and
         *                         the caller should fall back to the flat-grid path
         *                         for this frame.
         *   - `nodeCount` → svdag().nodes.size() (cheap accessor).
         */
        struct UpdateResult {
            bool rebuilt = false;
            bool capacityExceeded = false;
            size_t nodeCount = 0;
        };

        explicit GpuSvdagCache(VulkanAllocator &allocator);

        /**
         * Rebuild the SVDAG from `(handles, dims)` if the content hash differs from the
         * last call, then upload the params (always — they describe spatial layout and
         * may move even on cache hit). Re-uploads the node array only on rebuild.
         */
        UpdateResult update(std::span<const BrickHandle> handles, IVec3 dims,
                            Vec3 originWorld, float brickWorldSize);

        const Svdag &svdag() const { return svdag_; }

        /** SVDAG node SSBO. Null until first `update()`. */
        const VulkanBuffer *nodesBuffer() const {
            return nodesBuffer_.has_value() ? &(*nodesBuffer_) : nullptr;
        }
        VulkanBuffer *nodesBuffer() {
            return nodesBuffer_.has_value() ? &(*nodesBuffer_) : nullptr;
        }

        /** SVDAG params UBO. Null until first `update()`. */
        const VulkanBuffer *paramsBuffer() const {
            return paramsBuffer_.has_value() ? &(*paramsBuffer_) : nullptr;
        }
        VulkanBuffer *paramsBuffer() {
            return paramsBuffer_.has_value() ? &(*paramsBuffer_) : nullptr;
        }

    private:
        static constexpr uint64_t kMaxNodes_ = 64u * 1024u;

        VulkanAllocator &allocator_;
        std::optional<VulkanBuffer> nodesBuffer_;
        std::optional<VulkanBuffer> paramsBuffer_;
        Svdag svdag_{};
        uint64_t lastSourceHash_ = 0;
    };
}
