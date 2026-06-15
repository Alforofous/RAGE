cmake_minimum_required(VERSION 3.16...3.28)

# Use Clang compiler instead of MinGW
set(CMAKE_C_COMPILER "C:/Program Files/LLVM/bin/clang.exe")
set(CMAKE_CXX_COMPILER "C:/Program Files/LLVM/bin/clang++.exe")

set(CMAKE_CXX_STANDARD 20)
project(RAGE CXX C)

# Debug symbols configuration
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3 -ggdb -O0")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g3 -ggdb -O0")
set(CMAKE_BUILD_TYPE Debug)

# Generate compile_commands.json for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Libraries ---

set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "Build SPIRV-Reflect static library" FORCE)
set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "Don't build SPIRV-Reflect executable" FORCE)
set(SPIRV_REFLECT_BUILD_TESTS OFF CACHE BOOL "Don't build SPIRV-Reflect tests" FORCE)
add_subdirectory(libraries/spirv-reflect)

set(BUILD_GMOCK ON CACHE BOOL "Build Google Mock" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "Don't install Google Test" FORCE)
add_subdirectory(libraries/googletest)

add_subdirectory(libraries/)

# --- Engine library ---

set(ENGINE_SOURCES
    src/platform/gpu/vulkan/vma_impl.cpp
    src/platform/gpu/vulkan/vulkan_allocator.cpp
    src/platform/gpu/vulkan/vulkan_image.cpp
    src/platform/gpu/vulkan/vulkan_context.cpp
    src/platform/gpu/vulkan/vulkan_instance.cpp
    src/platform/gpu/vulkan/vulkan_device.cpp
    src/platform/gpu/vulkan/vulkan_fence_pool.cpp
    src/platform/gpu/vulkan/vulkan_compute_pipeline.cpp
    src/platform/gpu/vulkan/vulkan_descriptor_layout_cache.cpp
    src/platform/gpu/vulkan/vulkan_descriptor_pool.cpp
    src/platform/gpu/vulkan/vulkan_descriptor_writer.cpp
    src/platform/gpu/vulkan/vulkan_pipeline.cpp
    src/platform/gpu/vulkan/vulkan_render_target.cpp
    src/platform/gpu/vulkan/vulkan_semaphore_pool.cpp
    src/platform/gpu/vulkan/vulkan_shader_module.cpp
    src/platform/gpu/vulkan/vulkan_swapchain.cpp
)

add_library(rage_engine STATIC ${ENGINE_SOURCES})

target_include_directories(rage_engine PUBLIC
    src/
    src/shared
    src/platform
    src/platform/gpu
)

target_include_directories(rage_engine SYSTEM PUBLIC
    libraries/
    libraries/stb
    libraries/glfw/include
    libraries/imgui
    libraries/glm
    libraries/spirv-reflect
    libraries/vma/include
    $ENV{VULKAN_SDK}/Include
)

if(DEFINED ENV{VULKAN_SDK})
    target_link_directories(rage_engine PUBLIC "$ENV{VULKAN_SDK}/Lib")
    target_link_libraries(rage_engine vulkan-1)
endif()

target_link_libraries(rage_engine project_libraries spirv-reflect-static)

# --- App (temporary test executable) ---

add_executable(RAGE src/app/main.cpp)
target_link_libraries(RAGE rage_engine)

# --- Tests ---

add_subdirectory(tests/)
