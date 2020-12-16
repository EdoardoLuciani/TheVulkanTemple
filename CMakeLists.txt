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
        src/volk.c src/vsm/vsm_context.cpp src/vsm/vsm_context.h)

target_link_libraries(base_vulkan_app glfw3 dl pthread stdc++fs X11)
