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
     * @brief GPU-resident SVDAG cache: an SSBO of nodes + a UBO of params, rebuilt and
     *        uploaded only when the source brick-handle grid's content hash changes.
     */
    class GpuSvdagCache {
    public:
        struct UpdateResult {
            bool rebuilt = false;
            bool capacityExceeded = false;     ///< Rebuilt SVDAG didn't fit; buffer is stale, fall back to flat grid.
            size_t nodeCount = 0;
        };

        explicit GpuSvdagCache(VulkanAllocator &allocator);

        UpdateResult update(std::span<const BrickHandle> handles, IVec3 dims,
                            Vec3 originWorld, float brickWorldSize);

        /**
         * @brief Adopt a DAG built elsewhere (e.g. on a worker thread via `buildSvdag`)
         *        and upload it — the upload half of `update` without the CPU build.
         */
        UpdateResult uploadBuilt(Svdag &&svdag, Vec3 originWorld, float brickWorldSize);

        const Svdag &svdag() const { return svdag_; }

        const VulkanBuffer *nodesBuffer() const {
            return nodesBuffer_.has_value() ? &(*nodesBuffer_) : nullptr;
        }
        VulkanBuffer *nodesBuffer() {
            return nodesBuffer_.has_value() ? &(*nodesBuffer_) : nullptr;
        }

        const VulkanBuffer *paramsBuffer() const {
            return paramsBuffer_.has_value() ? &(*paramsBuffer_) : nullptr;
        }
        VulkanBuffer *paramsBuffer() {
            return paramsBuffer_.has_value() ? &(*paramsBuffer_) : nullptr;
        }

    private:
        void writeParams_(Vec3 originWorld, float brickWorldSize);

        static constexpr uint64_t kMaxNodes_ = 64u * 1024u;

        VulkanAllocator &allocator_;
        std::optional<VulkanBuffer> nodesBuffer_;
        std::optional<VulkanBuffer> paramsBuffer_;
        Svdag svdag_{};
        uint64_t lastSourceHash_ = 0;
    };
}
