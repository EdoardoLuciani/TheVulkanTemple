#include "base_vulkan_app.h"

BaseVulkanApp::BaseVulkanApp(const std::string &application_name,
					  		 const std::vector<const char*> &desired_validation_layers,
					  		 const std::vector<const char*> &desired_instance_level_extensions,
					  		 VkExtent2D window_size,
					  		 const std::vector<const char*> &desired_device_level_extensions,
					  		 const VkPhysicalDeviceFeatures &required_physical_device_features,
							 VkBool32 surface_support) {
	
	// Dynamic library loading inizialization
	check_error(volkInitialize(), Error::VOLK_INITIALIZATION_FAILED);

	// Instance creation
	VkApplicationInfo application_info = {
		VK_STRUCTURE_TYPE_APPLICATION_INFO,
		nullptr,
		application_name.c_str(),
		VK_MAKE_VERSION(1,0,0),
		"Pure Vulkan",
		VK_MAKE_VERSION(1,0,0),
		VK_MAKE_VERSION(1,0,0)
	};

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

	check_error(vkCreateInstance(&instance_create_info, nullptr, &instance), Error::INSTANCE_CREATION_FAILED);
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
		check_error(vkCreateDebugReportCallbackEXT(instance, &debug_report_callback_create_info_EXT, nullptr, &debug_report_callback), Error::DEBUG_REPORT_CALLBACK_CREATION_FAILED);
	#endif

	// Window Creation
	if (glfwInit() != GLFW_TRUE) { check_error(VK_ERROR_UNKNOWN, Error::GLFW_INITIALIZATION_FAILED); }

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	if ((window_size.width == 0) && (window_size.height == 0)) {
		const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
		this->window_size.width = mode->width;
		this->window_size.height = mode->height;
		window = glfwCreateWindow(this->window_size.width, this->window_size.height, application_name.c_str(), glfwGetPrimaryMonitor(), nullptr);
	}
	else {
		this->window_size.width = window_size.width;
		this->window_size.height = window_size.height;
		window = glfwCreateWindow(this->window_size.width, this->window_size.height, application_name.c_str(), nullptr, nullptr);
	}
	if (window == NULL) { check_error(VK_ERROR_UNKNOWN, Error::GLFW_WINDOW_CREATION_FAILED); }

	// Surface Creation
	#ifdef _WIN64
		VkWin32SurfaceCreateInfoKHR surface_create_info = {
			VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
			nullptr,
			0,
			GetModuleHandle(NULL),
			glfwGetWin32Window(window)
		};
		check_error(vkCreateWin32SurfaceKHR(instance, &surface_create_info, nullptr, &surface) != VK_SUCCESS), Error::SURFACE_CREATION_FAILED);
	#elif __linux__
		VkXlibSurfaceCreateInfoKHR surface_create_info = {
    	    VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
        	nullptr,
        	0,
        	glfwGetX11Display(),
    		glfwGetX11Window(window)
    	};
		check_error(vkCreateXlibSurfaceKHR(instance, &surface_create_info, nullptr, &surface), Error::SURFACE_CREATION_FAILED);
	#else
		#error "Unknown compiler or not supported OS"
	#endif

	// Device selection: we first iterate through all available devices and compare their features with the requested ones,
	// if there are more than 1 devices which can be selected then the decision is left to the user
	uint32_t devices_number;
	vkEnumeratePhysicalDevices(instance, &devices_number, nullptr);
	std::vector<VkPhysicalDevice> devices(devices_number);
	check_error(vkEnumeratePhysicalDevices(instance, &devices_number, devices.data()), Error::PHYSICAL_DEVICES_ENUMERATION_FAILED);

	std::vector<std::pair<size_t, size_t>> plausible_devices_d_index_qf_index;
	for (uint32_t i = 0; i < devices.size(); i++) {
		VkPhysicalDeviceFeatures devices_features;
		vkGetPhysicalDeviceFeatures(devices[i], &devices_features);

		if (vulkan_helper::compare_physical_device_features_structs(devices_features, required_physical_device_features)) {
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
		check_error(VK_ERROR_UNKNOWN, Error::NO_AVAILABLE_DEVICE_FOUND);
	}

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
		nullptr,
		0,
		static_cast<uint32_t>(queue_create_info.size()),
		queue_create_info.data(),
		0,
		nullptr,
		static_cast<uint32_t>(desired_device_level_extensions.size()),
		desired_device_level_extensions.data(),
		&required_physical_device_features
	};

	check_error(vkCreateDevice(selected_physical_device, &device_create_info, nullptr, &device), Error::DEVICE_CREATION_FAILED);
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

