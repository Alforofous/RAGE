cmake_minimum_required(VERSION 3.1)

project(RAGE CXX)

find_package(OpenGL REQUIRED)

set(SOURCES_DIRECTORY "./sources/")
set(INCLUDES_DIRECTORY "./includes/")
set(SHADERS_DIRECTORY "./shaders/")

set(
	SOURCES
	main.cpp
	utility_functions.cpp
	RAGE.cpp
	RAGE_window.cpp
)

set(
	INCLUDES
	RAGE.hpp
	RAGE_window.hpp
	ShaderLoader.hpp
)

set(
	SHADERS
	fragment_test.glsl
	vertex_test.glsl
)

set(GLAD_C_FILE libraries/glad/glad.c)

list(TRANSFORM SOURCES PREPEND ${SOURCES_DIRECTORY})
list(TRANSFORM INCLUDES PREPEND ${INCLUDES_DIRECTORY})
list(TRANSFORM SHADERS PREPEND ${SHADERS_DIRECTORY})

source_group("Shader Files" FILES ${SHADERS})

message("SOURCES: ${SOURCES}")
message("INCLUDES: ${INCLUDES}")

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(libraries/glfw-3.3.8)

add_executable(RAGE ${SOURCES} ${INCLUDES} ${GLAD_C_FILE} ${SHADERS})
include_directories(${INCLUDES_DIRECTORY})
include_directories(libraries/glm/)
include_directories(libraries/)
target_link_libraries(RAGE PUBLIC glfw ${OPENGL_LIBRARY})
