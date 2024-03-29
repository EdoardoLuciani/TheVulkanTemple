# TheVulkanTemple
TheVulkanTemple is a cross-platform vulkan rendering engine that focuses on simplicity and visual quality. By providing 3D PBR models in GLTF format and by setting lights it renders a scene.
TheVulkanTemple uses Variance Shadow Mapping for shadows, a Cook-Torrance BRDF for Physically Based Rendering, SMAA for antialiasing, HBAO for ambient occlusion and finally HDR tonemapping with optional upscaling. 
It supports fullscreen mode as well as windowed. Due to its modularity, it is quite easy to add features or to integrate it in larger projects.

![alt text](https://github.com/EdoardoLuciani/TheVulkanTemple/blob/master/docs/Screenshot_1.png)
![alt text](https://github.com/EdoardoLuciani/TheVulkanTemple/blob/master/docs/Screenshot_2.png)
![alt text](https://github.com/EdoardoLuciani/TheVulkanTemple/blob/master/docs/Screenshot_3.png)
![alt text](https://github.com/EdoardoLuciani/TheVulkanTemple/blob/master/docs/Screenshot_4.png)

## Features
- PBR Pipeline
- Shadows
- Point, spot and directional lights
- Horizon Based Ambient Occlusion
- Edge detection anti-aliasing
- High dynamic range with final tonemapping
- Image upscaling with FidelityFX-FSR  
- GLTF model as inputs
- Runtime mipmap generation
- Fullscreen, windowed and window resize

## Libraries used
- volk: to dynamically load entrypoints - https://github.com/zeux/volk
- GLFW: for window creation and related operations (glfwPollEvents()) - https://github.com/glfw/glfw
- OpenGL-Mathematics (GLM): for math related operations - https://github.com/g-truc/glm
- magic_enum: to print informing error messages - https://github.com/Neargye/magic_enum
- SMAA: header for the smaa library - https://github.com/iryoku/smaa
- GLSL-Color-Spaces: for color manipulation in hdr shader - https://github.com/tobspr/GLSL-Color-Spaces
- tiny_gltf: to open .gltf files - https://github.com/syoyo/tinygltf
- VMA: to better manage allocations - https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
- boost: used for multi_index_container
- FidelityFX-FSR: image upscaling - https://github.com/GPUOpen-Effects/FidelityFX-FSR

## Files
- main.cpp: shows how to use the rendering engine
- engine/graphics_module_vulkan_app : handles the high level functions of the engine, such as loading objects
- engine/base_vulkan_app : handles the device initialization

## How to build
Builds have been successfully tested on Windows and Linux. In order to build, Cmake is required and the following variables need to be set:
GLFW_INCLUDE_DIR,
GLFW_LIB_DIR,
GLM_INCLUDE_DIR,
VULKAN_SDK_INCLUDE_DIR,
BOOST_INCLUDE_DIR.

## Future plans
Occlusion culling on objects, multithreaded rendering.
