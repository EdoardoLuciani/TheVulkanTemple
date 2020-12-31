cmake_minimum_required(VERSION 3.0)
project(base_vulkan_app)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_VERBOSE_MAKEFILE ON)

if (WIN32)
    add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
elseif(UNIX)
    add_definitions(-DVK_USE_PLATFORM_XLIB_KHR)
endif()

add_executable(base_vulkan_app
        src/base_vulkan_app.cpp
        src/main.cpp
        src/vulkan_helper.cpp
        src/graphics_module_vulkan_app.cpp
        src/smaa/smaa_context.cpp
        src/volk.c
        src/vsm/vsm_context.cpp
        src/camera.cpp
        src/light.cpp)

target_link_libraries(base_vulkan_app glfw3 dl pthread stdc++fs X11)

# SPIR-V Shader compilation
if (WIN32)
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslc.exe")
elseif(UNIX)
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/bin/glslc")
endif()

file(GLOB_RECURSE GLSL_SOURCE_FILES
        "src/shaders/HDR/*.frag"
        "src/shaders/HDR/*.vert"
        "src/shaders/light/*.frag"
        "src/shaders/light/*.vert"
        "src/shaders/pbr/*.frag"
        "src/shaders/pbr/*.vert"
        "src/shaders/smaa/*.frag"
        "src/shaders/smaa/*.vert"
        "src/shaders/vsm/*.frag"
        "src/shaders/vsm/*.vert"
        "src/shaders/vsm/*.comp"
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

add_dependencies(base_vulkan_app shaders)

add_custom_command(TARGET base_vulkan_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "resources/shaders/"
        )
