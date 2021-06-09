cmake_minimum_required(VERSION 3.0)
project(TheVulkanTemple)

# Adding compile definitions and options
add_library(compiler_flags INTERFACE)
target_compile_features(compiler_flags INTERFACE cxx_std_17)
if (WIN32)
    target_compile_definitions(compiler_flags INTERFACE "VK_USE_PLATFORM_WIN32_KHR")
    target_compile_definitions(compiler_flags INTERFACE "GLFW_EXPOSE_NATIVE_WIN32")
    target_compile_definitions(compiler_flags INTERFACE "_CRT_SECURE_NO_WARNINGS")
elseif(UNIX)
    target_compile_definitions(compiler_flags INTERFACE "VK_USE_PLATFORM_XLIB_KHR")
    target_compile_definitions(compiler_flags INTERFACE "GLFW_EXPOSE_NATIVE_X11")
endif()
target_compile_definitions(compiler_flags INTERFACE "GLM_FORCE_DEPTH_ZERO_TO_ONE")
target_compile_definitions(compiler_flags INTERFACE "GLM_FORCE_LEFT_HANDED")
target_compile_definitions(compiler_flags INTERFACE "GLM_FORCE_SSE4")

if(MSVC)
    target_compile_options(compiler_flags INTERFACE /arch:SSE4.1)
else()
    target_compile_options(compiler_flags INTERFACE -msse4.1)
    target_compile_options(compiler_flags INTERFACE -Wall)
endif()
# End compile definitions and options

# define directories for source code location
set(SRC_DIR "${PROJECT_SOURCE_DIR}/src")
set(ENGINE_SRC_DIR "${SRC_DIR}/TheVulkanTemple")

# define location for external libraries
set(GLFW_INCLUDE_DIR "" CACHE PATH "Include directories of GLFW")
set(GLFW_LIB_DIR "" CACHE PATH "Lib dir of GLFW")

set(GLM_INCLUDE_DIR "" CACHE PATH "Include dir for GLM")

set(VULKAN_SDK_INCLUDE_DIR "$ENV{VULKAN_SDK}/include" CACHE PATH "Include dir for the vulkan SDK")

set(TINY_GLTF_INCLUDE_DIR "" CACHE PATH "Include dir for tiny_gltf")

# Library for TheVulkanTemple engine
add_library(engine OBJECT
        ${ENGINE_SRC_DIR}/base_vulkan_app.cpp
        ${ENGINE_SRC_DIR}/base_vulkan_app.h
        ${ENGINE_SRC_DIR}/vulkan_helper.cpp
        ${ENGINE_SRC_DIR}/vulkan_helper.h
        ${ENGINE_SRC_DIR}/graphics_module_vulkan_app.cpp
        ${ENGINE_SRC_DIR}/graphics_module_vulkan_app.h
        ${ENGINE_SRC_DIR}/smaa/smaa_context.cpp
        ${ENGINE_SRC_DIR}/smaa/smaa_context.h
        ${ENGINE_SRC_DIR}/volk.c
        ${ENGINE_SRC_DIR}/volk.h
        ${ENGINE_SRC_DIR}/vsm/vsm_context.cpp
        ${ENGINE_SRC_DIR}/vsm/vsm_context.h
        ${ENGINE_SRC_DIR}/camera.cpp
        ${ENGINE_SRC_DIR}/camera.h
        ${ENGINE_SRC_DIR}/light.cpp
        ${ENGINE_SRC_DIR}/light.h
        ${ENGINE_SRC_DIR}/hdr_tonemap/hdr_tonemap_context.cpp
        ${ENGINE_SRC_DIR}/hdr_tonemap/hdr_tonemap_context.h
        ${ENGINE_SRC_DIR}/gltf_model.cpp
        ${ENGINE_SRC_DIR}/gltf_model.h
        ${ENGINE_SRC_DIR}/hbao/hbao_context.cpp
        ${ENGINE_SRC_DIR}/hbao/hbao_context.h)

target_include_directories(engine PUBLIC ${GLFW_INCLUDE_DIR})
target_link_directories(engine PUBLIC ${GLFW_LIB_DIR})

target_include_directories(engine PUBLIC ${GLM_INCLUDE_DIR})

target_include_directories(engine PUBLIC ${VULKAN_SDK_INCLUDE_DIR})

target_include_directories(engine PUBLIC ${TINY_GLTF_INCLUDE_DIR})

if (WIN32)
    target_link_libraries(engine compiler_flags glfw3)
elseif(UNIX)
    target_link_libraries(engine compiler_flags glfw3 dl pthread)
endif()

# Library for the sample
add_executable(sample
        ${SRC_DIR}/magic_enum.hpp
        ${SRC_DIR}/main.cpp)

target_link_libraries(sample compiler_flags engine)

add_custom_command(TARGET sample POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/resources"  "$<TARGET_FILE_DIR:sample>/resources"
        )

add_custom_command(TARGET sample POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:sample>/resources/shaders/"
        )

# SPIR-V Shader compilation
if (WIN32)
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslc.exe")
elseif(UNIX)
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/bin/glslc")
endif()

file(GLOB_RECURSE GLSL_SOURCE_FILES
        "${ENGINE_SRC_DIR}/shaders/*/*.comp"
        "${ENGINE_SRC_DIR}/shaders/*/*.frag"
        "${ENGINE_SRC_DIR}/shaders/*/*.vert"
        )

foreach(GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_SOURCE_DIR}/resources/shaders/${FILE_NAME}.spv")
    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${GLSL_VALIDATOR} -c -o ${SPIRV} ${GLSL}
            DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
        shaders DEPENDS ${SPIRV_BINARY_FILES}
)

add_dependencies(sample shaders)


