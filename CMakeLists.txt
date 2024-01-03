cmake_minimum_required(VERSION 3.7)
set(CMAKE_CXX_STANDARD 17)
project(RAGE CXX C)

message("CMAKE ${PROJECT_SOURCE_DIR}")

set(SOURCES_DIRECTORY "sources")
set(GUI_DIRECTORY "gui")
set(PHYSICS_DIRECTORY "physics")
set(INCLUDES_DIRECTORY "includes")
set(SHADERS_DIRECTORY "shaders")
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(
	GUI_SOURCES
	RAGE_gui.cpp
	RAGE_menu_bar.cpp
	RAGE_scene_view.cpp
	RAGE_inspector.cpp
)
list(TRANSFORM GUI_SOURCES PREPEND ${SOURCES_DIRECTORY}/${GUI_DIRECTORY}/)

set(
	PHYSICS_SOURCES
	RAGE_ray_tracing.cpp
)
list(TRANSFORM PHYSICS_SOURCES PREPEND ${SOURCES_DIRECTORY}/${PHYSICS_DIRECTORY}/)

set(
	GUI_INCLUDES
	RAGE_gui.hpp
	RAGE_menu_bar.hpp
	RAGE_scene_view.hpp
	RAGE_inspector.hpp
)
list(TRANSFORM GUI_INCLUDES PREPEND ${INCLUDES_DIRECTORY}/${GUI_DIRECTORY}/)

set(
	PHYSICS_INCLUDES
	RAGE_ray_tracing.hpp
)
list(TRANSFORM PHYSICS_INCLUDES PREPEND ${INCLUDES_DIRECTORY}/${PHYSICS_DIRECTORY}/)

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
	RAGE_scene.cpp
	RAGE_scene_glb_loader.cpp
	RAGE_mesh.cpp
	RAGE_primitive_objects.cpp
	RAGE_keyboard_input.cpp
	RAGE_primitive.cpp
	vertex_array.cpp
	object_buffer.cpp
)
list(TRANSFORM SOURCES PREPEND ${SOURCES_DIRECTORY}/)

set(
	INCLUDES
	RAGE.hpp
	RAGE_window.hpp
	RAGE_shader.hpp
	RAGE_camera.hpp
	RAGE_object.hpp
	RAGE_transform.hpp
	RAGE_scene.hpp
	RAGE_mesh.hpp
	RAGE_primitive_objects.hpp
	RAGE_keyboard_input.hpp
	RAGE_user_input.hpp
	RAGE_primitive.hpp
	vertex_array.hpp
	object_buffer.hpp
)
list(TRANSFORM INCLUDES PREPEND ${INCLUDES_DIRECTORY}/)

set(
	SHADERS
	fragment_test.glsl
	vertex_test.glsl
)
list(TRANSFORM SHADERS PREPEND ${SHADERS_DIRECTORY}/)

source_group(${SHADERS_DIRECTORY} FILES ${SHADERS})
source_group(${SOURCES_DIRECTORY} FILES ${SOURCES})
source_group(${SOURCES_DIRECTORY}/${GUI_DIRECTORY} FILES ${GUI_SOURCES})
source_group(${SOURCES_DIRECTORY}/${PHYSICS_DIRECTORY} FILES ${PHYSICS_SOURCES})
source_group(${INCLUDES_DIRECTORY} FILES ${INCLUDES})
source_group(${INCLUDES_DIRECTORY}/${GUI_DIRECTORY} FILES ${GUI_INCLUDES})
source_group(${INCLUDES_DIRECTORY}/${PHYSICS_DIRECTORY} FILES ${PHYSICS_INCLUDES})

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

add_executable(RAGE ${SOURCES} ${GUI_SOURCES} ${PHYSICS_SOURCES} ${INCLUDES} ${GUI_INCLUDES} ${PHYSICS_INCLUDES} ${SHADERS})
target_include_directories(RAGE PRIVATE ${PROJECT_INCLUDES})
target_link_libraries(RAGE project_libraries)