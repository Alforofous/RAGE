cmake_minimum_required(VERSION 3.7)
set (CMAKE_CXX_STANDARD 17)
project(RAGE CXX C)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(glfw)
add_subdirectory(glad)
add_subdirectory(glm)
add_subdirectory(imgui)

set(GLM_DIRECTORY glm)
set(GLAD_DIRECTORY glad)
set(GLFW_DIRECTORY glfw)
set(IMGUI_DIRECTORY imgui)

list(APPEND LIBRARY_INCLUDE_DIRECTORIES
	${GLAD_DIRECTORY}
	${GLFW_DIRECTORY}/include
	${IMGUI_DIRECTORY}
	${GLM_DIRECTORY}
)

set(NAME project_libraries)
add_library(${NAME} INTERFACE)
target_include_directories(${NAME} INTERFACE ${LIBRARY_INCLUDE_DIRECTORIES})
target_link_libraries(${NAME} INTERFACE imgui glfw glm glad)