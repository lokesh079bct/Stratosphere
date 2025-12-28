cmake_minimum_required(VERSION 3.20)
project(Sample LANGUAGES CXX)

add_executable(SampleApp src/main.cpp)
target_link_libraries(SampleApp PRIVATE Engine)
target_include_directories(SampleApp PRIVATE ${CMAKE_SOURCE_DIR}/Engine/include)

# Option: run OBJ -> SMESH conversion for Sample assets during build
option(STRATO_PROCESS_SAMPLE_OBJ "Convert Sample OBJ assets to cooked SMESH during build" ON)

if (STRATO_PROCESS_SAMPLE_OBJ)
    add_custom_target(ProcessSampleOBJAssets
        COMMAND ObjToSMesh
            ${CMAKE_SOURCE_DIR}/Sample/assets/raw/ObjModels
            ${CMAKE_SOURCE_DIR}/Sample/assets/cooked/ObjModels
        DEPENDS ObjToSMesh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Conditioning Sample OBJ assets to SMESH"
    )
    add_dependencies(SampleApp ProcessSampleOBJAssets)

endif()
# Copy cooked SMESH assets next to the executable for relative imports
    add_custom_command(TARGET SampleApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:SampleApp>/assets/ObjModels
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/Sample/assets/cooked/ObjModels
            $<TARGET_FILE_DIR:SampleApp>/assets/ObjModels
        COMMENT "Copying Sample SMESH assets to runtime output"
    )

# Copy SPIR-V shaders to runtime output dir (expects Engine/Shaders/*.spv present)
file(GLOB SHADER_SPV ${CMAKE_SOURCE_DIR}/Engine/shaders/*.spv)
foreach(SPIRV ${SHADER_SPV})
    add_custom_command(TARGET SampleApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:SampleApp>/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SPIRV} $<TARGET_FILE_DIR:SampleApp>/shaders/
    )
endforeach()