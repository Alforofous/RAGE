cmake_minimum_required(VERSION 3.16...3.28)

# Prefer LLVM/Clang on Windows when installed at the canonical location (avoids MinGW being
# picked up via PATH). Skipped silently when the path doesn't exist (Linux / WSL / Mac) or
# when the user has already pinned the compiler via -DCMAKE_*_COMPILER or env CC/CXX.
if(NOT DEFINED CMAKE_CXX_COMPILER AND EXISTS "C:/Program Files/LLVM/bin/clang++.exe")
    set(CMAKE_C_COMPILER "C:/Program Files/LLVM/bin/clang.exe")
    set(CMAKE_CXX_COMPILER "C:/Program Files/LLVM/bin/clang++.exe")
endif()

set(CMAKE_CXX_STANDARD 20)
project(RAGE CXX C)

# Debug symbols configuration
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3 -ggdb -O0")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g3 -ggdb -O0")
set(CMAKE_BUILD_TYPE Debug)

# Generate compile_commands.json for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Build profile ---
# Single switch for "dev vs production". Dev = profiling client linked + dev-only UI compiled in.
# Production = lean binary, no Tracy, no dev panels. Sub-flags (RAGE_ENABLE_PROFILING, ...) default
# to follow RAGE_DEV_BUILD so a normal user only needs to flip this one option.
option(RAGE_DEV_BUILD "Compile as dev build (profiling, debug UI features). Default OFF (production); -DRAGE_DEV_BUILD=ON for dev." OFF)
set(RAGE_ENABLE_PROFILING ${RAGE_DEV_BUILD})

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
    src/engine/content/vox_loader.cpp
    src/engine/materials/pipeline_cache.cpp
    src/engine/rendering/renderer.cpp
    src/engine/scene/camera.cpp
    src/engine/scene/directional_light.cpp
    src/engine/scene/node3d.cpp
    src/engine/scene/renderable_node3d.cpp
    src/engine/scene/brick_pool.cpp
    src/engine/scene/voxel3d.cpp
    src/engine/scene/voxel_data.cpp
    src/engine/scene/voxel_occupancy_mip.cpp
    src/engine/rendering/world_brick_grid.cpp
    src/engine/rendering/world_grid.cpp
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

# --- Shader compilation ---

find_program(GLSLC NAMES glslc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
if(NOT GLSLC)
    message(FATAL_ERROR "glslc not found; set VULKAN_SDK or install Vulkan SDK")
endif()

set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/shaders/*.comp")
set(SHADER_OUTPUTS "")
foreach(SHADER_SRC ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER_SRC} NAME)
    set(SPV_OUT "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPV_OUT}
        COMMAND ${GLSLC} ${SHADER_SRC} -o ${SPV_OUT}
        DEPENDS ${SHADER_SRC}
        COMMENT "Compiling ${SHADER_NAME} -> ${SHADER_NAME}.spv"
        VERBATIM
    )
    list(APPEND SHADER_OUTPUTS ${SPV_OUT})
endforeach()

add_custom_target(shaders ALL DEPENDS ${SHADER_OUTPUTS})

# --- Sample asset generation ---

add_executable(make_sample_vox tools/make_sample_vox.cpp)
set_target_properties(make_sample_vox PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(SAMPLE_ASSETS_DIR "${CMAKE_BINARY_DIR}/assets")
set(SAMPLE_VOX_OUTPUTS
    "${SAMPLE_ASSETS_DIR}/sphere.vox"
    "${SAMPLE_ASSETS_DIR}/floor.vox"
)
add_custom_command(
    OUTPUT ${SAMPLE_VOX_OUTPUTS}
    COMMAND make_sample_vox ${SAMPLE_ASSETS_DIR}
    DEPENDS make_sample_vox
    COMMENT "Generating sample voxel assets -> ${SAMPLE_ASSETS_DIR}"
    VERBATIM
)
add_custom_target(sample_assets ALL DEPENDS ${SAMPLE_VOX_OUTPUTS})

# --- App (temporary test executable) ---

add_executable(RAGE
    src/app/debug_ui.cpp
    src/app/free_fly_controller.cpp
    src/app/main.cpp
    src/app/profiler.cpp
    src/app/window.cpp
)
target_link_libraries(RAGE rage_engine imgui)
add_dependencies(RAGE shaders sample_assets)

if(RAGE_ENABLE_PROFILING)
    target_compile_definitions(RAGE PRIVATE RAGE_PROFILING_TRACY)
endif()
if(RAGE_DEV_BUILD)
    target_compile_definitions(RAGE PRIVATE RAGE_DEV)
endif()

if(RAGE_DEV_BUILD)
    set(RAGE_DEV_BUILD_BOOL "true")
else()
    set(RAGE_DEV_BUILD_BOOL "false")
endif()

configure_file(
    "${CMAKE_SOURCE_DIR}/src/app/build_paths.hpp.in"
    "${CMAKE_BINARY_DIR}/generated/app/build_paths.hpp"
    @ONLY
)
target_include_directories(RAGE PRIVATE "${CMAKE_BINARY_DIR}/generated")

# --- Tests ---

add_subdirectory(tests/)
