#include "voxel3d.hpp"
#include <cstring>
#include <stdexcept>

namespace RAGE {
    Voxel3D::Voxel3D(VulkanAllocator &allocator, IVec3 dims, float voxelSize)
        : allocator_(&allocator)
        , dims_(dims)
        , voxelSize_(voxelSize) {
        if (dims.x <= 0 || dims.y <= 0 || dims.z <= 0) {
            throw std::runtime_error("Voxel3D: dimensions must be positive");
        }
        if (voxelSize <= 0.0f) {
            throw std::runtime_error("Voxel3D: voxelSize must be positive");
        }
        rebuildBuffer();
    }

    void Voxel3D::setVoxel(IVec3 c, Color rgba) {
        writeVoxelCpu(c, rgba);
    }

    Color Voxel3D::voxel(IVec3 c) const {
        if (!inBounds(c)) {
            return Color::transparent();
        }

        return Color::fromRGBA8(mappedVoxels()[coordToIndex(c)]);
    }

    void Voxel3D::fill(const std::function<Color(IVec3)> &fn) {
        uint32_t *data = mappedVoxels();
        for (int32_t z = 0; z < dims_.z; ++z) {
            for (int32_t y = 0; y < dims_.y; ++y) {
                for (int32_t x = 0; x < dims_.x; ++x) {
                    data[coordToIndex({ x, y, z })] = fn({ x, y, z }).toRGBA8();
                }
            }
        }
        dirty_ = true;
    }

    void Voxel3D::fillSolid(Color rgba) {
        const uint32_t packed = rgba.toRGBA8();
        uint32_t *data = mappedVoxels();
        const int64_t n = dims_.volume();
        for (int64_t i = 0; i < n; ++i) {
            data[i] = packed;
        }
        dirty_ = true;
    }

    void Voxel3D::clear() {
        fillSolid(Color::transparent());
    }

    void Voxel3D::resize(IVec3 newDims) {
        if (newDims == dims_) {
            return;
        }
        if (newDims.x <= 0 || newDims.y <= 0 || newDims.z <= 0) {
            throw std::runtime_error("Voxel3D::resize: dimensions must be positive");
        }
        dims_ = newDims;
        rebuildBuffer();
    }

    void Voxel3D::prepareFrame(VulkanDescriptorWriter &writer, VkDescriptorSet set, PushConstantBuilder &pc,
                               const FrameContext &frame) {
        uploadDirty();
        (void)writer;
        (void)set;
        (void)pc;
        (void)frame;
    }

    void Voxel3D::writeVoxelCpu(IVec3 c, Color rgba) {
        if (!inBounds(c)) {
            return;
        }
        mappedVoxels()[coordToIndex(c)] = rgba.toRGBA8();
        dirty_ = true;
    }

    void Voxel3D::uploadDirty() {
        if (!dirty_) {
            return;
        }
        dirty_ = false;
    }

    void Voxel3D::rebuildBuffer() {
        const uint64_t byteSize = static_cast<uint64_t>(dims_.volume()) * sizeof(uint32_t);
        buffer_.emplace(allocator_->createBuffer({
            .size = byteSize,
            .usage = BufferUsage::Storage,
            .memory = MemoryLocation::CpuToGpu,
        }));
        std::memset(buffer_->mappedData(), 0, byteSize);
        dirty_ = true;
    }

    bool Voxel3D::inBounds(IVec3 c) const {
        return c.x >= 0 && c.x < dims_.x && c.y >= 0 && c.y < dims_.y && c.z >= 0 && c.z < dims_.z;
    }

    size_t Voxel3D::coordToIndex(IVec3 c) const {
        return (static_cast<size_t>(c.z) * static_cast<size_t>(dims_.x) * static_cast<size_t>(dims_.y)) +
               (static_cast<size_t>(c.y) * static_cast<size_t>(dims_.x)) + static_cast<size_t>(c.x);
    }

    uint32_t *Voxel3D::mappedVoxels() {
        if (!buffer_.has_value()) {
            throw std::runtime_error("Voxel3D: buffer not initialised");
        }
        return static_cast<uint32_t *>(buffer_->mappedData());
    }

    const uint32_t *Voxel3D::mappedVoxels() const {
        if (!buffer_.has_value()) {
            throw std::runtime_error("Voxel3D: buffer not initialised");
        }
        return static_cast<const uint32_t *>(buffer_->mappedData());
    }
}
