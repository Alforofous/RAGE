cmake_minimum_required(VERSION 3.7)
set (CMAKE_CXX_STANDARD 17)
project(RAGE CXX C)

message("CMAKE ${PROJECT_SOURCE_DIR}")

set(SOURCES_DIRECTORY "./sources/")
set(INCLUDES_DIRECTORY "./includes/")
set(SHADERS_DIRECTORY "./shaders/")
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(
	SOURCES
	main.cpp
	utility_functions.cpp
	set_callbacks.cpp
	RAGE.cpp
	RAGE_window.cpp
	RAGE_shader.cpp
	RAGE_camera.cpp
	RAGE_object.cpp
	RAGE_transform.cpp
	RAGE_gui.cpp
	RAGE_scene_view.cpp
	RAGE_scene.cpp
	RAGE_mesh.cpp
	RAGE_primitive_objects.cpp
	RAGE_keyboard_input.cpp
	vertex_array.cpp
	vertex_buffer.cpp
	element_buffer.cpp
	GLobject.cpp
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
	RAGE_scene.hpp
	RAGE_mesh.hpp
	RAGE_primitive_objects.hpp
	RAGE_keyboard_input.hpp
	RAGE_user_input.hpp
	vertex_array.hpp
	vertex_buffer.hpp
	element_buffer.hpp
	GLobject.hpp
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

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/..)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/..)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/..)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT RAGE)

add_subdirectory(libraries/)

list(APPEND PROJECT_INCLUDES
	${INCLUDES_DIRECTORY}
	${LIBRARY_INCLUDE_DIRECTORIES}
	libraries/
)

add_executable(RAGE ${SOURCES} ${INCLUDES} ${SHADERS})
target_include_directories(RAGE PRIVATE ${PROJECT_INCLUDES})
target_link_libraries(RAGE project_libraries)