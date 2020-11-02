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

		if (compare_physical_device_features_structs(devices_features, required_physical_device_features)) {
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

	VkPhysicalDevice selected_physical_device = VK_NULL_HANDLE;
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

	// Swapchain creation

}


BaseVulkanApp::~BaseVulkanApp() {

}


bool BaseVulkanApp::compare_physical_device_features_structs(VkPhysicalDeviceFeatures base, VkPhysicalDeviceFeatures requested) {
	constexpr uint32_t feature_count = static_cast<uint32_t>(sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32));

	std::array<VkBool32, feature_count> base_bools_arr;
	std::memcpy(base_bools_arr.data(), &base, base_bools_arr.size()*sizeof(VkBool32));

	std::array<VkBool32, feature_count> requested_bools_arr;
	std::memcpy(requested_bools_arr.data(), &requested, requested_bools_arr.size()*sizeof(VkBool32));

	for(size_t i = 0; i < base_bools_arr.size(); i++) {
		if (base_bools_arr[i] == VK_FALSE && requested_bools_arr[i] == VK_TRUE) {
			return VK_FALSE;
		}
	}
	return VK_TRUE;
}

void BaseVulkanApp::check_error(int32_t last_return_value, Error error_location) {
	if (last_return_value) {
		throw std::make_pair(last_return_value, error_location);
	}
}

