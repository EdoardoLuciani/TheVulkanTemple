#include "base_vulkan_app.h"

#include <iostream>
#include <vector>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <algorithm>
#include "vulkan_helper.h"

BaseVulkanApp::BaseVulkanApp(const std::string &application_name,
					  		 std::vector<const char*> &desired_instance_level_extensions,
					  		 VkExtent2D window_size,
					  		 bool fullscreen,
					  		 const std::vector<const char*> &desired_device_level_extensions,
					  		 const VkPhysicalDeviceFeatures &required_physical_device_features,
							 VkBool32 surface_support,
							 void* additional_structure) {
	
	// Dynamic library loading inizialization
	check_error(volkInitialize(), vulkan_helper::Error::VOLK_INITIALIZATION_FAILED);

	// Instance creation
	VkApplicationInfo application_info = {
		VK_STRUCTURE_TYPE_APPLICATION_INFO,
		nullptr,
		application_name.c_str(),
		VK_MAKE_VERSION(1,0,0),
		"TheVulkanTemple",
		VK_MAKE_VERSION(1,0,0),
		VK_MAKE_VERSION(1,1,0)
	};

    #ifdef NDEBUG
        std::vector<const char*> desired_validation_layers = {};
    #else
        std::vector<const char*> desired_validation_layers = { "VK_LAYER_KHRONOS_validation" };
        desired_instance_level_extensions.push_back("VK_EXT_debug_report");
        desired_instance_level_extensions.push_back("VK_EXT_debug_utils");
    #endif

	VkInstanceCreateInfo instance_create_info = {
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		nullptr,
		0,
		&application_info,
		static_cast<uint32_t>(desired_validation_layers.size()),
		desired_validation_layers.data(),
		static_cast<uint32_t>(desired_instance_level_extensions.size()),
		desired_instance_level_extensions.data()
	};

	check_error(vkCreateInstance(&instance_create_info, nullptr, &instance), vulkan_helper::Error::INSTANCE_CREATION_FAILED);
	volkLoadInstance(instance);

	// Debug Report (only in debug mode!)
	#ifndef NDEBUG
		VkDebugReportCallbackCreateInfoEXT debug_report_callback_create_info_EXT = {
			VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
			nullptr,
			VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT,
			vulkan_helper::debug_callback,
			nullptr
		};
		check_error(vkCreateDebugReportCallbackEXT(instance, &debug_report_callback_create_info_EXT, nullptr, &debug_report_callback), vulkan_helper::Error::DEBUG_REPORT_CALLBACK_CREATION_FAILED);
	#endif

	// Window Creation
	if (glfwInit() != GLFW_TRUE) { check_error(VK_ERROR_UNKNOWN, vulkan_helper::Error::GLFW_INITIALIZATION_FAILED); }

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	if (fullscreen) {
		window = glfwCreateWindow(window_size.width, window_size.height, application_name.c_str(), glfwGetPrimaryMonitor(), nullptr);
	}
	else {
		window = glfwCreateWindow(window_size.width, window_size.height, application_name.c_str(), nullptr, nullptr);
	}
	if (window == nullptr) { check_error(VK_ERROR_UNKNOWN, vulkan_helper::Error::GLFW_WINDOW_CREATION_FAILED); }

	// Surface Creation
	#ifdef _WIN64
		VkWin32SurfaceCreateInfoKHR surface_create_info = {
			VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
			nullptr,
			0,
			GetModuleHandle(NULL),
			glfwGetWin32Window(window)
		};
		check_error(vkCreateWin32SurfaceKHR(instance, &surface_create_info, nullptr, &surface), vulkan_helper::Error::SURFACE_CREATION_FAILED);
	#elif __linux__
		VkXlibSurfaceCreateInfoKHR surface_create_info = {
    	    VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
        	nullptr,
        	0,
        	glfwGetX11Display(),
    		glfwGetX11Window(window)
    	};
		check_error(vkCreateXlibSurfaceKHR(instance, &surface_create_info, nullptr, &surface), vulkan_helper::Error::SURFACE_CREATION_FAILED);
	#else
		#error "Unknown compiler or not supported OS"
	#endif

	// Device selection: we first iterate through all available devices and compare their features with the requested ones,
	// if there are more than 1 devices which can be selected then the decision is left to the user
	uint32_t devices_number;
	vkEnumeratePhysicalDevices(instance, &devices_number, nullptr);
	std::vector<VkPhysicalDevice> devices(devices_number);
	check_error(vkEnumeratePhysicalDevices(instance, &devices_number, devices.data()), vulkan_helper::Error::PHYSICAL_DEVICES_ENUMERATION_FAILED);

	std::vector<std::pair<size_t, size_t>> plausible_devices_d_index_qf_index;
	for (uint32_t i = 0; i < devices.size(); i++) {
	    VkPhysicalDeviceFeatures2 devices_features = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            vulkan_helper::create_device_feature_struct_chain(desired_device_level_extensions),
	    };
        vkGetPhysicalDeviceFeatures2(devices[i], &devices_features);

        VkBool32 requested_features_are_available = vulkan_helper::compare_device_features_struct(&devices_features.features, &required_physical_device_features, sizeof(VkPhysicalDeviceFeatures));
        requested_features_are_available &= vulkan_helper::compare_device_feature_struct_chain(devices_features.pNext, additional_structure);
        vulkan_helper::free_device_feature_struct_chain(devices_features.pNext);

		if (requested_features_are_available) {
			uint32_t families_count;
			vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &families_count, nullptr);

			VkBool32 does_queue_family_support_surface = VK_FALSE;
			for (uint32_t y = 0; y < families_count; y++) {
				vkGetPhysicalDeviceSurfaceSupportKHR(devices[i], y, surface, &does_queue_family_support_surface);
				if (surface_support == does_queue_family_support_surface) {
					plausible_devices_d_index_qf_index.push_back(std::make_pair(i,y));
					break;
				}
			}
		}	
	}

	selected_physical_device = VK_NULL_HANDLE;
	uint32_t selected_queue_family_index = -1;
	if (plausible_devices_d_index_qf_index.size() == 1) {
		selected_physical_device = devices[plausible_devices_d_index_qf_index[0].first];
		selected_queue_family_index = plausible_devices_d_index_qf_index[0].second;
	}
	else if (plausible_devices_d_index_qf_index.size() > 1) {
		std::cout << "Found multiple GPUs available, input index of desired device" << std::endl;
		for (size_t i = 0; i < plausible_devices_d_index_qf_index.size(); i++) {
			VkPhysicalDeviceProperties devices_properties;
			vkGetPhysicalDeviceProperties(devices[plausible_devices_d_index_qf_index[i].first], &devices_properties);
			std::cout << i << ":Device: " << devices_properties.deviceName << std::endl;
		}
		size_t index;
		std::cin >> index;
		selected_physical_device = devices[plausible_devices_d_index_qf_index[index].first];
		selected_queue_family_index = plausible_devices_d_index_qf_index[index].second;
	}
	else {
		check_error(VK_ERROR_UNKNOWN, vulkan_helper::Error::NO_AVAILABLE_DEVICE_FOUND);
	}

	// Getting the properties of the selected physical device
    vkGetPhysicalDeviceMemoryProperties(selected_physical_device, &physical_device_memory_properties);
    vkGetPhysicalDeviceProperties(selected_physical_device, &physical_device_properties);

	// Creating the device with only one queue and the requested device level extensions and features
	std::vector<float> queue_priorities = { 1.0f };
	std::vector<VkDeviceQueueCreateInfo> queue_create_info;
	queue_create_info.push_back({
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		nullptr,
		0,
		static_cast<uint32_t>(selected_queue_family_index),
		static_cast<uint32_t>(queue_priorities.size()),
		queue_priorities.data()
		});

	VkDeviceCreateInfo device_create_info = {
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		additional_structure,
		0,
		static_cast<uint32_t>(queue_create_info.size()),
		queue_create_info.data(),
		0,
		nullptr,
		static_cast<uint32_t>(desired_device_level_extensions.size()),
		desired_device_level_extensions.data(),
		&required_physical_device_features
	};

	check_error(vkCreateDevice(selected_physical_device, &device_create_info, nullptr, &device), vulkan_helper::Error::DEVICE_CREATION_FAILED);
	vkGetDeviceQueue(device, selected_queue_family_index, 0, &queue);
	volkLoadDevice(device);

	create_swapchain();
    create_cmd_pool_and_buffers(selected_queue_family_index);
}

