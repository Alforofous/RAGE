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

# Build ImGui as a static library (no CMakeLists.txt in upstream repo)
add_library(imgui STATIC
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
)
target_include_directories(imgui PUBLIC imgui)

# Add GLM as a subdirectory
add_subdirectory(glm)

# Tracy profiler: opt-in client library. Off by default so non-profiling builds don't carry
# its threads or symbols. Enable with -DRAGE_ENABLE_PROFILING=ON at configure time.
option(RAGE_ENABLE_PROFILING "Build and link the Tracy profiler client" OFF)
if(RAGE_ENABLE_PROFILING)
    set(TRACY_ENABLE ON CACHE BOOL "" FORCE)
    set(TRACY_ON_DEMAND ON CACHE BOOL "" FORCE)
    add_subdirectory(tracy)
endif()

# Create a project_libraries target that includes all our libraries
add_library(project_libraries INTERFACE)
target_link_libraries(project_libraries INTERFACE glfw imgui glm)
if(RAGE_ENABLE_PROFILING)
    target_link_libraries(project_libraries INTERFACE TracyClient)
endif()
