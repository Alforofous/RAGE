#include "world_grid_gpu_sync.hpp"

#include <stdexcept>
#include "engine/scene/voxel_data.hpp"

#include <array>
#include <cstring>
#include "engine/scene/brick.hpp"

namespace RAGE {
    namespace {
        uint64_t cellCount(IVec3 dims) {
            return static_cast<uint64_t>(dims.x) * static_cast<uint64_t>(dims.y)
                   * static_cast<uint64_t>(dims.z);
        }
        constexpr uint64_t kParamsBytes = 64;   // 4 × vec4 in std140
    }

    WorldGridGpuSync::WorldGridGpuSync(VulkanAllocator &allocator, VkDevice device, IVec3 gridDims)
        : gridDims_(gridDims)
        , paramsBuffer_(allocator.createBuffer({
              .size = kParamsBytes,
              .usage = BufferUsage::Uniform,
              .memory = MemoryLocation::CpuToGpu,
          }))
        , handlesBuffer_(allocator.createBuffer({
              .size = cellCount(gridDims) * sizeof(BrickHandle),
              .usage = BufferUsage::Storage,
              .memory = MemoryLocation::CpuToGpu,
          }))
        , stagingBuffer_(allocator.createBuffer({
              .size = cellCount(gridDims) * sizeof(BrickHandle),
              .usage = BufferUsage::TransferSrc,
              .memory = MemoryLocation::CpuToGpu,
          }))
        , image_(allocator.createImage({
              .width = static_cast<uint32_t>(gridDims.x),
              .height = static_cast<uint32_t>(gridDims.y),
              .depth = static_cast<uint32_t>(gridDims.z),
              .format = ImageFormat::R32_UINT,
              .usage = ImageUsage::Sampled | ImageUsage::TransferDst,
              .memory = MemoryLocation::GpuOnly,
          }))
        , imageView_(image_.createView({ .viewType = ImageViewType::Type3D }))
        , sampler_(VulkanSampler::createNearestClamp(device)) {
        std::memset(paramsBuffer_.mappedData(), 0, kParamsBytes);
        std::memset(handlesBuffer_.mappedData(), 0, cellCount(gridDims) * sizeof(BrickHandle));
    }

    WorldGridView gridView(const WorldBrickGrid &grid) {
        return WorldGridView{ .windowMinBrick = grid.windowMinBrick(),
                              .windowExtent = grid.windowExtent(),
                              .storageDims = grid.fixedDims(),
                              .handles = grid.handles() };
    }

    WorldGridView gridView(const VoxelData &data) {
        if (!data.isWindowed()) {
            throw std::logic_error("gridView: volume storage is dense, not a world grid");
        }
        return WorldGridView{ .windowMinBrick = data.windowOriginBrick(),
                              .windowExtent = data.brickDims(),
                              .storageDims = data.storageBrickDims(),
                              .handles = data.handles() };
    }

