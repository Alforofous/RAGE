# Tests CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

add_executable(rage_tests
    platform/gpu/gpu_allocator_test.cpp
    platform/gpu/gpu_command_test.cpp
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
