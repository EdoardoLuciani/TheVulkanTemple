# TheVulkanTemple
TheVulkanTemple is a cross-platform vulkan rendering engine that focuses on simplicity and visual quality. By providing 3D PBR models in GLTF format and by setting lights it renders a scene.
TheVulkanTemple uses Variance Shadow Mapping for shadows, a Cook-Torrance BRDF for Physically Based Rendering, SMAA for antialiasing and finally HDR tonemapping.
It supports fullscreen mode as well as windowed. Due to its modularity, it is quite easy to add features or to integrate it in larger projects.

![alt text](https://github.com/EdoardoLuciani/TheVulkanTemple/blob/refactor/docs/Screenshot.png)

## Features
- PBR Pipeline
- Shadows
- Point lights
- Edge detection anti-aliasing
- High dynamic range with final tonemapping
- GLTF model as inputs
- Runtime mipmap generation
- Fullscreen, windowed and resizing of window

## External libraries
- volk: to dynamically load entrypoints - https://github.com/zeux/volk
- GLFW: for window creation and related operations (glfwPollEvents()) - https://github.com/glfw/glfw
- OpenGL-Mathematics (GLM): for math related operations - https://github.com/g-truc/glm
- magic_enum: to print informing error messages - https://github.com/Neargye/magic_enum
- SMAA: header for the smaa library - https://github.com/iryoku/smaa
- GLSL-Color-Spaces: for color manipulation in hdr shader - https://github.com/tobspr/GLSL-Color-Spaces

## Files
- main.cpp: shows how to use rendering engines
- graphics_module_vulkan_app: core file for the engine that handles parameter setting
- base_vulkan_app: used to do the initialization of any vulkan application
- vulkan_helper: various helper functions used in the engine
- fps_camera_control: provides a FPS-style camera used to navigate in the scene

## Future plans
By using Fidelity-FX effects, to implement SSSR and CACAO.
