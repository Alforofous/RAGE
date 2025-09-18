cmake_minimum_required(VERSION 3.16...3.28)

# Use Clang compiler instead of MinGW
set(CMAKE_C_COMPILER "C:/Program Files/LLVM/bin/clang.exe")
set(CMAKE_CXX_COMPILER "C:/Program Files/LLVM/bin/clang++.exe")

set(CMAKE_CXX_STANDARD 17)
project(RAGE CXX C)

# Debug symbols configuration for GDB
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3 -ggdb -O0")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g3 -ggdb -O0")
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-export-all-symbols")

# Generate compile_commands.json for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message("CMAKE ${PROJECT_SOURCE_DIR}")

set(SOURCES_DIRECTORY "sources")
set(INCLUDES_DIRECTORY "includes")
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(PRECOMPILE_HEADERS
    renderable_node3D.hpp
)
list(TRANSFORM PRECOMPILE_HEADERS PREPEND ${INCLUDES_DIRECTORY}/)

set(SOURCES
	main.cpp
	app.cpp
	window.cpp
	renderer.cpp
	vulkan_utils.cpp
	vulkan_render_target.cpp
	vulkan_swapchain_manager.cpp
	node3D.cpp
    matrix4.cpp
    vector2.cpp
    vector3.cpp
    vector4.cpp
    int_vector3.cpp
    voxel3D.cpp
    camera.cpp
    camera_controls.cpp
    input_handler.cpp
    scene.cpp
    renderable_node3D.cpp
    utils/file_reader.cpp
    utils/shader_compiler.cpp
    utils/buffer_utils.cpp
    vulkan_descriptor_manager.cpp
    materials/ray_tracing_material.cpp
    materials/voxel_ray_tracing_material.cpp
    materials/material.cpp
    pipelines/vulkan_pipeline.cpp
    pipelines/vulkan_ray_tracing_pipeline.cpp
    pipelines/shader_reflector.cpp
)

# Add SPIRV-Reflect as subdirectory
set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "Build SPIRV-Reflect static library" FORCE)
set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "Don't build SPIRV-Reflect executable" FORCE)
set(SPIRV_REFLECT_BUILD_TESTS OFF CACHE BOOL "Don't build SPIRV-Reflect tests" FORCE)
add_subdirectory(libraries/spirv-reflect)

# Add Google Test as subdirectory
set(BUILD_GMOCK ON CACHE BOOL "Build Google Mock" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "Don't install Google Test" FORCE)
add_subdirectory(libraries/googletest)
list(TRANSFORM SOURCES PREPEND ${SOURCES_DIRECTORY}/)



# Set RAGE as startup project
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT RAGE)

add_subdirectory(libraries/)
add_subdirectory(tests/)

list(APPEND PROJECT_INCLUDES
	${INCLUDES_DIRECTORY}
	${LIBRARY_INCLUDE_DIRECTORIES}
	libraries/
	libraries/stb
	libraries/glfw/include
	libraries/imgui
	libraries/glm
	libraries/spirv-reflect
	libraries/
	$ENV{VULKAN_SDK}/Include
)

# Add Vulkan SDK include directories if available
if(DEFINED ENV{VULKAN_SDK})
    # Add the main Vulkan include directory
    list(APPEND PROJECT_INCLUDES "$ENV{VULKAN_SDK}/Include")
    message(STATUS "Added Vulkan include directory: $ENV{VULKAN_SDK}/Include")
    
    # Check if vulkan.h actually exists
    if(EXISTS "$ENV{VULKAN_SDK}/Include/vulkan/vulkan.h")
        message(STATUS "Found vulkan.h at: $ENV{VULKAN_SDK}/Include/vulkan/vulkan.h")
        
        # Check if vulkan_core.h exists (the main header that vulkan.h includes)
        if(EXISTS "$ENV{VULKAN_SDK}/Include/vulkan/vulkan_core.h")
            message(STATUS "Found vulkan_core.h at: $ENV{VULKAN_SDK}/Include/vulkan/vulkan_core.h")
        else()
            message(WARNING "vulkan_core.h not found - this may cause compilation issues")
        endif()
    else()
        message(WARNING "vulkan.h not found in expected location: $ENV{VULKAN_SDK}/Include/vulkan/vulkan.h")
        
        # List contents of the vulkan directory to help debug
        if(EXISTS "$ENV{VULKAN_SDK}/Include/vulkan")
            file(GLOB VULKAN_VULKAN_CONTENTS "$ENV{VULKAN_SDK}/Include/vulkan/*")
            message(STATUS "Contents of $ENV{VULKAN_SDK}/Include/vulkan: ${VULKAN_VULKAN_CONTENTS}")
        endif()
    endif()
