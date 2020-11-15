cmake_minimum_required(VERSION 3.0)
project(base_vulkan_app)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_VERBOSE_MAKEFILE ON)

include_directories(src)
include_directories(src/shaders)

add_executable(base_vulkan_app
        src/base_vulkan_app.cpp
        src/main.cpp
        src/vulkan_helper.cpp)

target_link_libraries(base_vulkan_app glfw3 dl pthread stdc++fs X11)