void BaseVulkanApp::create_swapchain() {
	// Listing all available presentation
	uint32_t presentation_modes_number;
	vkGetPhysicalDeviceSurfacePresentModesKHR(selected_physical_device, surface, &presentation_modes_number, nullptr);
	std::vector<VkPresentModeKHR> presentation_modes(presentation_modes_number);
	check_error(vkGetPhysicalDeviceSurfacePresentModesKHR(selected_physical_device, surface, &presentation_modes_number, presentation_modes.data()),
				Error::PRESENT_MODES_RETRIEVAL_FAILED);

	VkPresentModeKHR selected_present_mode = vulkan_helper::select_presentation_mode(presentation_modes, VK_PRESENT_MODE_MAILBOX_KHR);

	VkSurfaceCapabilitiesKHR surface_capabilities;
	check_error(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(selected_physical_device, surface, &surface_capabilities),
				Error::SURFACE_CAPABILITIES_RETRIEVAL_FAILED);

	uint32_t number_of_images = vulkan_helper::select_number_of_images(surface_capabilities);
	VkExtent2D size_of_images = vulkan_helper::select_size_of_images(surface_capabilities, window_size);
	VkImageUsageFlags image_usage = vulkan_helper::select_image_usage(surface_capabilities, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
	VkSurfaceTransformFlagBitsKHR surface_transform = vulkan_helper::select_surface_transform(surface_capabilities, VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR);

	uint32_t formats_count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(selected_physical_device, surface, &formats_count, nullptr);
	std::vector<VkSurfaceFormatKHR> surface_formats(formats_count);
	check_error(vkGetPhysicalDeviceSurfaceFormatsKHR(selected_physical_device, surface, &formats_count, surface_formats.data()),
				Error::SURFACE_FORMATS_RETRIEVAL_FAILED);
	VkSurfaceFormatKHR surface_format = vulkan_helper::select_surface_format(surface_formats, { VK_FORMAT_B8G8R8A8_UNORM ,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });

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
	check_error(vkCreateSwapchainKHR(device, &swapchain_create_info, nullptr, &swapchain), Error::SWAPCHAIN_CREATION_FAILED);

    uint32_t swapchain_images_count;
	vkGetSwapchainImagesKHR(device, swapchain, &swapchain_images_count, nullptr);
	swapchain_images.resize(swapchain_images_count);
	check_error(vkGetSwapchainImagesKHR(device, swapchain, &swapchain_images_count, swapchain_images.data()), Error::SWAPCHAIN_IMAGES_RETRIEVAL_FAILED);
}

void BaseVulkanApp::create_cmd_pool_and_buffers(uint32_t queue_family_index) {
    VkCommandPoolCreateInfo command_pool_create_info = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        nullptr,
        0,
        queue_family_index
    };
    check_error(vkCreateCommandPool(device, &command_pool_create_info, nullptr, &command_pool), Error::COMMAND_POOL_CREATION_FAILED);

    VkCommandBufferAllocateInfo command_buffer_allocate_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        nullptr,
        command_pool,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        static_cast<uint32_t>(swapchain_images.size())
    };
    command_buffers.resize(swapchain_images.size());
    check_error(vkAllocateCommandBuffers(device, &command_buffer_allocate_info, command_buffers.data()), Error::COMMAND_BUFFER_CREATION_FAILED);
}

void BaseVulkanApp::check_error(int32_t last_return_value, Error error_location) {
	if (last_return_value) {
		throw std::make_pair(last_return_value, error_location);
	}
}



