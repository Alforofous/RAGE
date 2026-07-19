#include "voxel3d.hpp"

#include <stdexcept>
#include "engine/scene/brick_pool.hpp"

namespace RAGE {
    Voxel3D::Voxel3D(IVec3 dims, float voxelSize)
        : dims_(dims)
        , voxelSize_(voxelSize) {
        if (dims.x <= 0 || dims.y <= 0 || dims.z <= 0) {
            throw std::runtime_error("Voxel3D: dimensions must be positive");
        }
        if (voxelSize <= 0.0f) {
            throw std::runtime_error("Voxel3D: voxelSize must be positive");
        }
        data_ = std::make_unique<VoxelData>(dims);
        setTransformBumpsTreeVersion(false);
    }

    Voxel3D::Voxel3D(BrickPool &pool, IVec3 dims, float voxelSize)
        : dims_(dims)
        , voxelSize_(voxelSize) {
        if (dims.x <= 0 || dims.y <= 0 || dims.z <= 0) {
            throw std::runtime_error("Voxel3D: dimensions must be positive");
        }
        if (voxelSize <= 0.0f) {
            throw std::runtime_error("Voxel3D: voxelSize must be positive");
        }
        data_ = std::make_unique<VoxelData>(pool, dims);
        // Voxel3D transforms never dirty the scene tree: dense volumes re-upload
        // their transform every frame anyway, and content changes are tracked by
        // VoxelData::version(). Structural scene changes still bump normally.
        setTransformBumpsTreeVersion(false);
    }

    void Voxel3D::setVoxel(IVec3 c, Color rgba) { data_->setVoxel(c, rgba.toRGBA8()); }

    Color Voxel3D::voxel(IVec3 c) const { return Color::fromRGBA8(data_->voxel(c)); }

    void Voxel3D::fill(const std::function<Color(IVec3)> &fn) {
        for (int32_t z = 0; z < dims_.z; ++z) {
            for (int32_t y = 0; y < dims_.y; ++y) {
                for (int32_t x = 0; x < dims_.x; ++x) {
                    data_->setVoxel({ x, y, z }, fn({ x, y, z }).toRGBA8());
                }
            }
        }
    }

    void Voxel3D::fillSolid(Color rgba) {
        const uint32_t packed = rgba.toRGBA8();
        for (int32_t z = 0; z < dims_.z; ++z) {
            for (int32_t y = 0; y < dims_.y; ++y) {
                for (int32_t x = 0; x < dims_.x; ++x) {
                    data_->setVoxel({ x, y, z }, packed);
                }
            }
        }
    }

    void Voxel3D::clear() { fillSolid(Color::transparent()); }

    void Voxel3D::fillFromPackedRGBA8(const uint32_t *src, IVec3 srcDims) {
        if (loadProgress_) {
            loadProgress_(0.0f);
        }
        data_->fillFromPackedRGBA8(src, srcDims, loadProgress_, loadShouldCancel_);
        if (loadProgress_) {
            loadProgress_(1.0f);
        }
    }

    void Voxel3D::prepareFrame(VulkanDescriptorWriter & /*writer*/, VkDescriptorSet /*set*/,
                                PushConstantBuilder & /*pc*/, const FrameContext & /*frame*/) {
        // Brickmap storage: no per-frame state to push. The renderer reads brick handles
        // from VoxelData and uploads dirty bricks via BrickPool's tracking.
    }
}
