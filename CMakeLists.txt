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
    src/engine/content/vox_loader.cpp
    src/engine/materials/pipeline_cache.cpp
    src/engine/rendering/renderer.cpp
    src/engine/scene/camera.cpp
    src/engine/scene/directional_light.cpp
    src/engine/scene/node3d.cpp
    src/engine/scene/renderable_node3d.cpp
    src/engine/scene/voxel3d.cpp
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

# --- App (temporary test executable) ---

add_executable(RAGE
    src/app/free_fly_controller.cpp
    src/app/main.cpp
    src/app/window.cpp
)
target_link_libraries(RAGE rage_engine)
add_dependencies(RAGE shaders)

# --- Tests ---

add_subdirectory(tests/)
