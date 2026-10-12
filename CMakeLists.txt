cmake_minimum_required(VERSION 3.25)
project(VulkanPBR LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable debug symbols in release builds if desired or standard configurations
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find Dependencies via vcpkg / system
find_package(Vulkan REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(unofficial-vulkan-memory-allocator CONFIG REQUIRED)

# Find tinygltf header
find_path(TINYGLTF_INCLUDE_DIR NAMES tiny_gltf.h)
if(NOT TINYGLTF_INCLUDE_DIR)
    message(FATAL_ERROR "tiny_gltf.h not found! Please ensure tinygltf is installed.")
endif()

# Source files
file(GLOB_RECURSE SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

add_executable(VulkanPBR ${SOURCES})

target_include_directories(VulkanPBR PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    ${TINYGLTF_INCLUDE_DIR}
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(VulkanPBR PRIVATE
    Vulkan::Vulkan
    SDL3::SDL3
    glm::glm
    imgui::imgui
    unofficial::vulkan-memory-allocator::vulkan-memory-allocator
)

# Shader Compilation
find_program(GLSLANG_VALIDATOR glslangValidator HINTS $ENV{VULKAN_SDK}/Bin)

set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SHADER_SOURCES
    "${SHADER_SOURCE_DIR}/pbr.vert"
    "${SHADER_SOURCE_DIR}/pbr.frag"
    "${SHADER_SOURCE_DIR}/shadow.vert"
    "${SHADER_SOURCE_DIR}/shadow.frag"
)

set(SPIRV_OUTPUTS "")
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(FILE_NAME ${SHADER} NAME)
    set(SPIRV_OUT "${CMAKE_CURRENT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
    list(APPEND SPIRV_OUTPUTS ${SPIRV_OUT})

    if(GLSLANG_VALIDATOR)
        add_custom_command(
            OUTPUT ${SPIRV_OUT}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
            COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER} -o ${SPIRV_OUT}
            DEPENDS ${SHADER}
            COMMENT "Compiling shader ${FILE_NAME} to SPIR-V"
            VERBATIM
        )
    else()
        message(WARNING "glslangValidator not found. Shaders will not be auto-compiled by CMake.")
    endif()
endforeach()

if(GLSLANG_VALIDATOR)
    add_custom_target(compile_shaders ALL DEPENDS ${SPIRV_OUTPUTS})
    add_dependencies(VulkanPBR compile_shaders)
endif()

# Copy shaders directory next to executable if precompiled or for convenience
add_custom_command(TARGET VulkanPBR POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:VulkanPBR>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders" "$<TARGET_FILE_DIR:VulkanPBR>/shaders"
    COMMENT "Copying compiled shaders to target directory"
)
