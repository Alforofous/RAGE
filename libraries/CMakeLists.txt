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
option(RAGE_ENABLE_PROFILING "Build and link the Tracy profiler client" ON)
if(RAGE_ENABLE_PROFILING)
    set(TRACY_ENABLE ON CACHE BOOL "" FORCE)
    set(TRACY_ON_DEMAND ON CACHE BOOL "" FORCE)
    add_subdirectory(tracy)

    # Download the prebuilt Tracy profiler GUI on Windows so users don't have to fetch it
    # manually. Skipped on non-Windows platforms — build Tracy's profiler/ from source there.
    #
    # Uses curl rather than CMake's file(DOWNLOAD) because the latter relies on libcurl's
    # bundled CA store, which fails the GitHub CDN TLS handshake on some Windows toolchains
    # (clang+MSVC stdlib observed). The system curl (Windows 10/11 builtin / Git Bash) uses
    # the platform certificate store and works reliably.
    if(WIN32)
        set(TRACY_SERVER_VERSION "0.13.1")
        set(TRACY_SERVER_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/tracy-server")
        set(TRACY_SERVER_EXE  "${TRACY_SERVER_DIR}/tracy-profiler.exe")
        if(NOT EXISTS "${TRACY_SERVER_EXE}")
            find_program(CURL_EXECUTABLE curl)
            if(NOT CURL_EXECUTABLE)
                message(FATAL_ERROR
                    "curl not found on PATH. Install curl, or set RAGE_ENABLE_PROFILING=OFF "
                    "and download Tracy manually from "
                    "https://github.com/wolfpld/tracy/releases/tag/v${TRACY_SERVER_VERSION}")
            endif()
            set(_zip "${TRACY_SERVER_DIR}/windows-${TRACY_SERVER_VERSION}.zip")
            file(MAKE_DIRECTORY "${TRACY_SERVER_DIR}")
            message(STATUS "Downloading Tracy server v${TRACY_SERVER_VERSION} (~12 MB) via curl...")
            execute_process(
                COMMAND "${CURL_EXECUTABLE}" -fsSL
                    --output "${_zip}"
                    "https://github.com/wolfpld/tracy/releases/download/v${TRACY_SERVER_VERSION}/windows-${TRACY_SERVER_VERSION}.zip"
                RESULT_VARIABLE _dlCode
            )
            if(NOT _dlCode EQUAL 0)
                message(FATAL_ERROR "Tracy server download failed (curl exit ${_dlCode})")
            endif()
            file(ARCHIVE_EXTRACT INPUT "${_zip}" DESTINATION "${TRACY_SERVER_DIR}")
            file(REMOVE "${_zip}")
        endif()
        # Expose the resolved path to dependents (e.g. the RAGE executable) for runtime use.
        set(RAGE_TRACY_SERVER_EXE "${TRACY_SERVER_EXE}" CACHE INTERNAL "Path to tracy-profiler.exe")
    endif()
endif()

# Create a project_libraries target that includes all our libraries
add_library(project_libraries INTERFACE)
target_link_libraries(project_libraries INTERFACE glfw imgui glm)
if(RAGE_ENABLE_PROFILING)
    target_link_libraries(project_libraries INTERFACE TracyClient)
endif()
