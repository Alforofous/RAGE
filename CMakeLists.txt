cmake_minimum_required(VERSION 3.1)

project(RAGE CXX)

set(SOURCES_DIRECTORY "./sources/")

set(SOURCES
	main.cpp
	RAGE_window.cpp
)

list(TRANSFORM SOURCES PREPEND ${SOURCES_DIRECTORY})

message("SOURCES: ${SOURCES}")

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(libraries/glfw-3.3.8)

add_executable(RAGE ${SOURCES})

target_link_libraries(RAGE glfw)
