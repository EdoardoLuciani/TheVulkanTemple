#ifdef _WIN64
	#define VK_USE_PLATFORM_WIN32_KHR
	#define GLFW_EXPOSE_NATIVE_WIN32
    #define _CRT_SECURE_NO_WARNINGS
#elif __linux__
	#define VK_USE_PLATFORM_XLIB_KHR
	#define GLFW_EXPOSE_NATIVE_X11
#else
	#error "Unknown compiler or not supported OS"
#endif

#define VOLK_IMPLEMENTATION
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <random>
#include <utility>
#include <array>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/integer.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <glm/gtx/euler_angles.hpp>

#define MAGIC_ENUM_RANGE_MIN 0
#define MAGIC_ENUM_RANGE_MAX 256
#include "magic_enum.hpp"
#include "graphics_module_vulkan_app.h"

int main() {
    std::vector<const char*> desired_instance_level_extensions = {"VK_KHR_surface"};
	#ifdef _WIN64
		desired_instance_level_extensions.push_back("VK_KHR_win32_surface");
	#elif __linux__
		desired_instance_level_extensions.push_back("VK_KHR_xlib_surface");
	#else
		#error "Unknown compiler or not supported OS"
	#endif

	std::vector<const char*> desired_device_level_extensions = {"VK_KHR_swapchain"};
	VkPhysicalDeviceFeatures selected_device_features = { 0 };

	EngineOptions options;
	options.anti_aliasing = 0;
	options.shadows = 0;
	options.HDR = 0;

	try {
		GraphicsModuleVulkanApp app("TheVulkanTemple", desired_instance_level_extensions, {800,800}, desired_device_level_extensions, selected_device_features, VK_TRUE, options);
		app.load_3d_objects({"resources/models/WaterBottle/WaterBottle.glb", "resources//models//Table//Table.glb"},64);
	}
	catch (std::pair<int32_t,Error>& err) {
		std::cout << "The application encounted the error: " << magic_enum::enum_name(err.second) << " with return value: " << err.first << std::endl;
	}
	return 0;
}