BaseVulkanApp::~BaseVulkanApp() {
    vkDeviceWaitIdle(device);
    vkFreeCommandBuffers(device, command_pool, command_buffers.size(), command_buffers.data());
    vkDestroyCommandPool(device, command_pool, nullptr);
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    glfwDestroyWindow(window);
#ifndef NDEBUG
    vkDestroyDebugReportCallbackEXT(instance, debug_report_callback, nullptr);
#endif
    vkDestroyInstance(instance, nullptr);
}

GLFWwindow* BaseVulkanApp::get_glfw_window() { return window; }

void BaseVulkanApp::create_swapchain() {
	// Listing all available presentation
	uint32_t presentation_modes_number;
	vkGetPhysicalDeviceSurfacePresentModesKHR(selected_physical_device, surface, &presentation_modes_number, nullptr);
	std::vector<VkPresentModeKHR> presentation_modes(presentation_modes_number);
	check_error(vkGetPhysicalDeviceSurfacePresentModesKHR(selected_physical_device, surface, &presentation_modes_number, presentation_modes.data()),
				vulkan_helper::Error::PRESENT_MODES_RETRIEVAL_FAILED);

	VkPresentModeKHR selected_present_mode = vulkan_helper::select_presentation_mode(presentation_modes, VK_PRESENT_MODE_MAILBOX_KHR);

	VkSurfaceCapabilitiesKHR surface_capabilities;
	check_error(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(selected_physical_device, surface, &surface_capabilities),
				vulkan_helper::Error::SURFACE_CAPABILITIES_RETRIEVAL_FAILED);

	uint32_t number_of_images = vulkan_helper::select_number_of_images(surface_capabilities);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
	VkExtent2D size_of_images = vulkan_helper::select_size_of_images(surface_capabilities, { static_cast<uint32_t>(width),static_cast<uint32_t>(height) });

	VkImageUsageFlags image_usage = vulkan_helper::select_image_usage(surface_capabilities, VK_IMAGE_USAGE_STORAGE_BIT);
	VkSurfaceTransformFlagBitsKHR surface_transform = vulkan_helper::select_surface_transform(surface_capabilities, VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR);

	uint32_t formats_count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(selected_physical_device, surface, &formats_count, nullptr);
	std::vector<VkSurfaceFormatKHR> surface_formats(formats_count);
	check_error(vkGetPhysicalDeviceSurfaceFormatsKHR(selected_physical_device, surface, &formats_count, surface_formats.data()),
				vulkan_helper::Error::SURFACE_FORMATS_RETRIEVAL_FAILED);
	VkSurfaceFormatKHR surface_format = vulkan_helper::select_surface_format(surface_formats, { VK_FORMAT_B8G8R8A8_UNORM,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });

	VkSwapchainKHR old_swapchain = swapchain;
	swapchain_create_info = {
		VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		nullptr,
		0,
		surface,
		number_of_images,
		surface_format.format,
		surface_format.colorSpace,
		size_of_images,
		1,
		image_usage,
		VK_SHARING_MODE_EXCLUSIVE,
		0,
		nullptr,
		surface_transform,
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		selected_present_mode,
		VK_TRUE,
		old_swapchain
	};
	check_error(vkCreateSwapchainKHR(device, &swapchain_create_info, nullptr, &swapchain), vulkan_helper::Error::SWAPCHAIN_CREATION_FAILED);

    uint32_t swapchain_images_count;
	vkGetSwapchainImagesKHR(device, swapchain, &swapchain_images_count, nullptr);
	swapchain_images.resize(swapchain_images_count);
	check_error(vkGetSwapchainImagesKHR(device, swapchain, &swapchain_images_count, swapchain_images.data()), vulkan_helper::Error::SWAPCHAIN_IMAGES_RETRIEVAL_FAILED);
}

void BaseVulkanApp::create_cmd_pool_and_buffers(uint32_t queue_family_index) {
    VkCommandPoolCreateInfo command_pool_create_info = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        nullptr,
        0,
        queue_family_index
    };
    check_error(vkCreateCommandPool(device, &command_pool_create_info, nullptr, &command_pool), vulkan_helper::Error::COMMAND_POOL_CREATION_FAILED);

    VkCommandBufferAllocateInfo command_buffer_allocate_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        nullptr,
        command_pool,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        static_cast<uint32_t>(swapchain_images.size())
    };
    command_buffers.resize(swapchain_images.size());
    check_error(vkAllocateCommandBuffers(device, &command_buffer_allocate_info, command_buffers.data()), vulkan_helper::Error::COMMAND_BUFFER_CREATION_FAILED);
}



