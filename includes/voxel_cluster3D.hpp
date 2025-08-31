#pragma once
#include "node3D.hpp"

/**
 * A cluster of voxels in 3D space.
 */
class VoxelCluster3D : public Node3D {
public:
    VoxelCluster3D();
    ~VoxelCluster3D();

    void update();

private:
    std::vector<Voxel3D> voxels;
};