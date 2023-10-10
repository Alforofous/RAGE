cmake_minimum_required(VERSION 3.1)

project(RAGE CXX)

#find_package(OpenGL REQUIRED)

set(SOURCES_DIRECTORY "./sources/")
set(INCLUDES_DIRECTORY "./includes/")
set(SHADERS_DIRECTORY "./shaders/")

set(
	SOURCES
	main.cpp
	utility_functions.cpp
	RAGE.cpp
	RAGE_window.cpp
	RAGE_camera.cpp
	RAGE_object.cpp
	RAGE_transform.cpp
)

set(
	INCLUDES
	RAGE.hpp
	RAGE_window.hpp
	ShaderLoader.hpp
	RAGE_camera.hpp
	RAGE_object.hpp
	RAGE_transform.hpp
)

set(
	SHADERS
	fragment_test.glsl
	vertex_test.glsl
)

list(TRANSFORM SOURCES PREPEND ${SOURCES_DIRECTORY})
list(TRANSFORM INCLUDES PREPEND ${INCLUDES_DIRECTORY})
list(TRANSFORM SHADERS PREPEND ${SHADERS_DIRECTORY})

source_group("Shader Files" FILES ${SHADERS})

message("SOURCES: ${SOURCES}")
message("INCLUDES: ${INCLUDES}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT RAGE)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# Disable building extras we won't need (pure C++ project)
set(NANOGUI_BUILD_EXAMPLE OFF CACHE BOOL " " FORCE)
set(NANOGUI_BUILD_PYTHON  OFF CACHE BOOL " " FORCE)
set(NANOGUI_INSTALL       OFF CACHE BOOL " " FORCE)

# Add the configurations from nanogui
add_subdirectory(libraries/nanogui)
add_definitions(${NANOGUI_EXTRA_DEFS})

# For reliability of parallel build, make the NanoGUI targets dependencies
set_property(TARGET glfw glfw_objects nanogui PROPERTY FOLDER "dependencies")

add_executable(RAGE ${SOURCES} ${INCLUDES} ${SHADERS})
include_directories(${INCLUDES_DIRECTORY})
include_directories(libraries/glm/)
include_directories(libraries/)
include_directories(libraries/nanogui/include)
include_directories(${NANOGUI_EXTRA_INCS})
target_link_libraries(RAGE nanogui ${NANOGUI_EXTRA_LIBS})
