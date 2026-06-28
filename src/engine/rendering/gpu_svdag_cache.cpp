#include "gpu_svdag_cache.hpp"

#include <cstring>
#include "engine/rendering/svdag_params.hpp"
#include "gpu/gpu_types.hpp"
#include "gpu/vulkan/vulkan_allocator.hpp"

namespace RAGE {
    namespace {
        uint64_t hashSource(std::span<const BrickHandle> handles, IVec3 dims) {
            uint64_t h = 1469598103934665603ull;
            const auto mix = [&h](const void *bytes, size_t n) {
                const auto *p = static_cast<const uint8_t *>(bytes);
                for (size_t i = 0; i < n; ++i) {
                    h ^= static_cast<uint64_t>(p[i]);
                    h *= 1099511628211ull;
                }
            };
            mix(&dims, sizeof(dims));
            if (!handles.empty()) {
                mix(handles.data(), handles.size() * sizeof(BrickHandle));
            }
            return h;
        }
    }

    GpuSvdagCache::GpuSvdagCache(VulkanAllocator &allocator)
        : allocator_(allocator) {
        const uint64_t nodeBytes = static_cast<uint64_t>(kMaxNodes_) * sizeof(SvdagNode);
        nodesBuffer_.emplace(allocator_.createBuffer({
            .size = nodeBytes,
            .usage = BufferUsage::Storage,
            .memory = MemoryLocation::CpuToGpu,
        }));
        std::memset(nodesBuffer_->mappedData(), 0, nodeBytes);
        paramsBuffer_.emplace(allocator_.createBuffer({
            .size = sizeof(SvdagParamsUbo),
            .usage = BufferUsage::Uniform,
            .memory = MemoryLocation::CpuToGpu,
        }));
        std::memset(paramsBuffer_->mappedData(), 0, sizeof(SvdagParamsUbo));
    }

    GpuSvdagCache::UpdateResult GpuSvdagCache::update(std::span<const BrickHandle> handles, IVec3 dims,
                                                       Vec3 originWorld, float brickWorldSize) {
        UpdateResult result;

        const uint64_t sourceHash = hashSource(handles, dims);
        const bool dirty = (sourceHash != lastSourceHash_) || svdag_.nodes.empty();
        if (dirty) {
            result.rebuilt = true;
            if (handles.empty()) {
                svdag_ = Svdag{};
            } else {
                svdag_ = buildSvdag(handles.data(), dims);
            }
            lastSourceHash_ = sourceHash;
            if (svdag_.nodes.size() > kMaxNodes_) {
                result.capacityExceeded = true;
            } else if (!svdag_.nodes.empty()) {
                std::memcpy(nodesBuffer_->mappedData(), svdag_.nodes.data(),
                            svdag_.nodes.size() * sizeof(SvdagNode));
            }
        }
        result.nodeCount = svdag_.nodes.size();

        const SvdagParamsUbo params{
            .originWorld_brickSize = Vec4(originWorld.x, originWorld.y, originWorld.z, brickWorldSize),
            .rootIndex = static_cast<int32_t>(svdag_.rootIndex),
            .levels = svdag_.levels,
            .paddedDim = svdag_.paddedDim,
            ._pad = 0,
        };
        std::memcpy(paramsBuffer_->mappedData(), &params, sizeof(params));

        return result;
    }
}
