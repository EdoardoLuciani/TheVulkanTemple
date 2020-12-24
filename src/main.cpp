#include <iostream>
#include <utility>
#include <vector>
#include "vulkan_helper.h"
#include "graphics_module_vulkan_app.h"

#define MAGIC_ENUM_RANGE_MIN 0
#define MAGIC_ENUM_RANGE_MAX 256
#include "magic_enum.hpp"

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
	selected_device_features.samplerAnisotropy = VK_TRUE;

	EngineOptions options;
	options.anti_aliasing = 0;
	options.shadows = 0;
	options.HDR = 0;

	try {
		GraphicsModuleVulkanApp app("TheVulkanTemple", desired_instance_level_extensions, {800,800}, desired_device_level_extensions, selected_device_features, VK_TRUE, options);
		app.load_3d_objects({"resources/models/WaterBottle/WaterBottle.glb", "resources//models//Table//Table.glb"},64);
		app.load_lights({
		    {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, 90.0f, 1.0f},
            {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, 90.0f, 1.0f}
		});
		app.set_camera({{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, 100.0f, 1.0f});
        app.init_renderer();
	}
	catch (std::pair<int32_t,vulkan_helper::Error>& err) {
		std::cout << "The application encounted the error: " << magic_enum::enum_name(err.second) << " with return value: " << err.first << std::endl;
	}
	return 0;
}