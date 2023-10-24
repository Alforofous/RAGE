cmake_minimum_required(VERSION 3.1)
project(RAGE CXX)

set(SOURCES_DIRECTORY "./sources/")
set(INCLUDES_DIRECTORY "./includes/")
set(SHADERS_DIRECTORY "./shaders/")

set(
	SOURCES
	main.cpp
	utility_functions.cpp
	set_callbacks.cpp
	set_shader_variable_values.cpp
	RAGE.cpp
	RAGE_window.cpp
	RAGE_camera.cpp
	RAGE_object.cpp
	RAGE_transform.cpp
	RAGE_gui.cpp
	RAGE_scene_view.cpp
)

set(
	INCLUDES
	RAGE.hpp
	RAGE_window.hpp
	RAGE_shader.hpp
	RAGE_camera.hpp
	RAGE_object.hpp
	RAGE_transform.hpp
	RAGE_gui.hpp
	RAGE_scene_view.hpp
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

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT RAGE)

add_subdirectory(libraries/)

list(APPEND PROJECT_INCLUDES
	${INCLUDES_DIRECTORY}
	${LIBRARY_INCLUDE_DIRECTORIES}
)

add_executable(RAGE ${SOURCES} ${INCLUDES} ${SHADERS})
target_include_directories(RAGE PRIVATE ${PROJECT_INCLUDES})
target_link_libraries(RAGE project_libraries)