    void WorldGridGpuSync::upload(const WorldGridView &grid, float brickWorldSize,
                                  bool wantTexture) {
        // Params UBO: window origin/extent for the DDA, window min brick + wrap mask
        // for toroidal slot addressing.
        if (grid.storageDims != gridDims_) {
            throw std::invalid_argument(
                "WorldGridGpuSync::upload: view storage dims do not match the injected "
                "grid capacity");
        }
        const IVec3 dims = grid.windowExtent;
        const IVec3 origBrick = grid.windowMinBrick;
        const IVec3 fixedDims = grid.storageDims;
        struct WorldBrickGridParamsLayout {
            float originX;
            float originY;
            float originZ;
            float cellSize;
            int32_t dimsX;
            int32_t dimsY;
            int32_t dimsZ;
            int32_t voxelDim;
            int32_t winMinX;
            int32_t winMinY;
            int32_t winMinZ;
            int32_t _pad0;
            int32_t wrapMaskX;
            int32_t wrapMaskY;
            int32_t wrapMaskZ;
            int32_t _pad1;
        };
        static_assert(sizeof(WorldBrickGridParamsLayout) == kParamsBytes);
        const WorldBrickGridParamsLayout params{
            .originX = static_cast<float>(origBrick.x) * brickWorldSize,
            .originY = static_cast<float>(origBrick.y) * brickWorldSize,
            .originZ = static_cast<float>(origBrick.z) * brickWorldSize,
            .cellSize = brickWorldSize,
            .dimsX = dims.x,
            .dimsY = dims.y,
            .dimsZ = dims.z,
            .voxelDim = Brick::kDim,
            .winMinX = origBrick.x,
            .winMinY = origBrick.y,
            .winMinZ = origBrick.z,
            ._pad0 = 0,
            .wrapMaskX = fixedDims.x - 1,
            .wrapMaskY = fixedDims.y - 1,
            .wrapMaskZ = fixedDims.z - 1,
            ._pad1 = 0,
        };
        std::memcpy(paramsBuffer_.mappedData(), &params, sizeof(params));

        const auto handles = grid.handles;
        std::memcpy(handlesBuffer_.mappedData(), handles.data(),
                    handles.size() * sizeof(BrickHandle));

        if (wantTexture) {
            std::memcpy(stagingBuffer_.mappedData(), handles.data(),
                        handles.size() * sizeof(BrickHandle));
            uploadDims_ = fixedDims;
        } else {
            // Grid content changed without refreshing the texture: it is now stale.
            uploadDims_ = IVec3{ 0, 0, 0 };
            textureValid_ = false;
        }
    }

    bool WorldGridGpuSync::recordTextureUpload(VulkanRecorder<queue_kind::Graphics> &rec) {
        if (uploadDims_.x == 0) {
            if (!imageInitialized_) {
                // The descriptor set always references the texture, so it must sit in
                // ShaderReadOnlyOptimal even on frames that never sample it.
                const std::array<VulkanImageBarrier, 1> init{ { {
                    .image = image_.handle(),
                    .oldLayout = ImageLayout::Undefined,
                    .newLayout = ImageLayout::ShaderReadOnlyOptimal,
                    .srcStage = PipelineStage::TopOfPipe,
                    .dstStage = PipelineStage::ComputeShader,
                    .srcAccess = AccessFlags::None,
                    .dstAccess = AccessFlags::ShaderRead,
                } } };
                rec.pipelineBarrier(init);
                imageInitialized_ = true;
            }
            return false;
        }

        imageInitialized_ = true;
        const std::array<VulkanImageBarrier, 1> toTransfer{ { {
            .image = image_.handle(),
            .oldLayout = ImageLayout::Undefined,
            .newLayout = ImageLayout::TransferDstOptimal,
            .srcStage = PipelineStage::ComputeShader,
            .dstStage = PipelineStage::Transfer,
            .srcAccess = AccessFlags::None,
            .dstAccess = AccessFlags::TransferWrite,
        } } };
        rec.pipelineBarrier(toTransfer);

        const std::array<BufferImageCopy, 1> region{ { {
            .imageWidth = static_cast<uint32_t>(uploadDims_.x),
            .imageHeight = static_cast<uint32_t>(uploadDims_.y),
            .imageDepth = static_cast<uint32_t>(uploadDims_.z),
        } } };
        rec.copyBufferToImage(stagingBuffer_, image_, ImageLayout::TransferDstOptimal, region);

        const std::array<VulkanImageBarrier, 1> toSampled{ { {
            .image = image_.handle(),
            .oldLayout = ImageLayout::TransferDstOptimal,
            .newLayout = ImageLayout::ShaderReadOnlyOptimal,
            .srcStage = PipelineStage::Transfer,
            .dstStage = PipelineStage::ComputeShader,
            .srcAccess = AccessFlags::TransferWrite,
            .dstAccess = AccessFlags::ShaderRead,
        } } };
        rec.pipelineBarrier(toSampled);

        uploadDims_ = IVec3{ 0, 0, 0 };
        textureValid_ = true;
        return true;
    }
}
