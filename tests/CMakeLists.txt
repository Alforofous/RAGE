# Tests CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Create test executable
add_executable(rage_tests
    pipelines/vulkan_pipeline_test.cpp
)

# Link against Google Test, Google Mock, and project libraries
target_link_libraries(rage_tests
    gtest
    gtest_main
    gmock
    gmock_main
    spirv-reflect-static
    project_libraries
)

# Add Vulkan library
if(DEFINED ENV{VULKAN_SDK})
    target_link_directories(rage_tests PRIVATE "$ENV{VULKAN_SDK}/Lib")
    target_link_libraries(rage_tests vulkan-1 shaderc_shared)
endif()

# Include directories
target_include_directories(rage_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/includes
    ${CMAKE_SOURCE_DIR}/libraries
    ${CMAKE_SOURCE_DIR}/libraries/spirv-reflect
    $ENV{VULKAN_SDK}/Include
)

# Add test sources that need to be compiled
target_sources(rage_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/sources/pipelines/vulkan_pipeline.cpp
    ${CMAKE_SOURCE_DIR}/sources/utils/file_reader.cpp
    ${CMAKE_SOURCE_DIR}/sources/utils/shader_compiler.cpp
)

# Enable testing
enable_testing()

# Add test to CTest
add_test(NAME VulkanPipelineTests COMMAND rage_tests)
