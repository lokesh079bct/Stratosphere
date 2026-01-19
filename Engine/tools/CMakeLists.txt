cmake_minimum_required(VERSION 3.20)
project(StratosphereTools LANGUAGES CXX)

include(FetchContent)

# ============================================================
# Common settings for all tools
# ============================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
# TinyOBJ - keep your existing "download header into build dir"
# ============================================================
set(TINYOBJ_HEADER ${CMAKE_CURRENT_BINARY_DIR}/tiny_obj_loader.h)

if (NOT EXISTS ${TINYOBJ_HEADER})
    file(DOWNLOAD
        https://raw.githubusercontent.com/tinyobjloader/tinyobjloader/master/tiny_obj_loader.h
        ${TINYOBJ_HEADER}
        SHOW_PROGRESS
    )
endif()

# ============================================================
# Assimp dependency for GltfToSModel
#
# Preferred behavior:
#   1) Try find_package(assimp) if installed on system
#   2) Otherwise FetchContent downloads it into build/_deps
# ============================================================
find_package(assimp QUIET)

if (NOT assimp_FOUND)
    message(STATUS "Assimp not found on system - FetchContent will download/build it into build/_deps")

    FetchContent_Declare(
        assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG v5.3.1
    )

    # ------------------------------
    # Make assimp build MUCH lighter
    # ------------------------------
    set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)

    # reduce extra import/export stuff
    set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)

    # enable only what we need
    set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_GLTF_EXPORTER OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(assimp)
endif()

# ============================================================
# Tool: ObjToSmesh
# ============================================================
add_executable(ObjToSMeshTool
    ObjToSmesh/ObjToSmesh.cpp
)

target_include_directories(ObjToSMeshTool PRIVATE
    ${CMAKE_SOURCE_DIR}/Engine/include
    ${CMAKE_CURRENT_BINARY_DIR}   # tiny_obj_loader.h downloaded here
)

# GCC < 9 std::filesystem workaround
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
        target_link_libraries(ObjToSMesh PRIVATE stdc++fs)
    endif()
endif()

# ============================================================
# Tool: GltfToSModel
# ============================================================
add_executable(GltfToSmodelTool
    GltfToSmodel/GltfToSmodel.cpp
)

target_include_directories(GltfToSmodelTool PRIVATE
    ${CMAKE_SOURCE_DIR}/Engine/include
)

# If we found system assimp, its target is usually "assimp::assimp"
# If FetchContent built it, the target is "assimp"
if (assimp_FOUND)
    target_link_libraries(GltfToSmodelTool PRIVATE assimp::assimp)
else()
    target_link_libraries(GltfToSmodelTool PRIVATE assimp)
endif()

# GCC < 9 std::filesystem workaround (only if you use filesystem in tool)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
        target_link_libraries(GltfToSmodelTool PRIVATE stdc++fs)
    endif()
endif()
