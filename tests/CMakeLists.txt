# Tests CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

add_executable(rage_tests
    engine/content/vox_loader_test.cpp
    engine/materials/material_test.cpp
    engine/rendering/frame_context_test.cpp
    engine/rendering/world_grid_test.cpp
    engine/scene/camera_test.cpp
    engine/scene/voxel_occupancy_mip_test.cpp
    engine/scene/node3d_test.cpp
    engine/scene/renderable_node3d_test.cpp
    platform/gpu/gpu_allocator_test.cpp
    platform/gpu/gpu_command_test.cpp
    shared/math/color_test.cpp
    shared/math/ivec_test.cpp
    shared/math/mat_test.cpp
    shared/math/quat_test.cpp
    shared/math/vec_test.cpp
    shared/small_vector_test.cpp
)

target_link_libraries(rage_tests
    gtest
    gtest_main
    gmock
    gmock_main
    rage_engine
)

target_include_directories(rage_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/shared
    ${CMAKE_SOURCE_DIR}/src/platform
    ${CMAKE_SOURCE_DIR}/src/platform/gpu
)

enable_testing()
add_test(NAME RageTests COMMAND rage_tests)
