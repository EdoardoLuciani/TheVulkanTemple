#ifndef BASE_VULKAN_APP_H
#define BASE_VULKAN_APP_H

#include <string>
#include <vector>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "volk.h"

class BaseVulkanApp {
	public:
		BaseVulkanApp(const std::string &application_name,
					  std::vector<const char*> desired_instance_level_extensions,
					  VkExtent2D window_size,
					  bool fullscreen,
					  const std::vector<const char*> desired_device_level_extensions,
					  const VkPhysicalDeviceFeatures2 *desired_physical_device_features2,
					  VkBool32 surface_support);
		virtual ~BaseVulkanApp();
		GLFWwindow* get_glfw_window();

	protected:
		// Vulkan attributes
		VkInstance instance = VK_NULL_HANDLE;
		#ifndef NDEBUG
			VkDebugReportCallbackEXT debug_report_callback;
		#endif
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