else()
    message(WARNING "VULKAN_SDK environment variable not set")
    
    # Try to find Vulkan SDK in common locations
    set(VULKAN_POSSIBLE_PATHS
        "C:/VulkanSDK"
        "C:/Program Files/VulkanSDK"
        "C:/Program Files (x86)/VulkanSDK"
    )
    
    foreach(VULKAN_PATH ${VULKAN_POSSIBLE_PATHS})
        if(EXISTS "${VULKAN_PATH}")
            # Look for versioned subdirectories
            file(GLOB VULKAN_VERSIONS LIST_DIRECTORIES true "${VULKAN_PATH}/*")
            foreach(VERSION_PATH ${VULKAN_VERSIONS})
                if(IS_DIRECTORY ${VERSION_PATH})
                    if(EXISTS "${VERSION_PATH}/Include/vulkan/vulkan.h")
                        message(STATUS "Found Vulkan SDK at: ${VERSION_PATH}")
                        list(APPEND PROJECT_INCLUDES "${VERSION_PATH}/Include")
                        break()
                    endif()
                endif()
            endforeach()
        endif()
    endforeach()
endif()

add_executable(
	RAGE
	${SOURCES}
	${SHADERS}
)

target_include_directories(RAGE PUBLIC ${PROJECT_INCLUDES})
target_precompile_headers(RAGE PUBLIC ${PRECOMPILE_HEADERS})

# Add Vulkan library directory if available
if(DEFINED ENV{VULKAN_SDK})
    target_link_directories(RAGE PRIVATE "$ENV{VULKAN_SDK}/Lib")
    message(STATUS "Added Vulkan library directory: $ENV{VULKAN_SDK}/Lib")
    target_link_libraries(RAGE vulkan-1 shaderc_shared)
else()
    # Try to find Vulkan library in common locations
    set(VULKAN_LIB_POSSIBLE_PATHS
        "C:/VulkanSDK/*/Lib"
        "C:/Program Files/VulkanSDK/*/Lib"
        "C:/Program Files (x86)/VulkanSDK/*/Lib"
    )
    
    foreach(VULKAN_LIB_PATH ${VULKAN_LIB_POSSIBLE_PATHS})
        file(GLOB VULKAN_LIB_DIRS ${VULKAN_LIB_PATH})
        foreach(LIB_DIR ${VULKAN_LIB_DIRS})
            if(EXISTS "${LIB_DIR}/vulkan-1.lib")
                message(STATUS "Found Vulkan library at: ${LIB_DIR}")
                target_link_directories(RAGE PRIVATE "${LIB_DIR}")
                target_link_libraries(RAGE vulkan-1 shaderc_shared)
                break()
            endif()
        endforeach()
    endforeach()
endif()

target_link_libraries(RAGE project_libraries spirv-reflect-static)

# Copy shader source files to build directory (for runtime loading)
add_custom_target(copy_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        ${CMAKE_BINARY_DIR}/shaders
    COMMENT "Copying shader source files to build directory"
)

# Make sure shaders are copied before the main target
add_dependencies(${PROJECT_NAME} copy_shaders)