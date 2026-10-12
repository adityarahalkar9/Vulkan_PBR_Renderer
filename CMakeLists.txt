cmake_minimum_required(VERSION 3.25)
project(VulkanPBR LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Put the exe at build/bin/VulkanPBR.exe (predictable path)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

# ── Dependencies ──
find_package(Vulkan REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)

find_path(STB_INCLUDE_DIR NAMES stb_image.h)
if(NOT STB_INCLUDE_DIR)
    message(FATAL_ERROR "stb_image.h not found!")
endif()

find_path(TINYGLTF_INCLUDE_DIR NAMES tiny_gltf.h)
if(NOT TINYGLTF_INCLUDE_DIR)
    message(STATUS "tinygltf not found - OK")
    set(TINYGLTF_INCLUDE_DIR "")
endif()

# ── Sources ──
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

add_executable(VulkanPBR ${SOURCES})

target_include_directories(VulkanPBR PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${STB_INCLUDE_DIR}"
    "${TINYGLTF_INCLUDE_DIR}"
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(VulkanPBR PRIVATE
    Vulkan::Vulkan
    SDL3::SDL3
    glm::glm
    imgui::imgui
    GPUOpen::VulkanMemoryAllocator
)

# ── Shader Compilation ──
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
    set(SPIRV_OUT "${CMAKE_BINARY_DIR}/shaders/${FILE_NAME}.spv")
    list(APPEND SPIRV_OUTPUTS ${SPIRV_OUT})
    if(GLSLANG_VALIDATOR)
        add_custom_command(
            OUTPUT ${SPIRV_OUT}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/shaders"
            COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER} -o ${SPIRV_OUT}
            DEPENDS ${SHADER}
            COMMENT "Compiling ${FILE_NAME}"
            VERBATIM
        )
    endif()
endforeach()

if(GLSLANG_VALIDATOR)
    add_custom_target(compile_shaders ALL DEPENDS ${SPIRV_OUTPUTS})
    add_dependencies(VulkanPBR compile_shaders)
else()
    message(WARNING "glslangValidator not found - shaders will NOT compile!")
endif()

# Copy shaders next to exe
add_custom_command(TARGET VulkanPBR POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:VulkanPBR>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_BINARY_DIR}/shaders"
        "$<TARGET_FILE_DIR:VulkanPBR>/shaders"
    COMMENT "Copying shaders to exe directory"
)