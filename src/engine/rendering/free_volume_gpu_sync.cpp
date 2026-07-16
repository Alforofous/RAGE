#include "free_volume_gpu_sync.hpp"

#include <cstring>
#include <stdexcept>
#include <string>
#include "engine/scene/brick.hpp"
#include "engine/scene/voxel3d.hpp"
#include "engine/scene/voxel_data.hpp"
#include "math/mat.hpp"

namespace RAGE {
    namespace {
        /// std430 mirror of the shader's VolumeDesc (binding 12).
        struct GpuVolumeDesc {
            float invWorld[16];
            float world[16];
            int32_t brickDimsX;
            int32_t brickDimsY;
            int32_t brickDimsZ;
            int32_t handleOffset;
            float voxelSize;
            float _pad0;
            float _pad1;
            float _pad2;
        };
        static_assert(sizeof(GpuVolumeDesc) == 160, "must match shader std430 layout");
    }

    FreeVolumeGpuSync::FreeVolumeGpuSync(VulkanAllocator &allocator, uint32_t maxVolumes,
                                         uint32_t maxHandleCells)
        : maxVolumes_(maxVolumes)
        , maxHandleCells_(maxHandleCells)
        , descsBuffer_(allocator.createBuffer({
              .size = static_cast<uint64_t>(maxVolumes) * sizeof(GpuVolumeDesc),
              .usage = BufferUsage::Storage,
              .memory = MemoryLocation::CpuToGpu,
          }))
        , handlesBuffer_(allocator.createBuffer({
              .size = static_cast<uint64_t>(maxHandleCells) * sizeof(BrickHandle),
              .usage = BufferUsage::Storage,
              .memory = MemoryLocation::CpuToGpu,
          })) {
        std::memset(handlesBuffer_.mappedData(), 0,
                    static_cast<size_t>(maxHandleCells) * sizeof(BrickHandle));
    }

    uint32_t FreeVolumeGpuSync::upload(std::span<Voxel3D *const> volumes) {
        if (volumes.empty()) {
            return 0;
        }
        if (volumes.size() > maxVolumes_) {
            throw std::runtime_error("FreeVolumeGpuSync: " + std::to_string(volumes.size())
                                     + " free-standing volumes exceed budget "
                                     + std::to_string(maxVolumes_));
        }

        auto *descDst = static_cast<GpuVolumeDesc *>(descsBuffer_.mappedData());
        auto *handleDst = static_cast<BrickHandle *>(handlesBuffer_.mappedData());
        uint32_t handleCursor = 0;
        for (size_t i = 0; i < volumes.size(); ++i) {
            const Voxel3D &v = *volumes[i];
            const VoxelData &data = *v.voxelData();
            const auto handles = data.handles();
            if (handleCursor + handles.size() > maxHandleCells_) {
                throw std::runtime_error("FreeVolumeGpuSync: free-volume handle cells exceed budget "
                                         + std::to_string(maxHandleCells_));
            }

            GpuVolumeDesc desc{};
            const Mat4 world = v.worldMatrix();
            const Mat4 invWorld = world.inverted();
            std::memcpy(desc.invWorld, invWorld.data(), sizeof(desc.invWorld));
            std::memcpy(desc.world, world.data(), sizeof(desc.world));
            const IVec3 bd = data.brickDims();
            desc.brickDimsX = bd.x;
            desc.brickDimsY = bd.y;
            desc.brickDimsZ = bd.z;
            desc.handleOffset = static_cast<int32_t>(handleCursor);
            desc.voxelSize = v.voxelSize();
            descDst[i] = desc;

            std::memcpy(handleDst + handleCursor, handles.data(),
                        handles.size() * sizeof(BrickHandle));
            handleCursor += static_cast<uint32_t>(handles.size());
        }
        return static_cast<uint32_t>(volumes.size());
    }
}
