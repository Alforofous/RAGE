cmake_minimum_required(VERSION 3.7)
set (CMAKE_CXX_STANDARD 11)
project(RAGE CXX)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(glfw)
add_subdirectory(glad)
add_subdirectory(glm)

set(GLM_DIRECTORY glm)
set(GLAD_DIRECTORY glad)
set(GLFW_DIRECTORY glfw)
set(IMGUI_DIRECTORY imgui)

list(APPEND LIBRARY_INCLUDE_DIRECTORIES
	${GLAD_DIRECTORY}/glad
	${GLAD_DIRECTORY}
	${GLFW_DIRECTORY}/include
	${IMGUI_DIRECTORY}
	${GLM_DIRECTORY}
)

list(APPEND LIBRARY_SOURCES
	${IMGUI_DIRECTORY}/imgui_impl_opengl3.h
	${IMGUI_DIRECTORY}/imgui_impl_opengl3.cpp
	${IMGUI_DIRECTORY}/imgui_demo.cpp
	${IMGUI_DIRECTORY}/imgui_impl_glfw.h
	${IMGUI_DIRECTORY}/imgui_impl_glfw.cpp
)

set(NAME project_libraries)
add_library(${NAME} ${LIBRARY_SOURCES})
target_include_directories(${NAME} PUBLIC ${LIBRARY_INCLUDE_DIRECTORIES})
target_link_libraries(${NAME} imgui glfw glm glad)


list(APPEND IMGUI_PUBLIC_HEADERS
		"${IMGUI_DIRECTORY}/imgui.h"
		)

list(APPEND IMGUI_PRIVATE_HEADERS
		"${IMGUI_DIRECTORY}/imgui_internal.h"
		"${IMGUI_DIRECTORY}/imconfig.h"
		"${IMGUI_DIRECTORY}/imstb_rect_pack.h"
		"${IMGUI_DIRECTORY}/imstb_textedit.h"
		"${IMGUI_DIRECTORY}/imstb_truetype.h"
		)

list(APPEND IMGUI_SOURCES
		"${IMGUI_DIRECTORY}/imgui.cpp"
		"${IMGUI_DIRECTORY}/imgui_draw.cpp"
		"${IMGUI_DIRECTORY}/imgui_demo.cpp"
		"${IMGUI_DIRECTORY}/imgui_widgets.cpp"
		"${IMGUI_DIRECTORY}/imgui_tables.cpp"
		)

list(APPEND IMGUI_PUBLIC_INCLUDE_DIRS
		${IMGUI_DIRECTORY}
		)

list(APPEND IMGUI_PUBLIC_LIBRARIES)

set(NAME imgui)
add_library(${NAME} STATIC ${IMGUI_SOURCES})
target_link_libraries(${NAME} PUBLIC ${IMGUI_PUBLIC_LIBRARIES})
target_include_directories(${NAME}
		PUBLIC ${IMGUI_PUBLIC_INCLUDE_DIRS}
		PRIVATE ${IMGUI_PRIVATE_INCLUDE_DIRS}
		)
