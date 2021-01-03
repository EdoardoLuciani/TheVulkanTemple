#ifndef BASE_VULKAN_APP_H
#define BASE_VULKAN_APP_H

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

#include <string>
#include <vector>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "volk.h"

enum class Projection {
	ORTHOGONAL,
	PERSPECTIVE
};

struct EngineFeatures {
	VkPresentModeKHR present_mode;
	float fov;
	Projection projection_mode;
	float anisotropic_filtering_level;
};

class BaseVulkanApp {
	public:
		BaseVulkanApp(const std::string &application_name,
					  std::vector<const char*> &desired_instance_level_extensions,
					  VkExtent2D window_size,
					  const std::vector<const char*> &desired_device_level_extensions,
					  const VkPhysicalDeviceFeatures &required_physical_device_features,
					  VkBool32 surface_support,
					  void* additional_structure);
		virtual ~BaseVulkanApp();

	protected:
		// Vulkan attributes
		VkInstance instance = VK_NULL_HANDLE;
		#ifndef NDEBUG
			VkDebugReportCallbackEXT debug_report_callback;
		#endif
		VkExtent2D window_size;
		GLFWwindow* window;
		VkSurfaceKHR surface;
		VkPhysicalDevice selected_physical_device = VK_NULL_HANDLE;
		VkPhysicalDeviceMemoryProperties physical_device_memory_properties;
        VkPhysicalDeviceProperties physical_device_properties;
		VkDevice device = VK_NULL_HANDLE;
		VkQueue queue;

		VkSwapchainCreateInfoKHR swapchain_create_info;
		VkSwapchainKHR swapchain = VK_NULL_HANDLE;
		std::vector<VkImage> swapchain_images;

		VkCommandPool command_pool;
		std::vector<VkCommandBuffer> command_buffers;

		// Vulkan related private methods
		void create_swapchain();
		void create_cmd_pool_and_buffers(uint32_t queue_family_index);
};

#endif