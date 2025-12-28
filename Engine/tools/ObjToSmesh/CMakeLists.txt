cmake_minimum_required(VERSION 3.20)
project(ObjToSMesh LANGUAGES CXX)

# Download the header file to the build dir (works behind firewalls too)
set(TINYOBJ_HEADER ${CMAKE_CURRENT_BINARY_DIR}/tiny_obj_loader.h)
file(DOWNLOAD
    https://raw.githubusercontent.com/tinyobjloader/tinyobjloader/master/tiny_obj_loader.h
    ${TINYOBJ_HEADER}
    SHOW_PROGRESS
)

add_executable(ObjToSMesh
    ObjToSmesh.cpp
)

target_include_directories(ObjToSMesh
    PRIVATE
      ${CMAKE_SOURCE_DIR}/Engine/include
      ${CMAKE_CURRENT_BINARY_DIR}   # where tiny_obj_loader.h was downloaded
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
        target_link_libraries(ObjToSMesh PRIVATE stdc++fs)
    endif()
endif()

target_compile_features(ObjToSMesh PRIVATE cxx_std_17)