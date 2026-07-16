#pragma once

#include <memory>
#include <vector>

namespace RAGE {
    class BrickPool;
    class Voxel3D;
}

namespace RAGE::Toolkit::Content {
    /**
     * @brief Produces `Voxel3D` instances on demand. Concrete generators (procedural
     *        terrain, prefab arrays, file-loaded scenes) implement `generate`. The
     *        engine doesn't know what scene shapes exist — main.cpp picks a generator
     *        and adds the returned nodes to its scene graph.
     */
    class Generator {
    public:
        virtual ~Generator() = default;

        Generator() = default;
        Generator(const Generator &) = delete;
        Generator &operator=(const Generator &) = delete;
        Generator(Generator &&) = delete;
        Generator &operator=(Generator &&) = delete;

        virtual std::vector<std::unique_ptr<Voxel3D>> generate(BrickPool &pool,
                                                                float voxelSize) = 0;
    };
}
