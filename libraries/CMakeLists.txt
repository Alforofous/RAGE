cmake_minimum_required(VERSION 3.16...3.28)
set(CMAKE_CXX_STANDARD 11)
project(RAGE CXX C)

# Configure GLFW options before adding subdirectory
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target" FORCE)

# Add GLFW as a subdirectory
add_subdirectory(glfw)

# Add ImGui as a subdirectory
add_subdirectory(imgui)

# Add GLM as a subdirectory
add_subdirectory(glm)

# Create a project_libraries target that includes all our libraries
add_library(project_libraries INTERFACE)
target_link_libraries(project_libraries INTERFACE glfw imgui glm)