cmake_minimum_required(VERSION 3.20)
project(Engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Compiler warnings ---
if (MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# --- Dependencies: Vulkan (system) ---
find_package(Vulkan REQUIRED)

# --- Vendor GLFW as static using FetchContent ---
include(FetchContent)

# Force static linking for projects that honor BUILD_SHARED_LIBS
# (This is a cache variable so set it before FetchContent_MakeAvailable)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build libraries static" FORCE)

# GLFW-specific options (turn off extras)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "GLFW docs" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "GLFW tests" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "GLFW examples" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "GLFW install target" FORCE)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        3.3.8
)
FetchContent_MakeAvailable(glfw)

# Modern GLFW provides a target named 'glfw' (static in this config).
# If your environment produces a different target name, adapt accordingly.

# --- Engine target ---
add_library(Engine STATIC
    src/Application.cpp
    src/VulkanContext.cpp
    src/GLFWWindow.cpp
    src/SwapChain.cpp
    src/Renderer.cpp
    src/TrianglesRenderPassModule.cpp
    src/MeshRenderPassModule.cpp
    src/Pipeline.cpp
    src/BufferUtils.cpp
    src/SMeshLoader.cpp
    # add other engine sources here...
)

target_include_directories(Engine
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link static GLFW and system Vulkan loader
target_link_libraries(Engine
    PUBLIC
        Vulkan::Vulkan
    PRIVATE
        glfw    # static target from FetchContent
)

# On some platforms/linkers you might need to link additional system libs.
# GLFW target typically handles its own transitive dependencies, but if you run into
# undefined references on Windows/Linux, add them here conditionally.

# Example: on Linux you may need to link pthread explicitly for some builds:
if (UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(Engine PRIVATE Threads::Threads)
endif()

# If you plan to use volk (optional), you can add it here as a source file:
# add_subdirectory(vendor/volk) or FetchContent_Declare volk and link it.

# Expose a usage requirement to consumers (e.g., to include Engine include dir)
target_compile_features(Engine PUBLIC cxx_std_17)

add_subdirectory(tools/ObjToSmesh)