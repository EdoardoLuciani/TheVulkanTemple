#ifndef BASE_VULKAN_APP_H
#define BASE_VULKAN_APP_H

#ifdef _WIN64
	#define VK_USE_PLATFORM_WIN32_KHR
	#define GLFW_EXPOSE_NATIVE_WIN32
#elif __linux__
	#define VK_USE_PLATFORM_XLIB_KHR
	#define GLFW_EXPOSE_NATIVE_X11
#else
	#error "Unknown compiler or not supported OS"
#endif

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

#include "volk.h"
#include "vulkan_helper.h"

enum class Error {
	VOLK_INITIALIZATION_FAILED,
	INSTANCE_CREATION_FAILED,
	DEBUG_REPORT_CALLBACK_CREATION_FAILED,
	GLFW_INITIALIZATION_FAILED,
	GLFW_WINDOW_CREATION_FAILED,
	SURFACE_CREATION_FAILED,
	PHYSICAL_DEVICES_ENUMERATION_FAILED,
	NO_AVAILABLE_DEVICE_FOUND,
	DEVICE_CREATION_FAILED,
	PRESENT_MODES_RETRIEVAL_FAILED,
	SURFACE_CAPABILITIES_RETRIEVAL_FAILED,
	SURFACE_FORMATS_RETRIEVAL_FAILED,
	SWAPCHAIN_CREATION_FAILED,
	SWAPCHAIN_IMAGES_RETRIEVAL_FAILED,
	COMMAND_POOL_CREATION_FAILED,
	COMMAND_BUFFER_CREATION_FAILED,
	MEMORY_ALLOCATION_FAILED,
	SHADER_MODULE_CREATION_FAILED,
	ACQUIRE_NEXT_IMAGE_FAILED,
	QUEUE_PRESENT_FAILED
};

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
					  const std::vector<const char*> &desired_validation_layers,
					  const std::vector<const char*> &desired_instance_level_extensions,
					  VkExtent2D windows_size, 
					  const std::vector<const char*> &desired_device_level_extensions,
					  const VkPhysicalDeviceFeatures &required_physical_device_features,
					  VkBool32 surface_support);
		//setEngineFeatures();
		~BaseVulkanApp();


	private:
		// Vulkan attributes
		VkInstance instance = VK_NULL_HANDLE;
		#ifndef NDEBUG
			VkDebugReportCallbackEXT debug_report_callback;
		#endif
		VkExtent2D window_size;
		GLFWwindow* window;
		VkSurfaceKHR surface;
		VkPhysicalDevice selected_physical_device = VK_NULL_HANDLE;
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

		// Non-related Vulkan methods
		void check_error(int32_t last_return_value, Error error_location);
};

#endif