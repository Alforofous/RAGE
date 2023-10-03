cmake_minimum_required(VERSION 3.1)

project(RAGE CXX)

set(SOURCES_DIRECTORY "./sources/")
set(INCLUDES_DIRECTORY "./includes/")

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
)

list(TRANSFORM SOURCES PREPEND ${SOURCES_DIRECTORY})
list(TRANSFORM INCLUDES PREPEND ${INCLUDES_DIRECTORY})

message("SOURCES: ${SOURCES}")
message("INCLUDES: ${INCLUDES}")

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(libraries/glfw-3.3.8)

add_executable(RAGE ${SOURCES} ${INCLUDES})
include_directories(${INCLUDES_DIRECTORY})
target_link_libraries(RAGE glfw)
