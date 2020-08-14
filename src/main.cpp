#ifdef _WIN64
	#define VK_USE_PLATFORM_WIN32_KHR
	#define GLFW_EXPOSE_NATIVE_WIN32
#elif __linux__
	#define VK_USE_PLATFORM_XLIB_KHR
	#define GLFW_EXPOSE_NATIVE_X11
#else
	#error "Unknown compiler or not supported OS"
#endif

#define VOLK_IMPLEMENTATION
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define _CRT_SECURE_NO_WARNINGS

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

class VulkanSSAO {
private:
	void create_instance();
	void setup_debug_callback();
	void create_window();
	void create_surface();
	void create_logical_device();
	void create_swapchain();
	void calculate_projection_matrix();
	void create_command_pool();
	void allocate_command_buffers();
	void create_host_buffers();
	void create_device_buffers();
	void create_attachments();
	void create_descriptor_pool_and_layouts();
	void allocate_gaussian_blur_descriptor_sets();
	void allocate_pbr_descriptor_sets();
	void allocate_camera_light_sets();
	void allocate_smaa_descriptor_sets();
	void allocate_hdr_descriptor_sets();
	void create_renderpass();
	void create_framebuffers();
	void create_shadow_map_pipeline();
	void create_gaussian_blur_pipelines();
	void create_light_pipeline();
	void create_pbr_pipeline();
	void create_smaa_edge_pipeline();
	void create_smaa_weight_pipeline();
	void create_smaa_blend_pipeline();
	void create_hdr_tonemap_pipeline();
	void upload_input_data();
	void record_command_buffers();
	void create_semaphores();

	void frame_loop();

	static void key_callback_static(GLFWwindow* window, int key, int scancode, int action, int mods);
	void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void cursor_position_callback_static(GLFWwindow* window, double xpos, double ypos);
	void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);

	void on_window_resize();

	VkInstance instance;
	VkDebugReportCallbackEXT debug_report_callback;

	VkExtent2D window_size = { 1777,1000 };
	GLFWwindow* window;
	VkSurfaceKHR surface;

	VkPhysicalDevice physical_device;
	VkPhysicalDeviceMemoryProperties physical_device_memory_properties;
	uint32_t queue_family_index;
	VkDevice device;
	VkQueue queue;

	VkSwapchainKHR old_swapchain = VK_NULL_HANDLE;
	VkSwapchainCreateInfoKHR swapchain_create_info;
	VkSwapchainKHR swapchain;
	uint32_t swapchain_images_count;
	std::vector<VkImage> swapchain_images;

	VkCommandPool command_pool;
	std::vector<VkCommandBuffer> command_buffers;

	VkBuffer host_vertex_buffer, host_uniform_buffer;
	std::array<VkMemoryRequirements, 2> host_memory_requirements;
	VkDeviceMemory host_memory;
	void* host_data_pointer;
	vulkan_helper::model_data_info water_bottle_model_data, box_model_data, table_model_data, sphere_model_data;
	int area_tex_size, search_tex_size;

	VkBuffer device_vertex_buffer, device_uniform_buffer;
	VkImage device_water_bottle_image, device_box_image, device_table_image, device_smaa_area_image, device_smaa_search_image;
	VkImageView device_water_bottle_color_image_view, device_water_bottle_orm_image_view,
		device_water_bottle_normal_image_view, device_water_bottle_emissive_image_view, device_box_normal_image_view,
		device_box_color_image_view, device_box_orm_image_view, device_box_emissive_image_view,
		device_table_color_image_view, device_table_orm_image_view, device_table_normal_image_view,
		device_table_emissive_image_view, device_smaa_area_image_view, device_smaa_search_image_view;
	VkSampler device_max_aniso_linear_sampler;
	std::array<VkMemoryRequirements, 7> device_memory_requirements;
	VkDeviceMemory device_memory;

	VkImage device_depth_image, device_stencil_image, device_render_target_image, device_vsm_depth_image, device_smaa_image;
	VkImageView device_depth_image_view, device_stencil_image_view, device_render_target_image_view,
		device_render_target_second_image_view, device_vsm_depth_0_image_view, device_vsm_depth_1_image_view,
		device_smaa_edge_image_view, device_smaa_weight_image_view;
	VkSampler device_render_target_sampler, device_shadow_map_sampler;
	std::array<VkMemoryRequirements, 5> device_memory_attachments_requirements;
	VkDeviceMemory device_memory_attachments;

	VkDescriptorPool descriptor_pool;
	std::array<VkDescriptorSetLayout, 1> gaussian_blur_descriptor_sets_layout;
	std::array<VkDescriptorSetLayout, 1> pbr_descriptor_sets_layout;
	std::array<VkDescriptorSetLayout, 1> camera_light_sets_layout;
	std::array<VkDescriptorSetLayout, 4> smaa_descriptor_sets_layout;
	std::array<VkDescriptorSetLayout, 1> hdr_descriptor_sets_layout;
	std::array<VkDescriptorSet, 2> gaussian_blur_descriptor_sets;
	std::array<VkDescriptorSet, 3> pbr_descriptor_sets;
	std::array<VkDescriptorSet, 2> camera_light_sets;
	std::array<VkDescriptorSet, 4> smaa_descriptor_sets;
	std::array<VkDescriptorSet, 2> hdr_descriptor_sets;

	VkRenderPass shadow_map_render_pass, pbr_render_pass, smaa_edge_renderpass, smaa_weight_renderpass, smaa_blend_renderpass, hdr_tonemap_renderpass;
	std::vector<VkFramebuffer> framebuffers;
	std::vector<VkImageView> swapchain_images_views;

	VkPipelineLayout shadow_map_pipeline_layout, light_pipeline_layout, pbr_pipeline_layout, smaa_edge_pipeline_layout, smaa_weight_pipeline_layout,
					smaa_blend_pipeline_layout, hdr_tonemap_pipeline_layout;
	VkPipeline shadow_map_pipeline, light_pipeline, pbr_pipeline, smaa_edge_pipeline, smaa_weight_pipeline, smaa_blend_pipeline, hdr_tonemap_pipeline;
	
	std::array<VkPipelineLayout,2> gaussian_blur_xy_pipeline_layouts;
	std::array<VkPipeline,2> gaussian_blur_xy_pipelines;

	std::vector<VkSemaphore> semaphores;

	glm::mat4 water_bottle_m_matrix, water_bottle_normal_matrix, box_m_matrix, box_normal_matrix, table_m_matrix, table_normal_matrix;
	glm::mat4 v_matrix, inverse_v_matrix, p_matrix, camera_v_matrix, camera_p_matrix;
	glm::vec4 camera_pos, camera_dir, light_pos[4], light_color[4];
	double ex_xpos = -1, ex_ypos = -1;

public:
	VulkanSSAO();
	void start_main_loop();
	~VulkanSSAO();

	typedef enum Errors {
		VOLK_INITIALIZATION_FAILED = -1,
		GLFW_INITIALIZATION_FAILED = -2,
		INSTANCE_CREATION_FAILED = -3,
		GLFW_WINDOW_CREATION_FAILED = -4,
		SURFACE_CREATION_FAILED = -5,
		DEVICE_CREATION_FAILED = -6,
		SWAPCHAIN_CREATION_FAILED = -7,
		COMMAND_POOL_CREATION_FAILED = -8,
		COMMAND_BUFFER_CREATION_FAILED = -9,
		MEMORY_ALLOCATION_FAILED = -10,
		SHADER_MODULE_CREATION_FAILED = -11,
		ACQUIRE_NEXT_IMAGE_FAILED = -12,
		QUEUE_PRESENT_FAILED = -13
	} Errors;
};

void VulkanSSAO::create_instance() {
	if (volkInitialize() != VK_SUCCESS) { throw VOLK_INITIALIZATION_FAILED; }
	if (glfwInit() != GLFW_TRUE) { throw GLFW_INITIALIZATION_FAILED; }

	VkApplicationInfo application_info = {
		VK_STRUCTURE_TYPE_APPLICATION_INFO,
		nullptr,
		"Vulkan Teapot",
		VK_MAKE_VERSION(1,0,0),
		"Pure Vulkan",
		VK_MAKE_VERSION(1,0,0),
		VK_MAKE_VERSION(1,0,0)
	};

#ifdef NDEBUG
	std::vector<const char*> desired_validation_layers = {};
	std::vector<const char*> desired_instance_level_extensions = { "VK_KHR_surface"};
#else
	std::vector<const char*> desired_validation_layers = { "VK_LAYER_KHRONOS_validation" };
	std::vector<const char*> desired_instance_level_extensions = { "VK_KHR_surface","VK_EXT_debug_report" };
#endif

#ifdef _WIN64
	desired_instance_level_extensions.push_back("VK_KHR_win32_surface");
#elif __linux__
	desired_instance_level_extensions.push_back("VK_KHR_xlib_surface");
#else
	#error "Unknown compiler or not supported OS"
#endif

	VkInstanceCreateInfo instance_create_info = {
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		nullptr,
		0,
		&application_info,
		desired_validation_layers.size(),
		desired_validation_layers.data(),
		desired_instance_level_extensions.size(),
		desired_instance_level_extensions.data()
	};

	if (vkCreateInstance(&instance_create_info, nullptr, &instance)) {
		instance = VK_NULL_HANDLE;
		throw INSTANCE_CREATION_FAILED;
	}
	volkLoadInstance(instance);
}

void VulkanSSAO::create_window() {
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window = glfwCreateWindow(window_size.width, window_size.height, "Vulkan SMAA", nullptr, nullptr);
	glfwSetKeyCallback(window, key_callback_static);
	glfwSetCursorPosCallback(window, cursor_position_callback_static);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	if (window == NULL) { throw GLFW_WINDOW_CREATION_FAILED; }
}

void VulkanSSAO::setup_debug_callback() {
	VkDebugReportCallbackCreateInfoEXT debug_report_callback_create_info_EXT = {
		VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
		nullptr,
		VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT,
		vulkan_helper::debug_callback,
		nullptr
	};
	vkCreateDebugReportCallbackEXT(instance, &debug_report_callback_create_info_EXT, nullptr, &debug_report_callback);
}

void VulkanSSAO::create_surface() {
#ifdef _WIN64
	VkWin32SurfaceCreateInfoKHR surface_create_info = {
		VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		nullptr,
		0,
		GetModuleHandle(NULL),
		glfwGetWin32Window(window)
	};
	if (vkCreateWin32SurfaceKHR(instance, &surface_create_info, nullptr, &surface) != VK_SUCCESS) { throw SURFACE_CREATION_FAILED; }
#elif __linux__
	    VkXlibSurfaceCreateInfoKHR surface_create_info = {
        VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
        nullptr,
        0,
        glfwGetX11Display(),
        glfwGetX11Window(window)
    };
    if (vkCreateXlibSurfaceKHR(instance, &surface_create_info, nullptr, &surface) != VK_SUCCESS) { throw SURFACE_CREATION_FAILED; }
#else
	#error "Unknown compiler or not supported OS"
#endif
}

void VulkanSSAO::create_logical_device() {
	uint32_t devices_number;
	vkEnumeratePhysicalDevices(instance, &devices_number, nullptr);
	std::vector<VkPhysicalDevice> devices(devices_number);
	std::vector<VkPhysicalDeviceProperties> devices_properties(devices_number);
	std::vector<VkPhysicalDeviceFeatures> devices_features(devices_number);
	vkEnumeratePhysicalDevices(instance, &devices_number, devices.data());

	for (uint32_t i = 0; i < devices.size(); i++) {
		vkGetPhysicalDeviceProperties(devices[i], &devices_properties[i]);
		vkGetPhysicalDeviceFeatures(devices[i], &devices_features[i]);
	}
	size_t selected_device_number = 0;

	// we get the device memory properties because we need them later for allocations
	physical_device = devices[selected_device_number];
	vkGetPhysicalDeviceMemoryProperties(physical_device, &physical_device_memory_properties);

	uint32_t families_count;
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &families_count, nullptr);
	std::vector<VkQueueFamilyProperties> queue_families_properties(families_count);
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &families_count, queue_families_properties.data());

	queue_family_index = -1;
	VkBool32 does_queue_family_support_surface = VK_FALSE;
	while (does_queue_family_support_surface == VK_FALSE) {
		queue_family_index++;
		vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, queue_family_index, surface, &does_queue_family_support_surface);
	}

	//logical device creation
	std::vector<float> queue_priorities = { 1.0f };
	std::vector<VkDeviceQueueCreateInfo> queue_create_info;
	queue_create_info.push_back({
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		nullptr,
		0,
		static_cast<uint32_t>(queue_family_index),
		static_cast<uint32_t>(queue_priorities.size()),
		queue_priorities.data()
		});

	std::vector<const char*> desired_device_level_extensions = { "VK_KHR_swapchain"};
	VkPhysicalDeviceFeatures selected_device_features = { 0 };
	selected_device_features.samplerAnisotropy = VK_TRUE;
	selected_device_features.shaderStorageImageWriteWithoutFormat = VK_TRUE;

	VkDeviceCreateInfo device_create_info = {
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		nullptr,
		0,
		queue_create_info.size(),
		queue_create_info.data(),
		0,
		nullptr,
		desired_device_level_extensions.size(),
		desired_device_level_extensions.data(),
		&selected_device_features
	};

	if (vkCreateDevice(physical_device, &device_create_info, nullptr, &device)) { throw DEVICE_CREATION_FAILED; }
	vkGetDeviceQueue(device, queue_family_index, 0, &queue);
	volkLoadDevice(device);
}

void VulkanSSAO::create_swapchain() {
	uint32_t presentation_modes_number;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &presentation_modes_number, nullptr);
	std::vector<VkPresentModeKHR> presentation_modes(presentation_modes_number);
	vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &presentation_modes_number, presentation_modes.data());
	VkPresentModeKHR selected_present_mode = vulkan_helper::select_presentation_mode(presentation_modes, VK_PRESENT_MODE_IMMEDIATE_KHR);

	VkSurfaceCapabilitiesKHR surface_capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities);

	uint32_t number_of_images = vulkan_helper::select_number_of_images(surface_capabilities);
	VkExtent2D size_of_images = vulkan_helper::select_size_of_images(surface_capabilities, window_size);
	VkImageUsageFlags image_usage = vulkan_helper::select_image_usage(surface_capabilities, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
	VkSurfaceTransformFlagBitsKHR surface_transform = vulkan_helper::select_surface_transform(surface_capabilities, VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR);

	uint32_t formats_count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &formats_count, nullptr);
	std::vector<VkSurfaceFormatKHR> surface_formats(formats_count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &formats_count, surface_formats.data());
	VkSurfaceFormatKHR surface_format = vulkan_helper::select_surface_format(surface_formats, { VK_FORMAT_B8G8R8A8_UNORM ,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });

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
	if (vkCreateSwapchainKHR(device, &swapchain_create_info, nullptr, &swapchain) != VK_SUCCESS) { throw SWAPCHAIN_CREATION_FAILED; }

	vkGetSwapchainImagesKHR(device, swapchain, &swapchain_images_count, nullptr);
	swapchain_images.resize(swapchain_images_count);
	vkGetSwapchainImagesKHR(device, swapchain, &swapchain_images_count, swapchain_images.data());
}

void VulkanSSAO::calculate_projection_matrix() {
	p_matrix = glm::perspective(glm::radians(90.0f), static_cast<float>(window_size.width) / window_size.height, 0.1f, 1000.0f);
	p_matrix[1][1] *= -1;

	camera_p_matrix = glm::perspective(glm::radians(120.0f), static_cast<float>(window_size.width) / window_size.height, 0.1f, 1000.0f);
	camera_p_matrix[1][1] *= -1;
}

void VulkanSSAO::create_command_pool() {
	VkCommandPoolCreateInfo command_pool_create_info = {
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		nullptr,
		0,
		queue_family_index
	};
	if (vkCreateCommandPool(device, &command_pool_create_info, nullptr, &command_pool)) { throw COMMAND_POOL_CREATION_FAILED; }
}

void VulkanSSAO::allocate_command_buffers() {
	VkCommandBufferAllocateInfo command_buffer_allocate_info = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		nullptr,
		command_pool,
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		swapchain_images_count
	};
	command_buffers.resize(swapchain_images_count);
	if (vkAllocateCommandBuffers(device, &command_buffer_allocate_info, command_buffers.data())) { throw COMMAND_BUFFER_CREATION_FAILED; }
}

void VulkanSSAO::create_host_buffers() {
	tinygltf::TinyGLTF loader;
	tinygltf::Model water_bottle, box, sphere, table;
	std::string err;
	std::string warn;

	loader.LoadBinaryFromFile(&water_bottle, &err, &warn, "resources//models//WaterBottle//WaterBottle.glb");
	vulkan_helper::copy_gltf_contents(water_bottle,
		std::vector{
		vulkan_helper::v_model_attributes::V_VERTEX,
		vulkan_helper::v_model_attributes::V_TEX_COORD,
		vulkan_helper::v_model_attributes::V_NORMAL,
		vulkan_helper::v_model_attributes::V_TANGENT },
		true,
		true,
		std::vector{
		vulkan_helper::t_model_attributes::T_ALBEDO_MAP,
		vulkan_helper::t_model_attributes::T_ORM_MAP,
		vulkan_helper::t_model_attributes::T_NORMAL_MAP,
		vulkan_helper::t_model_attributes::T_EMISSIVE_MAP },
		nullptr, water_bottle_model_data);

	loader.LoadBinaryFromFile(&box, &err, &warn, "resources//models//Box//Box.glb");
	vulkan_helper::copy_gltf_contents(box,
		std::vector{
		vulkan_helper::v_model_attributes::V_VERTEX,
		vulkan_helper::v_model_attributes::V_TEX_COORD,
		vulkan_helper::v_model_attributes::V_NORMAL,
		vulkan_helper::v_model_attributes::V_TANGENT },
		true,
		true,
		std::vector{
		vulkan_helper::t_model_attributes::T_NORMAL_MAP,
		vulkan_helper::t_model_attributes::T_ALBEDO_MAP,
		vulkan_helper::t_model_attributes::T_ORM_MAP },
		nullptr, box_model_data);
	box_model_data.image_layers++;

	loader.LoadBinaryFromFile(&table, &err, &warn, "resources//models//Table//Table.glb");
	vulkan_helper::copy_gltf_contents(table,
		std::vector{
		vulkan_helper::v_model_attributes::V_VERTEX,
		vulkan_helper::v_model_attributes::V_TEX_COORD,
		vulkan_helper::v_model_attributes::V_NORMAL,
		vulkan_helper::v_model_attributes::V_TANGENT },
		true,
		true,
		std::vector{
		vulkan_helper::t_model_attributes::T_ALBEDO_MAP,
		vulkan_helper::t_model_attributes::T_ORM_MAP,
		vulkan_helper::t_model_attributes::T_NORMAL_MAP,
		vulkan_helper::t_model_attributes::T_EMISSIVE_MAP},
		nullptr, table_model_data);

	loader.LoadBinaryFromFile(&sphere, &err, &warn, "resources//models//Sphere//Sphere.glb");
	vulkan_helper::copy_gltf_contents(sphere,
		std::vector{
		vulkan_helper::v_model_attributes::V_VERTEX,
		vulkan_helper::v_model_attributes::V_NORMAL },
		true,
		true,
		std::vector<vulkan_helper::t_model_attributes>(),
		nullptr, sphere_model_data);

	// images for SMAA
	// AreaTexDx10.R8G8 is 160x560
	std::string area_tex_file_path = "resources//textures//AreaTexDX10.R8G8";
	area_tex_size = std::filesystem::file_size(area_tex_file_path.c_str());
	std::ifstream area_tex(area_tex_file_path, std::ifstream::in | std::ifstream::binary);

	// SearchTex.R8 is 64x16
	std::string search_tex_file_path = "resources//textures//SearchTex.R8";
	search_tex_size = std::filesystem::file_size(search_tex_file_path.c_str());
	std::fstream search_tex(search_tex_file_path, std::ifstream::in | std::ifstream::binary);

	VkBufferCreateInfo buffer_create_info = {
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		nullptr,
		0,
		vulkan_helper::get_model_data_total_size(water_bottle_model_data) +
		vulkan_helper::get_model_data_total_size(box_model_data) +
		vulkan_helper::get_model_data_total_size(table_model_data) +
		vulkan_helper::get_model_data_total_size(sphere_model_data) +
		area_tex_size + search_tex_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		0,
		nullptr
	};
	vkCreateBuffer(device, &buffer_create_info, nullptr, &host_vertex_buffer);

	//	bottle descriptor		box descriptor			table descriptor
	//	[---------------]-------[---------------]-------[---------------]-------
	//	[ set:0	128b    ] 128b	[ set:0	128b    ] 128b	[ set:0	128b    ] 128b
	//	[ 2*mat4 		]		[ 2*mat4 		]		[ 2*mat4		]
	//	[---------------]-------[---------------]-------[---------------]-------

	//	camera descriptor		light_descriptor		smaa_rt_metrics descriptor
	//	[---------------]-------[---------------]-------[---------------] 
	//	[ set:1	208b    ] 48b	[ set:2	160b   	] 	96b	[ set:3	16b		]
	//	[ 3*mat4+vec4	]		[ 2*mat4+2*vec4	]		[ 1*vec4		]
	//	[---------------]-------[---------------]-------[---------------]

	buffer_create_info.size = 256*5+16;
	vkCreateBuffer(device, &buffer_create_info, nullptr, &host_uniform_buffer);

	vkGetBufferMemoryRequirements(device, host_vertex_buffer, &host_memory_requirements[0]);
	vkGetBufferMemoryRequirements(device, host_uniform_buffer, &host_memory_requirements[1]);

	VkMemoryAllocateInfo memory_allocate_info = {
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		nullptr,
		host_memory_requirements[0].size + host_memory_requirements[1].size,
		vulkan_helper::select_memory_index(physical_device_memory_properties,host_memory_requirements[0],VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	};
	if (vkAllocateMemory(device, &memory_allocate_info, nullptr, &host_memory) != VK_SUCCESS) { throw MEMORY_ALLOCATION_FAILED; }

	vkBindBufferMemory(device, host_vertex_buffer, host_memory, 0);
	vkBindBufferMemory(device, host_uniform_buffer, host_memory, host_memory_requirements[0].size);

	vkMapMemory(device, host_memory, 0, VK_WHOLE_SIZE, 0, &host_data_pointer);

	vulkan_helper::copy_gltf_contents(water_bottle,
		std::vector{
		vulkan_helper::v_model_attributes::V_VERTEX,
		vulkan_helper::v_model_attributes::V_TEX_COORD,
		vulkan_helper::v_model_attributes::V_NORMAL,
		vulkan_helper::v_model_attributes::V_TANGENT },
		true,
		true,
		std::vector{
		vulkan_helper::t_model_attributes::T_ALBEDO_MAP,
		vulkan_helper::t_model_attributes::T_ORM_MAP,
		vulkan_helper::t_model_attributes::T_NORMAL_MAP,
		vulkan_helper::t_model_attributes::T_EMISSIVE_MAP },
		host_data_pointer, water_bottle_model_data);

	int offset = vulkan_helper::get_model_data_total_size(water_bottle_model_data);
	
	vulkan_helper::copy_gltf_contents(box,
		std::vector{
		vulkan_helper::v_model_attributes::V_VERTEX,
		vulkan_helper::v_model_attributes::V_TEX_COORD,
		vulkan_helper::v_model_attributes::V_NORMAL,
		vulkan_helper::v_model_attributes::V_TANGENT },
		true,
		true,
		std::vector{
		vulkan_helper::t_model_attributes::T_NORMAL_MAP,
		vulkan_helper::t_model_attributes::T_ALBEDO_MAP,
		vulkan_helper::t_model_attributes::T_ORM_MAP },
		static_cast<uint8_t*>(host_data_pointer) + offset, box_model_data);

	box_model_data.host_interleaved_vertex_data_offset += offset;
	box_model_data.host_index_data_offset += offset;
	box_model_data.host_image_data_offset += offset;

	// black image generation
	offset += vulkan_helper::get_model_data_total_size(box_model_data);
	for (int i = 0; i < 2048*2048*4; i++) {
		(static_cast<uint8_t*>(host_data_pointer)+offset)[i] = 0;
	}
	box_model_data.image_layers++;
	offset += 2048*2048*4;

	vulkan_helper::copy_gltf_contents(table,
		std::vector{
		vulkan_helper::v_model_attributes::V_VERTEX,
		vulkan_helper::v_model_attributes::V_TEX_COORD,
		vulkan_helper::v_model_attributes::V_NORMAL,
		vulkan_helper::v_model_attributes::V_TANGENT },
		true,
		true,
		std::vector{
		vulkan_helper::t_model_attributes::T_ALBEDO_MAP,
		vulkan_helper::t_model_attributes::T_ORM_MAP,
		vulkan_helper::t_model_attributes::T_NORMAL_MAP,
		vulkan_helper::t_model_attributes::T_EMISSIVE_MAP},
		static_cast<uint8_t*>(host_data_pointer) + offset, table_model_data);
	table_model_data.host_interleaved_vertex_data_offset += offset;
	table_model_data.host_index_data_offset += offset;
	table_model_data.host_image_data_offset += offset;
	offset += vulkan_helper::get_model_data_total_size(table_model_data);

	vulkan_helper::copy_gltf_contents(sphere,
		std::vector{
		vulkan_helper::v_model_attributes::V_VERTEX,
		vulkan_helper::v_model_attributes::V_NORMAL },
		true,
		true,
		std::vector<vulkan_helper::t_model_attributes>(),
		static_cast<uint8_t*>(host_data_pointer) + offset, sphere_model_data);
	sphere_model_data.host_interleaved_vertex_data_offset += offset;
	sphere_model_data.host_index_data_offset += offset;
	offset += vulkan_helper::get_model_data_total_size(sphere_model_data);

	// SMAA images copying
	area_tex.read(reinterpret_cast<char*>(host_data_pointer) + offset, area_tex_size);
	search_tex.read(static_cast<char*>(host_data_pointer) + offset + area_tex_size, search_tex_size);

	VkMappedMemoryRange mapped_memory_range = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, host_memory,0,host_memory_requirements[0].size };
	vkFlushMappedMemoryRanges(device, 1, &mapped_memory_range);
}

void VulkanSSAO::create_device_buffers() {
	// vertex data size + index data size for all models
	VkBufferCreateInfo buffer_create_info = {
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		nullptr,
		0,
		water_bottle_model_data.interleaved_vertex_data_size + water_bottle_model_data.index_data_size +
		box_model_data.interleaved_vertex_data_size + box_model_data.index_data_size +
		table_model_data.interleaved_vertex_data_size + table_model_data.index_data_size +
		sphere_model_data.interleaved_vertex_data_size + sphere_model_data.index_data_size,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		0,
		nullptr
	};
	vkCreateBuffer(device, &buffer_create_info, nullptr, &device_vertex_buffer);
	
	buffer_create_info.size = 256*5+16;
	buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	vkCreateBuffer(device, &buffer_create_info, nullptr, &device_uniform_buffer);

	VkImageCreateInfo image_create_info = {
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		nullptr,
		VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
		VK_IMAGE_TYPE_2D,
		VK_FORMAT_R8G8B8A8_UNORM,
		water_bottle_model_data.image_size,
		1,
		water_bottle_model_data.image_layers,
		VK_SAMPLE_COUNT_1_BIT,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		0,
		nullptr,
		VK_IMAGE_LAYOUT_UNDEFINED
	};
	vkCreateImage(device, &image_create_info, nullptr, &device_water_bottle_image);

	image_create_info.extent = box_model_data.image_size;
	image_create_info.arrayLayers = box_model_data.image_layers;
	vkCreateImage(device, &image_create_info, nullptr, &device_box_image);

	image_create_info.extent = table_model_data.image_size;
	image_create_info.arrayLayers = table_model_data.image_layers;
	vkCreateImage(device, &image_create_info, nullptr, &device_table_image);

	image_create_info.flags = 0;
	image_create_info.format = VK_FORMAT_R8G8_UNORM;
	image_create_info.extent = {160,560,1};
	image_create_info.arrayLayers = 1;
	vkCreateImage(device, &image_create_info, nullptr, &device_smaa_area_image);

	image_create_info.format = VK_FORMAT_R8_UNORM;
	image_create_info.extent = { 64,16,1 };
	vkCreateImage(device, &image_create_info, nullptr, &device_smaa_search_image);

	vkGetBufferMemoryRequirements(device, device_vertex_buffer, &device_memory_requirements[0]);
	vkGetBufferMemoryRequirements(device, device_uniform_buffer, &device_memory_requirements[1]);
	vkGetImageMemoryRequirements(device, device_water_bottle_image, &device_memory_requirements[2]);
	vkGetImageMemoryRequirements(device, device_box_image, &device_memory_requirements[3]);
	vkGetImageMemoryRequirements(device, device_table_image, &device_memory_requirements[4]);
	vkGetImageMemoryRequirements(device, device_smaa_area_image, &device_memory_requirements[5]);
	vkGetImageMemoryRequirements(device, device_smaa_search_image, &device_memory_requirements[6]);

	uint32_t alignment = vulkan_helper::get_buffer_image_alignment(device_memory_requirements[0].size + device_memory_requirements[1].size, device_memory_requirements[2].alignment);

	uint64_t allocation_size = alignment;
	for (const auto& device_memory_requirement : device_memory_requirements) {
		allocation_size += device_memory_requirement.size;
	}

	VkMemoryAllocateInfo memory_allocate_info = {
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		nullptr,
		allocation_size,
		vulkan_helper::select_memory_index(physical_device_memory_properties,device_memory_requirements[2],VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	};
	if (vkAllocateMemory(device, &memory_allocate_info, nullptr, &device_memory) != VK_SUCCESS) { throw MEMORY_ALLOCATION_FAILED; }

	auto sum_offsets = [&](int idx) {
		int sum = 0;
		for (int i = 0; i < idx; i++) {
			sum += device_memory_requirements[i].size;
		}
		return sum;
	};

	vkBindBufferMemory(device, device_vertex_buffer, device_memory, 0);
	vkBindBufferMemory(device, device_uniform_buffer, device_memory, sum_offsets(1));
	vkBindImageMemory(device, device_water_bottle_image, device_memory, sum_offsets(2) + alignment);
	vkBindImageMemory(device, device_box_image, device_memory, sum_offsets(3) + alignment);
	vkBindImageMemory(device, device_table_image, device_memory, sum_offsets(4) + alignment);
	vkBindImageMemory(device, device_smaa_area_image, device_memory, sum_offsets(5) + alignment);
	vkBindImageMemory(device, device_smaa_search_image, device_memory, sum_offsets(6) + alignment);

	// Water bottle image view creation
	VkImageViewCreateInfo image_view_create_info = {
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		nullptr,
		0,
		device_water_bottle_image,
		VK_IMAGE_VIEW_TYPE_2D,
		VK_FORMAT_R8G8B8A8_SRGB,
		{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
		{VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 1}
	};
	vkCreateImageView(device, &image_view_create_info, nullptr, &device_water_bottle_color_image_view);

	image_view_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
	image_view_create_info.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 1, 1 };
	vkCreateImageView(device, &image_view_create_info, nullptr, &device_water_bottle_orm_image_view);

	image_view_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
	image_view_create_info.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 2, 1 };
	vkCreateImageView(device, &image_view_create_info, nullptr, &device_water_bottle_normal_image_view);

	image_view_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
	image_view_create_info.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 3, 1 };
	vkCreateImageView(device, &image_view_create_info, nullptr, &device_water_bottle_emissive_image_view);

	// Box image view creation
	image_view_create_info.image = device_box_image;
	image_view_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
	image_view_create_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 1};
	vkCreateImageView(device, &image_view_create_info, nullptr, &device_box_normal_image_view);

	image_view_create_info.format = VK_FORMAT_R8G8B8A8_SRGB;
	image_view_create_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 1, 1};
	vkCreateImageView(device, &image_view_create_info, nullptr, &device_box_color_image_view);

	image_view_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
	image_view_create_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 2, 1};
	vkCreateImageView(device, &image_view_create_info, nullptr, &device_box_orm_image_view);

	image_view_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
	image_view_create_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 3, 1};
	vkCreateImageView(device, &image_view_create_info, nullptr, &device_box_emissive_image_view);

	// Table image view creation
	image_view_create_info.image = device_table_image;
	image_view_create_info.format = VK_FORMAT_R8G8B8A8_SRGB;
	image_view_create_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 1};
	vkCreateImageView(device, &image_view_create_info, nullptr, &device_table_color_image_view);

	image_view_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
	image_view_create_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 1, 1};
	vkCreateImageView(device, &image_view_create_info, nullptr, &device_table_orm_image_view);

	image_view_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
	image_view_create_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 2, 1};
	vkCreateImageView(device, &image_view_create_info, nullptr, &device_table_normal_image_view);

	image_view_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
	image_view_create_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 3, 1};
	vkCreateImageView(device, &image_view_create_info, nullptr, &device_table_emissive_image_view);

	// SMAA textures image view creation
	image_view_create_info.image = device_smaa_area_image;
	image_view_create_info.format = VK_FORMAT_R8G8_UNORM;
	image_view_create_info.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 1 };
	vkCreateImageView(device, &image_view_create_info, nullptr, &device_smaa_area_image_view);

	image_view_create_info.image = device_smaa_search_image;
	image_view_create_info.format = VK_FORMAT_R8_UNORM;
	vkCreateImageView(device, &image_view_create_info, nullptr, &device_smaa_search_image_view);

	VkSamplerCreateInfo sampler_create_info = {
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		nullptr,
		0,
		VK_FILTER_LINEAR,
		VK_FILTER_LINEAR,
		VK_SAMPLER_MIPMAP_MODE_LINEAR,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		1,
		VK_TRUE,
		16.0f,
		VK_FALSE,
		VK_COMPARE_OP_ALWAYS,
		0.0f,
		1.0f,
		VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
		VK_FALSE,
	};
	vkCreateSampler(device, &sampler_create_info, nullptr, &device_max_aniso_linear_sampler);

	water_bottle_model_data.device_interleaved_vertex_data_offset = 0;
	water_bottle_model_data.device_index_data_offset = water_bottle_model_data.interleaved_vertex_data_size;
	water_bottle_model_data.device_image_data_offset = sum_offsets(2) + alignment;

	box_model_data.device_interleaved_vertex_data_offset = water_bottle_model_data.interleaved_vertex_data_size + water_bottle_model_data.index_data_size;
	box_model_data.device_index_data_offset = box_model_data.device_interleaved_vertex_data_offset + box_model_data.interleaved_vertex_data_size;
	box_model_data.device_image_data_offset = sum_offsets(3) + alignment;

	table_model_data.device_interleaved_vertex_data_offset = box_model_data.device_index_data_offset + box_model_data.index_data_size;
	table_model_data.device_index_data_offset = table_model_data.device_interleaved_vertex_data_offset + table_model_data.interleaved_vertex_data_size;
	table_model_data.device_image_data_offset = sum_offsets(4) + alignment;

	sphere_model_data.device_interleaved_vertex_data_offset = table_model_data.device_index_data_offset + table_model_data.index_data_size;
	sphere_model_data.device_index_data_offset = sphere_model_data.device_interleaved_vertex_data_offset + sphere_model_data.interleaved_vertex_data_size;
}

void VulkanSSAO::create_attachments() {
	VkImageCreateInfo image_create_info = {
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		nullptr,
		0,
		VK_IMAGE_TYPE_2D,
		VK_FORMAT_D32_SFLOAT,
		{ window_size.width, window_size.height, 1 },
		1,
		1,
		VK_SAMPLE_COUNT_1_BIT,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		0,
		nullptr,
		VK_IMAGE_LAYOUT_UNDEFINED
	};
	vkCreateImage(device, &image_create_info, nullptr, &device_depth_image);
	
	// Stencil image for smaa pass
	image_create_info.format = VK_FORMAT_S8_UINT;
	image_create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	image_create_info.arrayLayers = 1;
	vkCreateImage(device, &image_create_info, nullptr, &device_stencil_image);

	// Ping Pong HDR buffer
	image_create_info.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	image_create_info.arrayLayers = 2;
	image_create_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	vkCreateImage(device, &image_create_info, nullptr, &device_render_target_image);

	// Ping Pong R32B32 depth image for variance shadow mapping
	image_create_info.format = VK_FORMAT_R32G32_SFLOAT;
	image_create_info.arrayLayers = 2;
	image_create_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	vkCreateImage(device, &image_create_info, nullptr, &device_vsm_depth_image);

	// Smaa image will hold both the blend_tex and edge_tex
	image_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
	image_create_info.arrayLayers = 2;
	image_create_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	vkCreateImage(device, &image_create_info, nullptr, &device_smaa_image);

	vkGetImageMemoryRequirements(device, device_depth_image, &device_memory_attachments_requirements[0]);
	vkGetImageMemoryRequirements(device, device_stencil_image, &device_memory_attachments_requirements[1]);
	vkGetImageMemoryRequirements(device, device_render_target_image, &device_memory_attachments_requirements[2]);
	vkGetImageMemoryRequirements(device, device_vsm_depth_image, &device_memory_attachments_requirements[3]);
	vkGetImageMemoryRequirements(device, device_smaa_image, &device_memory_attachments_requirements[4]);

	uint64_t allocation_size = 0;
	for (const auto& device_memory_attachments_requirement : device_memory_attachments_requirements) {
		allocation_size += device_memory_attachments_requirement.size;
	}

	VkMemoryAllocateInfo memory_allocate_info = {
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		nullptr,
		allocation_size,
		vulkan_helper::select_memory_index(	physical_device_memory_properties,
											device_memory_attachments_requirements[0],
											VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	};
	if (vkAllocateMemory(device, &memory_allocate_info, nullptr, &device_memory_attachments) != VK_SUCCESS) { throw MEMORY_ALLOCATION_FAILED; }

	auto sum_offsets = [&](int idx) {
		int sum = 0;
		for (int i = 0; i < idx; i++) {
			sum += device_memory_attachments_requirements[i].size;
		}
		return sum;
	};

	vkBindImageMemory(device, device_depth_image, device_memory_attachments, 0);
	vkBindImageMemory(device, device_stencil_image, device_memory_attachments, sum_offsets(1));
	vkBindImageMemory(device, device_render_target_image, device_memory_attachments, sum_offsets(2));
	vkBindImageMemory(device, device_vsm_depth_image, device_memory_attachments, sum_offsets(3));
	vkBindImageMemory(device, device_smaa_image, device_memory_attachments, sum_offsets(4));

	VkImageViewCreateInfo image_view_create_info = {
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		nullptr,
		0,
		device_depth_image,
		VK_IMAGE_VIEW_TYPE_2D,
		VK_FORMAT_D32_SFLOAT,
		{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
		{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, VK_REMAINING_MIP_LEVELS, 0 , 1 }
	};
	vkCreateImageView(device, &image_view_create_info, nullptr, &device_depth_image_view);

	image_view_create_info.image = device_stencil_image;
	image_view_create_info.format = VK_FORMAT_S8_UINT;
	image_view_create_info.subresourceRange = { VK_IMAGE_ASPECT_STENCIL_BIT, 0, VK_REMAINING_MIP_LEVELS, 0 , 1 };
	vkCreateImageView(device, &image_view_create_info, nullptr, &device_stencil_image_view);

	// Render targets image views
	image_view_create_info.image = device_render_target_image;
	image_view_create_info.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	image_view_create_info.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 1 };
	vkCreateImageView(device, &image_view_create_info, nullptr, &device_render_target_image_view);

	image_view_create_info.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	image_view_create_info.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 1, 1 };
	vkCreateImageView(device, &image_view_create_info, nullptr, &device_render_target_second_image_view);

	// VSM image views
	image_view_create_info.image = device_vsm_depth_image;
	image_view_create_info.format = VK_FORMAT_R32G32_SFLOAT;
	image_view_create_info.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 1 };
	vkCreateImageView(device, &image_view_create_info, nullptr, &device_vsm_depth_0_image_view);

	image_view_create_info.format = VK_FORMAT_R32G32_SFLOAT;
	image_view_create_info.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 1, 1 };
	vkCreateImageView(device, &image_view_create_info, nullptr, &device_vsm_depth_1_image_view);

	// SMAA image views
	image_view_create_info.image = device_smaa_image;
	image_view_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
	image_view_create_info.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 1 };
	vkCreateImageView(device, &image_view_create_info, nullptr, &device_smaa_edge_image_view);

	image_view_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
	image_view_create_info.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 1, 1 };
	vkCreateImageView(device, &image_view_create_info, nullptr, &device_smaa_weight_image_view);

	VkSamplerCreateInfo sampler_create_info = {
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		nullptr,
		0,
		VK_FILTER_LINEAR,
		VK_FILTER_LINEAR,
		VK_SAMPLER_MIPMAP_MODE_LINEAR,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		1,
		VK_FALSE,
		0.0f,
		VK_FALSE,
		VK_COMPARE_OP_ALWAYS,
		0.0f,
		1.0f,
		VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
		VK_FALSE,
	};
	vkCreateSampler(device, &sampler_create_info, nullptr, &device_render_target_sampler);

	sampler_create_info = {
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		nullptr,
		0,
		VK_FILTER_LINEAR,
		VK_FILTER_LINEAR,
		VK_SAMPLER_MIPMAP_MODE_LINEAR,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
		0.0f,
		VK_TRUE,
		16.0f,
		VK_FALSE,
		VK_COMPARE_OP_LESS,
		0.0f,
		1.0f,
		VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
		VK_FALSE,
	};
	vkCreateSampler(device, &sampler_create_info, nullptr, &device_shadow_map_sampler);
}

void VulkanSSAO::create_descriptor_pool_and_layouts() {
	std::array<VkDescriptorPoolSize, 3> descriptor_pool_size { {
	{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 6},
	{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 25 },
	{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 }
	} };

	VkDescriptorPoolCreateInfo descriptor_pool_create_info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		nullptr,
		VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		12,
		descriptor_pool_size.size(),
		descriptor_pool_size.data()
	};
	vkCreateDescriptorPool(device, &descriptor_pool_create_info, nullptr, &descriptor_pool);

	std::array<VkDescriptorSetLayoutBinding,3> descriptor_set_layout_binding;
	
	// Layout of the depth r32g32 images for the gaussian blur pipelines
	descriptor_set_layout_binding[0] = {
		0,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		1,
		VK_SHADER_STAGE_COMPUTE_BIT,
		nullptr
	};
	descriptor_set_layout_binding[1] = {
		1,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		1,
		VK_SHADER_STAGE_COMPUTE_BIT,
		nullptr
	};
	VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		2,
		descriptor_set_layout_binding.data()
	};
	vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &gaussian_blur_descriptor_sets_layout[0]);

	// Layout for the pbr model
	descriptor_set_layout_binding[0] = {
		0,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		1,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		nullptr
	};
	descriptor_set_layout_binding[1] = {
		1,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		4,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		nullptr
	};
	descriptor_set_layout_binding[2] = {
		2,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		1,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		nullptr
	};
	descriptor_set_layout_create_info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		3,
		descriptor_set_layout_binding.data()
	};
	vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &pbr_descriptor_sets_layout[0]);

	// Layout for the camera and light descriptor model
	descriptor_set_layout_binding[0] = {
		0,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		1,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		nullptr
	};
	descriptor_set_layout_create_info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		1,
		descriptor_set_layout_binding.data()
	};
	vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &camera_light_sets_layout[0]);

	// Layout for the first descriptor for the first smaa pipeline
	descriptor_set_layout_binding[0] = {
		0,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		2,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		nullptr
	};
	descriptor_set_layout_create_info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		1,
		descriptor_set_layout_binding.data()
	};
	vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &smaa_descriptor_sets_layout[0]);

	// Layout for the second descriptor for the second smaa pipeline
	descriptor_set_layout_binding[0] = {
		0,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		3,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		nullptr
	};
	descriptor_set_layout_create_info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		1,
		descriptor_set_layout_binding.data()
	};
	vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &smaa_descriptor_sets_layout[1]);

	// Layout for the third descriptor for the third smaa pipeline
	descriptor_set_layout_binding[0] = {
		0,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		2,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		nullptr
	};
	descriptor_set_layout_create_info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		1,
		descriptor_set_layout_binding.data()
	};
	vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &smaa_descriptor_sets_layout[2]);

	// Layout for the fourth descriptor for SMAA_RT_METRICS
	descriptor_set_layout_binding[0] = {
		0,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		1,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
		nullptr
	};
	descriptor_set_layout_create_info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		1,
		descriptor_set_layout_binding.data()
	};
	vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &smaa_descriptor_sets_layout[3]);

	// Layout for hdr luminance resolve shader
	descriptor_set_layout_binding[0] = {
		0,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		1,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		nullptr
	};
	descriptor_set_layout_create_info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		1,
		descriptor_set_layout_binding.data()
	};
	vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, nullptr, &hdr_descriptor_sets_layout[0]);
}

void VulkanSSAO::allocate_gaussian_blur_descriptor_sets() {
	std::array<VkDescriptorSetLayout, 2> layouts_of_sets = {gaussian_blur_descriptor_sets_layout[0],gaussian_blur_descriptor_sets_layout[0]};
	VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		nullptr,
		descriptor_pool,
		layouts_of_sets.size(),
		layouts_of_sets.data()
	};
	vkAllocateDescriptorSets(device, &descriptor_set_allocate_info, gaussian_blur_descriptor_sets.data());
	
	VkDescriptorImageInfo in_shadow_map_descriptor_buffer_info_0 = {
		device_max_aniso_linear_sampler,
		device_vsm_depth_0_image_view,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};
	VkDescriptorImageInfo out_shadow_map_descriptor_buffer_info_0 = {
		device_max_aniso_linear_sampler,
		device_vsm_depth_1_image_view,
		VK_IMAGE_LAYOUT_GENERAL
	};

	VkDescriptorImageInfo in_shadow_map_descriptor_buffer_info_1 = {
		device_max_aniso_linear_sampler,
		device_vsm_depth_1_image_view,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};
	VkDescriptorImageInfo out_shadow_map_descriptor_buffer_info_1 = {
		device_max_aniso_linear_sampler,
		device_vsm_depth_0_image_view,
		VK_IMAGE_LAYOUT_GENERAL
	};

	// Second writes are for the gaussian blur x pipeline
	VkWriteDescriptorSet write_descriptor_set[] = { 
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		nullptr,
		gaussian_blur_descriptor_sets[0],
		0,
		0,
		1,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		&in_shadow_map_descriptor_buffer_info_0,
		nullptr,
		nullptr
	},
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		nullptr,
		gaussian_blur_descriptor_sets[0],
		1,
		0,
		1,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		&out_shadow_map_descriptor_buffer_info_0,
		nullptr,
		nullptr
	},
	// Second writes are for the gaussian blur y pipeline
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		nullptr,
		gaussian_blur_descriptor_sets[1],
		0,
		0,
		1,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		&in_shadow_map_descriptor_buffer_info_1,
		nullptr,
		nullptr
	},
	
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		nullptr,
		gaussian_blur_descriptor_sets[1],
		1,
		0,
		1,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		&out_shadow_map_descriptor_buffer_info_1,
		nullptr,
		nullptr
	}
	};
	vkUpdateDescriptorSets(device, 4, write_descriptor_set, 0, nullptr);
}

void VulkanSSAO::allocate_pbr_descriptor_sets() {
	std::array<VkDescriptorSetLayout, 3> layouts_of_sets = {pbr_descriptor_sets_layout[0],pbr_descriptor_sets_layout[0], pbr_descriptor_sets_layout[0]};
	VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		nullptr,
		descriptor_pool,
		layouts_of_sets.size(),
		layouts_of_sets.data()
	};
	vkAllocateDescriptorSets(device, &descriptor_set_allocate_info, pbr_descriptor_sets.data());

	VkDescriptorBufferInfo water_bottle_descriptor_buffer_info = { device_uniform_buffer, 0, 2 * sizeof(glm::mat4) };
	VkDescriptorImageInfo water_bottle_descriptor_images_info[] = {
		{ device_max_aniso_linear_sampler, device_water_bottle_color_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{ device_max_aniso_linear_sampler, device_water_bottle_orm_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{ device_max_aniso_linear_sampler, device_water_bottle_normal_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{ device_max_aniso_linear_sampler, device_water_bottle_emissive_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
	};

	VkDescriptorBufferInfo box_descriptor_buffer_info = { device_uniform_buffer, 256, 2 * sizeof(glm::mat4) };
	VkDescriptorImageInfo box_descriptor_images_info[] = {
		{ device_max_aniso_linear_sampler, device_box_color_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{ device_max_aniso_linear_sampler, device_box_orm_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{ device_max_aniso_linear_sampler, device_box_normal_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{ device_max_aniso_linear_sampler, device_box_emissive_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
	};

	VkDescriptorBufferInfo table_descriptor_buffer_info = { device_uniform_buffer, 512, 2 * sizeof(glm::mat4) };
	VkDescriptorImageInfo table_descriptor_images_info[] = {
		{ device_max_aniso_linear_sampler, device_table_color_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{ device_max_aniso_linear_sampler, device_table_orm_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{ device_max_aniso_linear_sampler, device_table_normal_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{ device_max_aniso_linear_sampler, device_table_emissive_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
	};

	VkDescriptorImageInfo shadow_map_image_info = {
		device_shadow_map_sampler, device_vsm_depth_0_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};

	VkWriteDescriptorSet write_descriptor_set[] = { {
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		nullptr,
		pbr_descriptor_sets[0],
		0,
		0,
		1,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		nullptr,
		&water_bottle_descriptor_buffer_info,
		nullptr
	},
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		nullptr,
		pbr_descriptor_sets[0],
		1,
		0,
		4,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		water_bottle_descriptor_images_info,
		nullptr,
		nullptr
	},
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		nullptr,
		pbr_descriptor_sets[0],
		2,
		0,
		1,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		&shadow_map_image_info,
		nullptr,
		nullptr
	},
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		nullptr,
		pbr_descriptor_sets[1],
		0,
		0,
		1,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		nullptr,
		&box_descriptor_buffer_info,
		nullptr
	},
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		nullptr,
		pbr_descriptor_sets[1],
		1,
		0,
		4,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		box_descriptor_images_info,
		nullptr,
		nullptr
	},
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		nullptr,
		pbr_descriptor_sets[1],
		2,
		0,
		1,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		&shadow_map_image_info,
		nullptr,
		nullptr
	},
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		nullptr,
		pbr_descriptor_sets[2],
		0,
		0,
		1,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		nullptr,
		&table_descriptor_buffer_info,
		nullptr
	},
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		nullptr,
		pbr_descriptor_sets[2],
		1,
		0,
		4,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		table_descriptor_images_info,
		nullptr,
		nullptr
	},
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		nullptr,
		pbr_descriptor_sets[2],
		2,
		0,
		1,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		&shadow_map_image_info,
		nullptr,
		nullptr
	}
	};
	vkUpdateDescriptorSets(device, 9, write_descriptor_set, 0, nullptr);
}

void VulkanSSAO::allocate_camera_light_sets() {
	std::array<VkDescriptorSetLayout, 2> layouts_of_sets = {camera_light_sets_layout[0],camera_light_sets_layout[0]};
	VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		nullptr,
		descriptor_pool,
		layouts_of_sets.size(),
		layouts_of_sets.data()
	};
	vkAllocateDescriptorSets(device, &descriptor_set_allocate_info, camera_light_sets.data());
	
	VkDescriptorBufferInfo camera_descriptor_buffer_info = {
		device_uniform_buffer,
		768,
		3*sizeof(glm::mat4)+sizeof(glm::vec4)
	};

	VkDescriptorBufferInfo light_descriptor_buffer_info = {
		device_uniform_buffer,
		1024,
		2*sizeof(glm::mat4)+2*sizeof(glm::vec4)
	};

	// First writes are for the camera descriptor
	VkWriteDescriptorSet write_descriptor_set[] = { {
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		nullptr,
		camera_light_sets[0],
		0,
		0,
		1,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		nullptr,
		&camera_descriptor_buffer_info,
		nullptr
	},
	// Second writes are for the light descriptor
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		nullptr,
		camera_light_sets[1],
		0,
		0,
		1,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		nullptr,
		&light_descriptor_buffer_info,
		nullptr
	}
	};
	vkUpdateDescriptorSets(device, 2, write_descriptor_set, 0, nullptr);
}

void VulkanSSAO::allocate_smaa_descriptor_sets() {
	// Allocation of both descriptors using the layouts
	VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		nullptr,
		descriptor_pool,
		smaa_descriptor_sets_layout.size(),
		smaa_descriptor_sets_layout.data()
	};
	vkAllocateDescriptorSets(device, &descriptor_set_allocate_info, smaa_descriptor_sets.data());

	VkDescriptorImageInfo smaa_edge_descriptor_images_info[] = { 
		{ device_render_target_sampler, device_render_target_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{ device_render_target_sampler, device_depth_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
	};

	VkDescriptorImageInfo smaa_weight_descriptor_images_info[] = {
		{ device_render_target_sampler, device_smaa_edge_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{ device_render_target_sampler, device_smaa_area_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{ device_render_target_sampler, device_smaa_search_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
	};

	VkDescriptorImageInfo smaa_blend_descriptor_images_info[] = {
		{ device_render_target_sampler, device_render_target_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{ device_render_target_sampler, device_smaa_weight_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
	};

	VkDescriptorBufferInfo smaa_rt_metrics = {device_uniform_buffer, 1280, sizeof(glm::vec4)};

	// First writes are for the smaa edge descriptor 
	VkWriteDescriptorSet write_descriptor_set[] = { {
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		nullptr,
		smaa_descriptor_sets[0],
		0,
		0,
		2,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		smaa_edge_descriptor_images_info,
		nullptr,
		nullptr
	},
		// Second writes are for the smaa weight descriptor
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		nullptr,
		smaa_descriptor_sets[1],
		0,
		0,
		3,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		smaa_weight_descriptor_images_info,
		nullptr,
		nullptr
	},
		// Third writes are for the smaa blend descriptor
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		nullptr,
		smaa_descriptor_sets[2],
		0,
		0,
		2,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		smaa_blend_descriptor_images_info,
		nullptr,
		nullptr
	},
		// Third writes are for the smaa blend descriptor
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		nullptr,
		smaa_descriptor_sets[3],
		0,
		0,
		1,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		nullptr,
		&smaa_rt_metrics,
		nullptr
	}
	};
	vkUpdateDescriptorSets(device, 4, write_descriptor_set, 0, nullptr);
}

void VulkanSSAO::allocate_hdr_descriptor_sets() {
	VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		nullptr,
		descriptor_pool,
		hdr_descriptor_sets_layout.size(),
		hdr_descriptor_sets_layout.data()
	};
	vkAllocateDescriptorSets(device, &descriptor_set_allocate_info, hdr_descriptor_sets.data());

	VkDescriptorImageInfo hdr_descriptor_image_info = {device_render_target_sampler, device_render_target_second_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

	VkWriteDescriptorSet write_descriptor_set[] = { {
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		nullptr,
		hdr_descriptor_sets[0],
		0,
		0,
		1,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		&hdr_descriptor_image_info,
		nullptr,
		nullptr
	}
	};
	vkUpdateDescriptorSets(device, 1, write_descriptor_set, 0, nullptr);
}

void VulkanSSAO::create_renderpass() {

	/* renderpasses
	*
	*	|-----------------------------------|
	*	| D-in: model_data + textures		|
	*	| #0 PBR							|
	*	| R-out: D16, R8G8B8A8_SRGB			|	>---------------|
	*	|-----------------------------------|					|
	*			|		|										|
	*			\		\										|
	*	|-----------------------------------|					|
	*	| D-in: D16, R8G8B8A8_UNORM			|					|
	*	| #1 SMAA-E							|					|
	*	| R-out: edge_tex					|					|
	*	|-----------------------------------|					|
	*			|												|
	*			\												|
	*	|-----------------------------------|					|
	*	| D-in: edge_tex, a_tex, s_tex		|					|
	*	| #2 SMAA-W							|					|
	*	| R-out: weight_tex					|					|
	*	|-----------------------------------|					|
	*				|											|
	*				\											|
	*	|-----------------------------------|					|
	*	| D-in: weight_tex, R8G8B8A8_UNORM	|	<---------------|
	*	| #3 SMAA-B							|
	*	| R-out: R8G8B8A8_UNORM (SRGB)		|		 
	*	|-----------------------------------|
	*/

	VkAttachmentDescription attachment_description[2];
	VkAttachmentReference attachment_reference[2];
	VkSubpassDescription subpass_description;
	VkRenderPassCreateInfo render_pass_create_info;

	// shadow_map renderpass creation
	attachment_description[0] = {
		0,
		VK_FORMAT_D32_SFLOAT,
		VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL
	};
	attachment_description[1] = {
		0,
		VK_FORMAT_R32G32_SFLOAT,
		VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_STORE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};
	attachment_reference[0] = { 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
	attachment_reference[1] = { 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	subpass_description = {
		0,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		0,
		nullptr,
		1,
		&attachment_reference[1],
		nullptr,
		&attachment_reference[0],
		0,
		nullptr
	};

	render_pass_create_info = {
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		nullptr,
		0,
		2,
		attachment_description,
		1,
		&subpass_description,
		0,
		nullptr
	};
	vkCreateRenderPass(device, &render_pass_create_info, nullptr, &shadow_map_render_pass);

	// PBR renderpass creation
	attachment_description[0] = {
		0,
		VK_FORMAT_D32_SFLOAT,
		VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_STORE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};
	attachment_description[1] = {
		0,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_STORE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};

	attachment_reference[0] = { 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
	attachment_reference[1] = { 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	subpass_description = {
		0,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		0,
		nullptr,
		1,
		&attachment_reference[1],
		nullptr,
		&attachment_reference[0],
		0,
		nullptr
	};

	render_pass_create_info = {
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		nullptr,
		0,
		2,
		attachment_description,
		1,
		&subpass_description,
		0,
		nullptr
	};
	vkCreateRenderPass(device, &render_pass_create_info, nullptr, &pbr_render_pass);

	// Smaa edge renderpass creation
	attachment_description[0] = {
		0,
		VK_FORMAT_S8_UINT,
		VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_STORE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
	};
	attachment_description[1] = {
		0,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_STORE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};

	attachment_reference[0] = { 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
	attachment_reference[1] = { 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	subpass_description = {
		0,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		0,
		nullptr,
		1,
		&attachment_reference[1],
		nullptr,
		&attachment_reference[0],
		0,
		nullptr
	};

	render_pass_create_info = {
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		nullptr,
		0,
		2,
		attachment_description,
		1,
		&subpass_description,
		0,
		nullptr
	};
	vkCreateRenderPass(device, &render_pass_create_info, nullptr, &smaa_edge_renderpass);

	// Smaa weight renderpass creation
	// TODO: final layout UNDEFINED?
	attachment_description[0] = {
		0,
		VK_FORMAT_S8_UINT,
		VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_ATTACHMENT_LOAD_OP_LOAD,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
	};
	attachment_description[1] = {
		0,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_STORE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};

	attachment_reference[0] = { 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL };
	attachment_reference[1] = { 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	subpass_description = {
		0,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		0,
		nullptr,
		1,
		&attachment_reference[1],
		nullptr,
		&attachment_reference[0],
		0,
		nullptr
	};

	render_pass_create_info = {
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		nullptr,
		0,
		2,
		attachment_description,
		1,
		&subpass_description,
		0,
		nullptr
	};
	vkCreateRenderPass(device, &render_pass_create_info, nullptr, &smaa_weight_renderpass);

	// Smaa blend renderpass creation
	attachment_description[0] = {
		0,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_STORE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};

	attachment_reference[0] = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	subpass_description = {
		0,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		0,
		nullptr,
		1,
		&attachment_reference[0],
		nullptr,
		nullptr,
		0,
		nullptr
	};

	render_pass_create_info = {
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		nullptr,
		0,
		1,
		attachment_description,
		1,
		&subpass_description,
		0,
		nullptr
	};
	vkCreateRenderPass(device, &render_pass_create_info, nullptr, &smaa_blend_renderpass);

	// HDR tonemap renderpass
	attachment_description[0] = {
		0,
		swapchain_create_info.imageFormat,
		VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_STORE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	};

	attachment_reference[0] = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	subpass_description = {
		0,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		0,
		nullptr,
		1,
		&attachment_reference[0],
		nullptr,
		nullptr,
		0,
		nullptr
	};

	render_pass_create_info = {
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		nullptr,
		0,
		1,
		attachment_description,
		1,
		&subpass_description,
		0,
		nullptr
	};
	vkCreateRenderPass(device, &render_pass_create_info, nullptr, &hdr_tonemap_renderpass);
}

void VulkanSSAO::create_framebuffers() {
	framebuffers.resize(5+swapchain_images_count);
	swapchain_images_views.resize(swapchain_images_count);

	VkImageView attachments[2];
	VkFramebufferCreateInfo framebuffer_create_info;

	attachments[0] = { device_depth_image_view };
	attachments[1] = { device_vsm_depth_0_image_view };
	framebuffer_create_info = {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		nullptr,
		0,
		shadow_map_render_pass,
		2,
		attachments,
		swapchain_create_info.imageExtent.width,
		swapchain_create_info.imageExtent.height,
		swapchain_create_info.imageArrayLayers
	};
	vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &framebuffers[0]);

	attachments[0] = { device_depth_image_view};
	attachments[1] = { device_render_target_image_view };
	framebuffer_create_info = {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		nullptr,
		0,
		pbr_render_pass,
		2,
		attachments,
		swapchain_create_info.imageExtent.width,
		swapchain_create_info.imageExtent.height,
		swapchain_create_info.imageArrayLayers
	};
	vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &framebuffers[1]);

	attachments[0] = { device_stencil_image_view };
	attachments[1] = { device_smaa_edge_image_view };
	framebuffer_create_info = {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		nullptr,
		0,
		smaa_edge_renderpass,
		2,
		attachments,
		swapchain_create_info.imageExtent.width,
		swapchain_create_info.imageExtent.height,
		swapchain_create_info.imageArrayLayers
	};
	vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &framebuffers[2]);

	attachments[0] = { device_stencil_image_view };
	attachments[1] = { device_smaa_weight_image_view };
	framebuffer_create_info = {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		nullptr,
		0,
		smaa_weight_renderpass,
		2,
		attachments,
		swapchain_create_info.imageExtent.width,
		swapchain_create_info.imageExtent.height,
		swapchain_create_info.imageArrayLayers
	};
	vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &framebuffers[3]);


	attachments[0] = { device_render_target_second_image_view };
	framebuffer_create_info = {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		nullptr,
		0,
		smaa_blend_renderpass,
		1,
		attachments,
		swapchain_create_info.imageExtent.width,
		swapchain_create_info.imageExtent.height,
		swapchain_create_info.imageArrayLayers
	};
	vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &framebuffers[4]);

	for (uint32_t i = 0; i < swapchain_images_count; i++) {
		VkImageViewCreateInfo image_view_create_info = {
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			nullptr,
			0,
			swapchain_images[i],
			VK_IMAGE_VIEW_TYPE_2D,
			swapchain_create_info.imageFormat,
			{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
			{VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0 , VK_REMAINING_ARRAY_LAYERS}
		};
		vkCreateImageView(device, &image_view_create_info, nullptr, &swapchain_images_views[i]);

		attachments[0] = { swapchain_images_views[i] };
		framebuffer_create_info = {
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			nullptr,
			0,
			hdr_tonemap_renderpass,
			1,
			attachments,
			swapchain_create_info.imageExtent.width,
			swapchain_create_info.imageExtent.height,
			swapchain_create_info.imageArrayLayers
		};
		vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &framebuffers[i + 5]);
	}

}

void VulkanSSAO::create_shadow_map_pipeline() {
	std::vector<uint8_t> shader_contents;
	vulkan_helper::get_binary_file_content("shaders//shadow_map.vert.spv", shader_contents);
	VkShaderModuleCreateInfo shader_module_create_info = {
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		nullptr,
		0,
		shader_contents.size(),
		reinterpret_cast<uint32_t*>(shader_contents.data())
	};
	VkShaderModule vertex_shader_module;
	if (vkCreateShaderModule(device, &shader_module_create_info, nullptr, &vertex_shader_module)) { throw SHADER_MODULE_CREATION_FAILED; }

	vulkan_helper::get_binary_file_content("shaders//shadow_map.frag.spv", shader_contents);
	shader_module_create_info.codeSize = shader_contents.size();
	shader_module_create_info.pCode = reinterpret_cast<uint32_t*>(shader_contents.data());
	VkShaderModule fragment_shader_module;
	if (vkCreateShaderModule(device, &shader_module_create_info, nullptr, &fragment_shader_module)) { throw SHADER_MODULE_CREATION_FAILED; }

	VkPipelineShaderStageCreateInfo pipeline_shaders_stage_create_info[2] = {
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		nullptr,
		0,
		VK_SHADER_STAGE_VERTEX_BIT,
		vertex_shader_module,
		"main",
		nullptr
	},
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		nullptr,
		0,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		fragment_shader_module,
		"main",
		nullptr
	}};

	VkVertexInputBindingDescription vertex_input_binding_description = {
		0,
		12 * sizeof(float),
		VK_VERTEX_INPUT_RATE_VERTEX
	};
	VkVertexInputAttributeDescription vertex_input_attribute_description[] = { {
		0,
		0,
		VK_FORMAT_R32G32B32_SFLOAT,
		0
	},
	{
		1,
		0,
		VK_FORMAT_R32G32_SFLOAT,
		3 * sizeof(float)
	},
	{
		2,
		0,
		VK_FORMAT_R32G32B32_SFLOAT,
		5 * sizeof(float)
	},
	{
		3,
		0,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		8 * sizeof(float)
	}
	};
	VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		nullptr,
		0,
		1,
		&vertex_input_binding_description,
		4,
		vertex_input_attribute_description
	};

	VkPipelineInputAssemblyStateCreateInfo pipeline_input_assembly_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VK_FALSE
	};

	VkViewport viewport = {
		0.0f,
		0.0f,
		static_cast<float>(swapchain_create_info.imageExtent.width),
		static_cast<float>(swapchain_create_info.imageExtent.height),
		0.0f,
		1.0f
	};
	VkRect2D scissor = {
		{0,0},
		swapchain_create_info.imageExtent
	};
	VkPipelineViewportStateCreateInfo pipeline_viewport_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		nullptr,
		0,
		1,
		&viewport,
		1,
		&scissor
	};

	VkPipelineRasterizationStateCreateInfo pipeline_rasterization_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_FALSE,
		VK_FALSE,
		VK_POLYGON_MODE_FILL,
		VK_CULL_MODE_FRONT_BIT,
		VK_FRONT_FACE_COUNTER_CLOCKWISE,
		VK_FALSE,
		0.0f,
		0.0f,
		0.0f,
		1.0f
	};

	VkPipelineMultisampleStateCreateInfo pipeline_multisample_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FALSE,
		1.0f,
		nullptr,
		VK_FALSE,
		VK_FALSE
	};

	VkPipelineDepthStencilStateCreateInfo pipeline_depth_stencil_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_TRUE,
		VK_TRUE,
		VK_COMPARE_OP_LESS,
		VK_FALSE,
		VK_FALSE,
		{},
		{},
		0.0f,
		1.0f
	};

	VkPipelineColorBlendAttachmentState pipeline_color_blend_attachment_state = {
		VK_FALSE,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_OP_ADD,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_OP_ADD,
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};
	VkPipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_FALSE,
		VK_LOGIC_OP_COPY,
		1,
		&pipeline_color_blend_attachment_state,
		{0.0f,0.0f,0.0f,0.0f}
	};

	std::array<VkDescriptorSetLayout,2> descriptor_set_layouts = { pbr_descriptor_sets_layout[0], camera_light_sets_layout[0] };
	VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		descriptor_set_layouts.size(),
		descriptor_set_layouts.data(),
		0,
		nullptr
	};
	vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &shadow_map_pipeline_layout);

	VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		nullptr,
		0,
		2,
		pipeline_shaders_stage_create_info,
		&pipeline_vertex_input_state_create_info,
		&pipeline_input_assembly_create_info,
		nullptr,
		&pipeline_viewport_state_create_info,
		&pipeline_rasterization_state_create_info,
		&pipeline_multisample_state_create_info,
		&pipeline_depth_stencil_state_create_info,
		&pipeline_color_blend_state_create_info,
		nullptr,
		shadow_map_pipeline_layout,
		shadow_map_render_pass,
		0,
		VK_NULL_HANDLE,
		-1
	};

	vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &shadow_map_pipeline);
	vkDestroyShaderModule(device, vertex_shader_module, nullptr);
	vkDestroyShaderModule(device, fragment_shader_module, nullptr);
}

void VulkanSSAO::create_gaussian_blur_pipelines() {
	std::vector<uint8_t> shader_contents;
	vulkan_helper::get_binary_file_content("shaders//gaussian_blur_x.comp.spv", shader_contents);
	VkShaderModuleCreateInfo shader_module_create_info = {
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		nullptr,
		0,
		shader_contents.size(),
		reinterpret_cast<uint32_t*>(shader_contents.data())
	};
	std::array<VkShaderModule,2> shader_modules;
	if (vkCreateShaderModule(device, &shader_module_create_info, nullptr, &shader_modules[0])) { throw SHADER_MODULE_CREATION_FAILED; }

	vulkan_helper::get_binary_file_content("shaders//gaussian_blur_y.comp.spv", shader_contents);
	shader_module_create_info.codeSize = shader_contents.size();
	shader_module_create_info.pCode = reinterpret_cast<uint32_t*>(shader_contents.data());
	if (vkCreateShaderModule(device, &shader_module_create_info, nullptr, &shader_modules[1])) { throw SHADER_MODULE_CREATION_FAILED; }

	std::array<VkPipelineShaderStageCreateInfo,2> pipeline_shaders_stage_create_infos;
	pipeline_shaders_stage_create_infos[0] = {
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		nullptr,
		0,
		VK_SHADER_STAGE_COMPUTE_BIT,
		shader_modules[0],
		"main",
		nullptr
	};
	pipeline_shaders_stage_create_infos[1] = {
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		nullptr,
		0,
		VK_SHADER_STAGE_COMPUTE_BIT,
		shader_modules[1],
		"main",
		nullptr
	};

	std::array<VkDescriptorSetLayout,2> descriptor_set_layouts = {gaussian_blur_descriptor_sets_layout[0], smaa_descriptor_sets_layout[3]};
	VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		descriptor_set_layouts.size(),
		descriptor_set_layouts.data(),
		0,
		nullptr
	};
	vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &gaussian_blur_xy_pipeline_layouts[0]);
	vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &gaussian_blur_xy_pipeline_layouts[1]);

	std::array<VkComputePipelineCreateInfo,2> compute_pipeline_create_infos;
	compute_pipeline_create_infos[0] = {
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		nullptr,
		0,
		pipeline_shaders_stage_create_infos[0],
		gaussian_blur_xy_pipeline_layouts[0],
		VK_NULL_HANDLE,
		-1
	};
	compute_pipeline_create_infos[1] = {
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		nullptr,
		0,
		pipeline_shaders_stage_create_infos[1],
		gaussian_blur_xy_pipeline_layouts[1],
		VK_NULL_HANDLE,
		-1
	};

	vkCreateComputePipelines(device, VK_NULL_HANDLE, compute_pipeline_create_infos.size(),
	 	compute_pipeline_create_infos.data(), nullptr, gaussian_blur_xy_pipelines.data() );

	vkDestroyShaderModule(device, shader_modules[0], nullptr);
	vkDestroyShaderModule(device, shader_modules[1], nullptr);
}

void VulkanSSAO::create_light_pipeline() {
	std::vector<uint8_t> shader_contents;
	vulkan_helper::get_binary_file_content("shaders//light.vert.spv", shader_contents);
	VkShaderModuleCreateInfo shader_module_create_info = {
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		nullptr,
		0,
		shader_contents.size(),
		reinterpret_cast<uint32_t*>(shader_contents.data())
	};
	VkShaderModule vertex_shader_module;
	if (vkCreateShaderModule(device, &shader_module_create_info, nullptr, &vertex_shader_module)) { throw SHADER_MODULE_CREATION_FAILED; }

	vulkan_helper::get_binary_file_content("shaders//light.frag.spv", shader_contents);
	shader_module_create_info.codeSize = shader_contents.size();
	shader_module_create_info.pCode = reinterpret_cast<uint32_t*>(shader_contents.data());
	VkShaderModule fragment_shader_module;
	if (vkCreateShaderModule(device, &shader_module_create_info, nullptr, &fragment_shader_module)) { throw SHADER_MODULE_CREATION_FAILED; }

	VkPipelineShaderStageCreateInfo pipeline_shaders_stage_create_info[2] = {
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_VERTEX_BIT,
			vertex_shader_module,
			"main",
			nullptr
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			fragment_shader_module,
			"main",
			nullptr
		}
	};

	VkVertexInputBindingDescription vertex_input_binding_description = {
		0,
		6 * sizeof(float),
		VK_VERTEX_INPUT_RATE_VERTEX
	};
	VkVertexInputAttributeDescription vertex_input_attribute_description[] = { {
		0,
		0,
		VK_FORMAT_R32G32B32_SFLOAT,
		0
	},
	{
		1,
		0,
		VK_FORMAT_R32G32B32_SFLOAT,
		3 * sizeof(float)
	}
	};
	VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		nullptr,
		0,
		1,
		&vertex_input_binding_description,
		2,
		vertex_input_attribute_description
	};

	VkPipelineInputAssemblyStateCreateInfo pipeline_input_assembly_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VK_FALSE
	};

	VkViewport viewport = {
		0.0f,
		0.0f,
		static_cast<float>(swapchain_create_info.imageExtent.width),
		static_cast<float>(swapchain_create_info.imageExtent.height),
		0.0f,
		1.0f
	};
	VkRect2D scissor = {
		{0,0},
		swapchain_create_info.imageExtent
	};
	VkPipelineViewportStateCreateInfo pipeline_viewport_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		nullptr,
		0,
		1,
		&viewport,
		1,
		&scissor
	};

	VkPipelineRasterizationStateCreateInfo pipeline_rasterization_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_FALSE,
		VK_FALSE,
		VK_POLYGON_MODE_FILL,
		VK_CULL_MODE_NONE,
		VK_FRONT_FACE_COUNTER_CLOCKWISE,
		VK_FALSE,
		0.0f,
		0.0f,
		0.0f,
		1.0f
	};

	VkPipelineMultisampleStateCreateInfo pipeline_multisample_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FALSE,
		1.0f,
		nullptr,
		VK_FALSE,
		VK_FALSE
	};

	VkPipelineDepthStencilStateCreateInfo pipeline_depth_stencil_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_TRUE,
		VK_TRUE,
		VK_COMPARE_OP_LESS,
		VK_FALSE,
		VK_FALSE,
		{},
		{},
		0.0f,
		1.0f
	};

	VkPipelineColorBlendAttachmentState pipeline_color_blend_attachment_state = {
		VK_FALSE,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_OP_ADD,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_OP_ADD,
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};
	VkPipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_FALSE,
		VK_LOGIC_OP_COPY,
		1,
		&pipeline_color_blend_attachment_state,
		{0.0f,0.0f,0.0f,0.0f}
	};

	std::array<VkDescriptorSetLayout,2> descriptor_set_layouts = { camera_light_sets_layout[0], camera_light_sets_layout[0] };
	VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		descriptor_set_layouts.size(),
		descriptor_set_layouts.data(),
		0,
		nullptr
	};
	vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &light_pipeline_layout);

	VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		nullptr,
		0,
		2,
		pipeline_shaders_stage_create_info,
		&pipeline_vertex_input_state_create_info,
		&pipeline_input_assembly_create_info,
		nullptr,
		&pipeline_viewport_state_create_info,
		&pipeline_rasterization_state_create_info,
		&pipeline_multisample_state_create_info,
		&pipeline_depth_stencil_state_create_info,
		&pipeline_color_blend_state_create_info,
		nullptr,
		light_pipeline_layout,
		pbr_render_pass,
		0,
		VK_NULL_HANDLE,
		-1
	};

	vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &light_pipeline);
	vkDestroyShaderModule(device, vertex_shader_module, nullptr);
	vkDestroyShaderModule(device, fragment_shader_module, nullptr);
}

void VulkanSSAO::create_pbr_pipeline() {
	std::vector<uint8_t> shader_contents;
	vulkan_helper::get_binary_file_content("shaders//pbr.vert.spv", shader_contents);
	VkShaderModuleCreateInfo shader_module_create_info = {
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		nullptr,
		0,
		shader_contents.size(),
		reinterpret_cast<uint32_t*>(shader_contents.data())
	};
	VkShaderModule vertex_shader_module;
	if (vkCreateShaderModule(device, &shader_module_create_info, nullptr, &vertex_shader_module)) { throw SHADER_MODULE_CREATION_FAILED; }

	vulkan_helper::get_binary_file_content("shaders//pbr.frag.spv", shader_contents);
	shader_module_create_info.codeSize = shader_contents.size();
	shader_module_create_info.pCode = reinterpret_cast<uint32_t*>(shader_contents.data());
	VkShaderModule fragment_shader_module;
	if (vkCreateShaderModule(device, &shader_module_create_info, nullptr, &fragment_shader_module)) { throw SHADER_MODULE_CREATION_FAILED; }

	VkPipelineShaderStageCreateInfo pipeline_shaders_stage_create_info[2] = {
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_VERTEX_BIT,
			vertex_shader_module,
			"main",
			nullptr
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			fragment_shader_module,
			"main",
			nullptr
		}
	};

	VkVertexInputBindingDescription vertex_input_binding_description = {
		0,
		12 * sizeof(float),
		VK_VERTEX_INPUT_RATE_VERTEX
	};
	VkVertexInputAttributeDescription vertex_input_attribute_description[] = { {
		0,
		0,
		VK_FORMAT_R32G32B32_SFLOAT,
		0
	},
	{
		1,
		0,
		VK_FORMAT_R32G32_SFLOAT,
		3 * sizeof(float)
	},
	{
		2,
		0,
		VK_FORMAT_R32G32B32_SFLOAT,
		5 * sizeof(float)
	},
	{
		3,
		0,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		8 * sizeof(float)
	}
	};
	VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		nullptr,
		0,
		1,
		&vertex_input_binding_description,
		4,
		vertex_input_attribute_description
	};

	VkPipelineInputAssemblyStateCreateInfo pipeline_input_assembly_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VK_FALSE
	};

	VkViewport viewport = {
		0.0f,
		0.0f,
		static_cast<float>(swapchain_create_info.imageExtent.width),
		static_cast<float>(swapchain_create_info.imageExtent.height),
		0.0f,
		1.0f
	};
	VkRect2D scissor = {
		{0,0},
		swapchain_create_info.imageExtent
	};
	VkPipelineViewportStateCreateInfo pipeline_viewport_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		nullptr,
		0,
		1,
		&viewport,
		1,
		&scissor
	};

	VkPipelineRasterizationStateCreateInfo pipeline_rasterization_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_FALSE,
		VK_FALSE,
		VK_POLYGON_MODE_FILL,
		VK_CULL_MODE_NONE,
		VK_FRONT_FACE_COUNTER_CLOCKWISE,
		VK_FALSE,
		0.0f,
		0.0f,
		0.0f,
		1.0f
	};

	VkPipelineMultisampleStateCreateInfo pipeline_multisample_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FALSE,
		1.0f,
		nullptr,
		VK_FALSE,
		VK_FALSE
	};

	VkPipelineDepthStencilStateCreateInfo pipeline_depth_stencil_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_TRUE,
		VK_TRUE,
		VK_COMPARE_OP_LESS,
		VK_FALSE,
		VK_FALSE,
		{},
		{},
		0.0f,
		1.0f
	};

	VkPipelineColorBlendAttachmentState pipeline_color_blend_attachment_state = {
		VK_FALSE,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_OP_ADD,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_OP_ADD,
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};
	VkPipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_FALSE,
		VK_LOGIC_OP_COPY,
		1,
		&pipeline_color_blend_attachment_state,
		{0.0f,0.0f,0.0f,0.0f}
	};

	std::array<VkDescriptorSetLayout,3> descriptor_set_layouts = {pbr_descriptor_sets_layout[0], camera_light_sets_layout[0], camera_light_sets_layout[0]};
	VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		descriptor_set_layouts.size(),
		descriptor_set_layouts.data(),
		0,
		nullptr
	};
	vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &pbr_pipeline_layout);

	VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		nullptr,
		0,
		2,
		pipeline_shaders_stage_create_info,
		&pipeline_vertex_input_state_create_info,
		&pipeline_input_assembly_create_info,
		nullptr,
		&pipeline_viewport_state_create_info,
		&pipeline_rasterization_state_create_info,
		&pipeline_multisample_state_create_info,
		&pipeline_depth_stencil_state_create_info,
		&pipeline_color_blend_state_create_info,
		nullptr,
		pbr_pipeline_layout,
		pbr_render_pass,
		0,
		VK_NULL_HANDLE,
		-1
	};
	vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &pbr_pipeline);
	vkDestroyShaderModule(device, vertex_shader_module, nullptr);
	vkDestroyShaderModule(device, fragment_shader_module, nullptr);
}

void VulkanSSAO::create_smaa_edge_pipeline() {
	std::vector<uint8_t> shader_contents;
	vulkan_helper::get_binary_file_content("shaders//smaa_edge.vert.spv", shader_contents);
	VkShaderModuleCreateInfo shader_module_create_info = {
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		nullptr,
		0,
		shader_contents.size(),
		reinterpret_cast<uint32_t*>(shader_contents.data())
	};
	VkShaderModule vertex_shader_module;
	if (vkCreateShaderModule(device, &shader_module_create_info, nullptr, &vertex_shader_module)) { throw SHADER_MODULE_CREATION_FAILED; }

	vulkan_helper::get_binary_file_content("shaders//smaa_edge.frag.spv", shader_contents);
	shader_module_create_info.codeSize = shader_contents.size();
	shader_module_create_info.pCode = reinterpret_cast<uint32_t*>(shader_contents.data());
	VkShaderModule fragment_shader_module;
	if (vkCreateShaderModule(device, &shader_module_create_info, nullptr, &fragment_shader_module)) { throw SHADER_MODULE_CREATION_FAILED; }

	VkPipelineShaderStageCreateInfo pipeline_shaders_stage_create_info[2] = {
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_VERTEX_BIT,
			vertex_shader_module,
			"main",
			nullptr
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			fragment_shader_module,
			"main",
			nullptr
		}
	};

	VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		nullptr,
		0,
		0,
		nullptr,
		0,
		nullptr
	};

	VkPipelineInputAssemblyStateCreateInfo pipeline_input_assembly_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VK_FALSE
	};

	VkViewport viewport = {
		0.0f,
		0.0f,
		static_cast<float>(swapchain_create_info.imageExtent.width),
		static_cast<float>(swapchain_create_info.imageExtent.height),
		0.0f,
		1.0f
	};
	VkRect2D scissor = {
		{0,0},
		swapchain_create_info.imageExtent
	};
	VkPipelineViewportStateCreateInfo pipeline_viewport_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		nullptr,
		0,
		1,
		&viewport,
		1,
		&scissor
	};

	VkPipelineRasterizationStateCreateInfo pipeline_rasterization_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_FALSE,
		VK_FALSE,
		VK_POLYGON_MODE_FILL,
		VK_CULL_MODE_FRONT_BIT,
		VK_FRONT_FACE_COUNTER_CLOCKWISE,
		VK_FALSE,
		0.0f,
		0.0f,
		0.0f,
		1.0f
	};

	VkPipelineMultisampleStateCreateInfo pipeline_multisample_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FALSE,
		1.0f,
		nullptr,
		VK_FALSE,
		VK_FALSE
	};

	VkPipelineDepthStencilStateCreateInfo pipeline_depth_stencil_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_FALSE,
		VK_FALSE,
		VK_COMPARE_OP_LESS,
		VK_FALSE,
		VK_TRUE,
		{VK_STENCIL_OP_ZERO, VK_STENCIL_OP_INCREMENT_AND_CLAMP, VK_STENCIL_OP_ZERO, VK_COMPARE_OP_GREATER, 0xff, 0xff, 1},
		{VK_STENCIL_OP_ZERO, VK_STENCIL_OP_INCREMENT_AND_CLAMP, VK_STENCIL_OP_ZERO, VK_COMPARE_OP_GREATER, 0xff, 0xff, 1},
		0.0f,
		1.0f
	};

	VkPipelineColorBlendAttachmentState pipeline_color_blend_attachment_state = {
		VK_FALSE,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_OP_ADD,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_OP_ADD,
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};
	VkPipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_FALSE,
		VK_LOGIC_OP_COPY,
		1,
		&pipeline_color_blend_attachment_state,
		{0.0f,0.0f,0.0f,0.0f}
	};

	std::array<VkDescriptorSetLayout,2> smaa_edge_descriptor_set_layouts = { smaa_descriptor_sets_layout[0], smaa_descriptor_sets_layout[3] };
	VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		smaa_edge_descriptor_set_layouts.size(),
		smaa_edge_descriptor_set_layouts.data(),
		0,
		nullptr
	};
	vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &smaa_edge_pipeline_layout);

	VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		nullptr,
		0,
		2,
		pipeline_shaders_stage_create_info,
		&pipeline_vertex_input_state_create_info,
		&pipeline_input_assembly_create_info,
		nullptr,
		&pipeline_viewport_state_create_info,
		&pipeline_rasterization_state_create_info,
		&pipeline_multisample_state_create_info,
		&pipeline_depth_stencil_state_create_info,
		&pipeline_color_blend_state_create_info,
		nullptr,
		smaa_edge_pipeline_layout,
		smaa_edge_renderpass,
		0,
		VK_NULL_HANDLE,
		-1
	};
	vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &smaa_edge_pipeline);
	vkDestroyShaderModule(device, vertex_shader_module, nullptr);
	vkDestroyShaderModule(device, fragment_shader_module, nullptr);
}

void VulkanSSAO::create_smaa_weight_pipeline() {
	std::vector<uint8_t> shader_contents;
	vulkan_helper::get_binary_file_content("shaders//smaa_weight.vert.spv", shader_contents);
	VkShaderModuleCreateInfo shader_module_create_info = {
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		nullptr,
		0,
		shader_contents.size(),
		reinterpret_cast<uint32_t*>(shader_contents.data())
	};
	VkShaderModule vertex_shader_module;
	if (vkCreateShaderModule(device, &shader_module_create_info, nullptr, &vertex_shader_module)) { throw SHADER_MODULE_CREATION_FAILED; }

	vulkan_helper::get_binary_file_content("shaders//smaa_weight.frag.spv", shader_contents);
	shader_module_create_info.codeSize = shader_contents.size();
	shader_module_create_info.pCode = reinterpret_cast<uint32_t*>(shader_contents.data());
	VkShaderModule fragment_shader_module;
	if (vkCreateShaderModule(device, &shader_module_create_info, nullptr, &fragment_shader_module)) { throw SHADER_MODULE_CREATION_FAILED; }

	VkPipelineShaderStageCreateInfo pipeline_shaders_stage_create_info[2] = {
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_VERTEX_BIT,
			vertex_shader_module,
			"main",
			nullptr
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			fragment_shader_module,
			"main",
			nullptr
		}
	};

	VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		nullptr,
		0,
		0,
		nullptr,
		0,
		nullptr
	};

	VkPipelineInputAssemblyStateCreateInfo pipeline_input_assembly_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VK_FALSE
	};

	VkViewport viewport = {
		0.0f,
		0.0f,
		static_cast<float>(swapchain_create_info.imageExtent.width),
		static_cast<float>(swapchain_create_info.imageExtent.height),
		0.0f,
		1.0f
	};
	VkRect2D scissor = {
		{0,0},
		swapchain_create_info.imageExtent
	};
	VkPipelineViewportStateCreateInfo pipeline_viewport_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		nullptr,
		0,
		1,
		&viewport,
		1,
		&scissor
	};

	VkPipelineRasterizationStateCreateInfo pipeline_rasterization_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_FALSE,
		VK_FALSE,
		VK_POLYGON_MODE_FILL,
		VK_CULL_MODE_FRONT_BIT,
		VK_FRONT_FACE_COUNTER_CLOCKWISE,
		VK_FALSE,
		0.0f,
		0.0f,
		0.0f,
		1.0f
	};

	VkPipelineMultisampleStateCreateInfo pipeline_multisample_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FALSE,
		1.0f,
		nullptr,
		VK_FALSE,
		VK_FALSE
	};

	VkPipelineDepthStencilStateCreateInfo pipeline_depth_stencil_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_FALSE,
		VK_FALSE,
		VK_COMPARE_OP_LESS,
		VK_FALSE,
		VK_TRUE,
		{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_EQUAL, 0xff, 0xff, 1 },
		{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_EQUAL, 0xff, 0xff, 1 },
		0.0f,
		1.0f
	};

	VkPipelineColorBlendAttachmentState pipeline_color_blend_attachment_state = {
		VK_FALSE,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_OP_ADD,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_OP_ADD,
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};
	VkPipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_FALSE,
		VK_LOGIC_OP_COPY,
		1,
		&pipeline_color_blend_attachment_state,
		{0.0f,0.0f,0.0f,0.0f}
	};

	std::array<VkDescriptorSetLayout, 2> smaa_weight_descriptor_set_layouts = { smaa_descriptor_sets_layout[1], smaa_descriptor_sets_layout[3] };
	VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		smaa_weight_descriptor_set_layouts.size(),
		smaa_weight_descriptor_set_layouts.data(),
		0,
		nullptr
	};
	vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &smaa_weight_pipeline_layout);

	VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		nullptr,
		0,
		2,
		pipeline_shaders_stage_create_info,
		&pipeline_vertex_input_state_create_info,
		&pipeline_input_assembly_create_info,
		nullptr,
		&pipeline_viewport_state_create_info,
		&pipeline_rasterization_state_create_info,
		&pipeline_multisample_state_create_info,
		&pipeline_depth_stencil_state_create_info,
		&pipeline_color_blend_state_create_info,
		nullptr,
		smaa_weight_pipeline_layout,
		smaa_weight_renderpass,
		0,
		VK_NULL_HANDLE,
		-1
	};
	vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &smaa_weight_pipeline);
	vkDestroyShaderModule(device, vertex_shader_module, nullptr);
	vkDestroyShaderModule(device, fragment_shader_module, nullptr);
}

void VulkanSSAO::create_smaa_blend_pipeline() {
	std::vector<uint8_t> shader_contents;
	vulkan_helper::get_binary_file_content("shaders//smaa_blend.vert.spv", shader_contents);
	VkShaderModuleCreateInfo shader_module_create_info = {
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		nullptr,
		0,
		shader_contents.size(),
		reinterpret_cast<uint32_t*>(shader_contents.data())
	};
	VkShaderModule vertex_shader_module;
	if (vkCreateShaderModule(device, &shader_module_create_info, nullptr, &vertex_shader_module)) { throw SHADER_MODULE_CREATION_FAILED; }

	vulkan_helper::get_binary_file_content("shaders//smaa_blend.frag.spv", shader_contents);
	shader_module_create_info.codeSize = shader_contents.size();
	shader_module_create_info.pCode = reinterpret_cast<uint32_t*>(shader_contents.data());
	VkShaderModule fragment_shader_module;
	if (vkCreateShaderModule(device, &shader_module_create_info, nullptr, &fragment_shader_module)) { throw SHADER_MODULE_CREATION_FAILED; }

	VkPipelineShaderStageCreateInfo pipeline_shaders_stage_create_info[2] = {
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_VERTEX_BIT,
			vertex_shader_module,
			"main",
			nullptr
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			fragment_shader_module,
			"main",
			nullptr
		}
	};

	VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		nullptr,
		0,
		0,
		nullptr,
		0,
		nullptr
	};

	VkPipelineInputAssemblyStateCreateInfo pipeline_input_assembly_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VK_FALSE
	};

	VkViewport viewport = {
		0.0f,
		0.0f,
		static_cast<float>(swapchain_create_info.imageExtent.width),
		static_cast<float>(swapchain_create_info.imageExtent.height),
		0.0f,
		1.0f
	};
	VkRect2D scissor = {
		{0,0},
		swapchain_create_info.imageExtent
	};
	VkPipelineViewportStateCreateInfo pipeline_viewport_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		nullptr,
		0,
		1,
		&viewport,
		1,
		&scissor
	};

	VkPipelineRasterizationStateCreateInfo pipeline_rasterization_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_FALSE,
		VK_FALSE,
		VK_POLYGON_MODE_FILL,
		VK_CULL_MODE_FRONT_BIT,
		VK_FRONT_FACE_COUNTER_CLOCKWISE,
		VK_FALSE,
		0.0f,
		0.0f,
		0.0f,
		1.0f
	};

	VkPipelineMultisampleStateCreateInfo pipeline_multisample_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FALSE,
		1.0f,
		nullptr,
		VK_FALSE,
		VK_FALSE
	};

	VkPipelineDepthStencilStateCreateInfo pipeline_depth_stencil_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_FALSE,
		VK_FALSE,
		VK_COMPARE_OP_LESS,
		VK_FALSE,
		VK_FALSE,
		{},
		{},
		0.0f,
		1.0f
	};

	VkPipelineColorBlendAttachmentState pipeline_color_blend_attachment_state = {
		VK_FALSE,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_OP_ADD,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_OP_ADD,
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};
	VkPipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_FALSE,
		VK_LOGIC_OP_COPY,
		1,
		&pipeline_color_blend_attachment_state,
		{0.0f,0.0f,0.0f,0.0f}
	};

	std::array<VkDescriptorSetLayout, 2> smaa_blend_descriptor_set_layouts = { smaa_descriptor_sets_layout[2], smaa_descriptor_sets_layout[3] };
	VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		smaa_blend_descriptor_set_layouts.size(),
		smaa_blend_descriptor_set_layouts.data(),
		0,
		nullptr
	};
	vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &smaa_blend_pipeline_layout);

	VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		nullptr,
		0,
		2,
		pipeline_shaders_stage_create_info,
		&pipeline_vertex_input_state_create_info,
		&pipeline_input_assembly_create_info,
		nullptr,
		&pipeline_viewport_state_create_info,
		&pipeline_rasterization_state_create_info,
		&pipeline_multisample_state_create_info,
		&pipeline_depth_stencil_state_create_info,
		&pipeline_color_blend_state_create_info,
		nullptr,
		smaa_blend_pipeline_layout,
		smaa_blend_renderpass,
		0,
		VK_NULL_HANDLE,
		-1
	};
	vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &smaa_blend_pipeline);
	vkDestroyShaderModule(device, vertex_shader_module, nullptr);
	vkDestroyShaderModule(device, fragment_shader_module, nullptr);
}

void VulkanSSAO::create_hdr_tonemap_pipeline() {
	std::vector<uint8_t> shader_contents;
	vulkan_helper::get_binary_file_content("shaders//tonemap.vert.spv", shader_contents);
	VkShaderModuleCreateInfo shader_module_create_info = {
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		nullptr,
		0,
		shader_contents.size(),
		reinterpret_cast<uint32_t*>(shader_contents.data())
	};
	VkShaderModule vertex_shader_module;
	if (vkCreateShaderModule(device, &shader_module_create_info, nullptr, &vertex_shader_module)) { throw SHADER_MODULE_CREATION_FAILED; }

	vulkan_helper::get_binary_file_content("shaders//tonemap.frag.spv", shader_contents);
	shader_module_create_info.codeSize = shader_contents.size();
	shader_module_create_info.pCode = reinterpret_cast<uint32_t*>(shader_contents.data());
	VkShaderModule fragment_shader_module;
	if (vkCreateShaderModule(device, &shader_module_create_info, nullptr, &fragment_shader_module)) { throw SHADER_MODULE_CREATION_FAILED; }

	VkPipelineShaderStageCreateInfo pipeline_shaders_stage_create_info[2] = {
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_VERTEX_BIT,
			vertex_shader_module,
			"main",
			nullptr
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			fragment_shader_module,
			"main",
			nullptr
		}
	};

	VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		nullptr,
		0,
		0,
		nullptr,
		0,
		nullptr
	};

	VkPipelineInputAssemblyStateCreateInfo pipeline_input_assembly_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VK_FALSE
	};

	VkViewport viewport = {
		0.0f,
		0.0f,
		static_cast<float>(swapchain_create_info.imageExtent.width),
		static_cast<float>(swapchain_create_info.imageExtent.height),
		0.0f,
		1.0f
	};
	VkRect2D scissor = {
		{0,0},
		swapchain_create_info.imageExtent
	};
	VkPipelineViewportStateCreateInfo pipeline_viewport_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		nullptr,
		0,
		1,
		&viewport,
		1,
		&scissor
	};

	VkPipelineRasterizationStateCreateInfo pipeline_rasterization_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_FALSE,
		VK_FALSE,
		VK_POLYGON_MODE_FILL,
		VK_CULL_MODE_FRONT_BIT,
		VK_FRONT_FACE_COUNTER_CLOCKWISE,
		VK_FALSE,
		0.0f,
		0.0f,
		0.0f,
		1.0f
	};

	VkPipelineMultisampleStateCreateInfo pipeline_multisample_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FALSE,
		1.0f,
		nullptr,
		VK_FALSE,
		VK_FALSE
	};

	VkPipelineDepthStencilStateCreateInfo pipeline_depth_stencil_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_FALSE,
		VK_FALSE,
		VK_COMPARE_OP_LESS,
		VK_FALSE,
		VK_FALSE,
		{},
		{},
		0.0f,
		1.0f
	};

	VkPipelineColorBlendAttachmentState pipeline_color_blend_attachment_state = {
		VK_FALSE,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_OP_ADD,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_OP_ADD,
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};
	VkPipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_FALSE,
		VK_LOGIC_OP_COPY,
		1,
		&pipeline_color_blend_attachment_state,
		{0.0f,0.0f,0.0f,0.0f}
	};

	std::array<VkDescriptorSetLayout,2> descriptor_sets_layout = { hdr_descriptor_sets_layout[0], smaa_descriptor_sets_layout[3] };
	VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		descriptor_sets_layout.size(),
		descriptor_sets_layout.data(),
		0,
		nullptr
	};
	vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &hdr_tonemap_pipeline_layout);

	VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		nullptr,
		0,
		2,
		pipeline_shaders_stage_create_info,
		&pipeline_vertex_input_state_create_info,
		&pipeline_input_assembly_create_info,
		nullptr,
		&pipeline_viewport_state_create_info,
		&pipeline_rasterization_state_create_info,
		&pipeline_multisample_state_create_info,
		&pipeline_depth_stencil_state_create_info,
		&pipeline_color_blend_state_create_info,
		nullptr,
		hdr_tonemap_pipeline_layout,
		hdr_tonemap_renderpass,
		0,
		VK_NULL_HANDLE,
		-1
	};
	vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &hdr_tonemap_pipeline);
	vkDestroyShaderModule(device, vertex_shader_module, nullptr);
	vkDestroyShaderModule(device, fragment_shader_module, nullptr);
}

void VulkanSSAO::upload_input_data() {
	VkCommandBufferBeginInfo command_buffer_begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,nullptr };
	vkBeginCommandBuffer(command_buffers[0], &command_buffer_begin_info);

	std::array <VkBufferCopy, 4> buffer_copies { {
	{ 
		0,0,
		water_bottle_model_data.interleaved_vertex_data_size + water_bottle_model_data.index_data_size
	},
	{
		vulkan_helper::get_model_data_total_size(water_bottle_model_data),
		water_bottle_model_data.interleaved_vertex_data_size + water_bottle_model_data.index_data_size,
		box_model_data.interleaved_vertex_data_size + box_model_data.index_data_size
	},


	{
		vulkan_helper::get_model_data_total_size(water_bottle_model_data) + vulkan_helper::get_model_data_total_size(box_model_data),
		water_bottle_model_data.interleaved_vertex_data_size + water_bottle_model_data.index_data_size +
		box_model_data.interleaved_vertex_data_size + box_model_data.index_data_size,
		table_model_data.interleaved_vertex_data_size + table_model_data.index_data_size
	},
	{
		vulkan_helper::get_model_data_total_size(water_bottle_model_data) + vulkan_helper::get_model_data_total_size(box_model_data)
		+ vulkan_helper::get_model_data_total_size(table_model_data),
		water_bottle_model_data.interleaved_vertex_data_size + water_bottle_model_data.index_data_size +
		box_model_data.interleaved_vertex_data_size + box_model_data.index_data_size +
		table_model_data.interleaved_vertex_data_size + table_model_data.index_data_size,
		sphere_model_data.interleaved_vertex_data_size + sphere_model_data.index_data_size
	}}};
	
	vkCmdCopyBuffer(command_buffers[0], host_vertex_buffer, device_vertex_buffer, buffer_copies.size(), buffer_copies.data());

	VkImageMemoryBarrier image_memory_barrier[5] = { {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		nullptr,
		0,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED,
		VK_QUEUE_FAMILY_IGNORED,
		device_water_bottle_image,
		{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,4}
	},
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		nullptr,
		0,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED,
		VK_QUEUE_FAMILY_IGNORED,
		device_box_image,
		{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,4}
	},
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		nullptr,
		0,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED,
		VK_QUEUE_FAMILY_IGNORED,
		device_table_image,
		{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,4}
	},
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		nullptr,
		0,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED,
		VK_QUEUE_FAMILY_IGNORED,
		device_smaa_area_image,
		{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}
	},
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		nullptr,
		0,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED,
		VK_QUEUE_FAMILY_IGNORED,
		device_smaa_search_image,
		{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}
	} };
	vkCmdPipelineBarrier(command_buffers[0], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 5, image_memory_barrier);

	VkBufferImageCopy buffer_image_copy = {
		water_bottle_model_data.host_image_data_offset,
		0,
		0,
		{VK_IMAGE_ASPECT_COLOR_BIT,0,0,4},
		{0,0,0},
		water_bottle_model_data.image_size
	};
	vkCmdCopyBufferToImage(command_buffers[0], host_vertex_buffer, device_water_bottle_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffer_image_copy);

	buffer_image_copy = {
		box_model_data.host_image_data_offset,
		0,
		0,
		{VK_IMAGE_ASPECT_COLOR_BIT,0,0,4},
		{0,0,0},
		box_model_data.image_size
	};
	vkCmdCopyBufferToImage(command_buffers[0], host_vertex_buffer, device_box_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffer_image_copy);

	buffer_image_copy = {
		table_model_data.host_image_data_offset,
		0,
		0,
		{VK_IMAGE_ASPECT_COLOR_BIT,0,0,4},
		{0,0,0},
		table_model_data.image_size
	};
	vkCmdCopyBufferToImage(command_buffers[0], host_vertex_buffer, device_table_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffer_image_copy);

	buffer_image_copy = {
		sphere_model_data.host_index_data_offset + sphere_model_data.index_data_size,
		0,
		0,
		{VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},
		{0,0,0},
		{160,560,1}
	};
	vkCmdCopyBufferToImage(command_buffers[0], host_vertex_buffer, device_smaa_area_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffer_image_copy);

	buffer_image_copy = {
		sphere_model_data.host_index_data_offset + sphere_model_data.index_data_size + area_tex_size,
		0,
		0,
		{VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},
		{0,0,0},
		{ 64,16,1 }
	};
	vkCmdCopyBufferToImage(command_buffers[0], host_vertex_buffer, device_smaa_search_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffer_image_copy);

	image_memory_barrier[0] = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		nullptr,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED,
		VK_QUEUE_FAMILY_IGNORED,
		device_water_bottle_image,
		{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,4}
	};
	image_memory_barrier[1] = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		nullptr,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED,
		VK_QUEUE_FAMILY_IGNORED,
		device_box_image,
		{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,4}
	};
	image_memory_barrier[2] = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		nullptr,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED,
		VK_QUEUE_FAMILY_IGNORED,
		device_table_image,
		{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,4}
	};
	image_memory_barrier[3] = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		nullptr,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED,
		VK_QUEUE_FAMILY_IGNORED,
		device_smaa_area_image,
		{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}
	};
	image_memory_barrier[4] = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		nullptr,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_QUEUE_FAMILY_IGNORED,
		VK_QUEUE_FAMILY_IGNORED,
		device_smaa_search_image,
		{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}
	};
	vkCmdPipelineBarrier(command_buffers[0], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 5, image_memory_barrier);

	vkEndCommandBuffer(command_buffers[0]);

	VkFenceCreateInfo fence_create_info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,nullptr,0 };
	VkFence fence;
	vkCreateFence(device, &fence_create_info, nullptr, &fence);

	VkPipelineStageFlags pipeline_stage_flags = { VK_PIPELINE_STAGE_TRANSFER_BIT };
	VkSubmitInfo submit_info = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO,
		nullptr,
		0,
		nullptr,
		&pipeline_stage_flags,
		1,
		&command_buffers[0],
		0,
		nullptr
	};
	vkQueueSubmit(queue, 1, &submit_info, fence);

	vkDeviceWaitIdle(device);
	vkWaitForFences(device, 1, &fence, VK_TRUE, 20000000);
	vkResetCommandPool(device, command_pool, 0);
	vkDestroyFence(device, fence, nullptr);
}

void VulkanSSAO::record_command_buffers() {
	VkClearValue clear_colors[3];
	clear_colors[0].depthStencil = { 1.0f, 0 };
	clear_colors[1].color = { 0.0f, 0.0f, 0.0f, 0.0f };
	clear_colors[2].color = { 1.0f, 1.0f, 1.0f, 1.0f };

	for (uint32_t i = 0; i < swapchain_images_count; i++) {
		VkCommandBufferBeginInfo command_buffer_begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, nullptr };
		vkBeginCommandBuffer(command_buffers[i], &command_buffer_begin_info);

		// Copying uniform buffer data for the pbr shader
		VkBufferCopy buffer_copy = { 0,0,256*5+16};
		vkCmdCopyBuffer(command_buffers[i], host_uniform_buffer, device_uniform_buffer, 1, &buffer_copy);
		
		// Transitioning layout from the write to the shader read
		VkBufferMemoryBarrier buffer_memory_barrier = {
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			nullptr,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_UNIFORM_READ_BIT,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			device_uniform_buffer,
			0,
			VK_WHOLE_SIZE
		};
		vkCmdPipelineBarrier(command_buffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0, 0, nullptr, 1, &buffer_memory_barrier, 0, nullptr);
		
		// shadow_map Renderpass Start
		std::array<VkClearValue,2> shadow_map_clear_colors = {clear_colors[0], clear_colors[2]};
		VkRenderPassBeginInfo render_pass_begin_info = {
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			nullptr,
			shadow_map_render_pass,
			framebuffers[0],
			{{0,0},{swapchain_create_info.imageExtent}},
			shadow_map_clear_colors.size(),
			shadow_map_clear_colors.data()
		};
		vkCmdBeginRenderPass(command_buffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, shadow_map_pipeline);
		vkCmdBindDescriptorSets(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, shadow_map_pipeline_layout, 0, 2,
			std::array<VkDescriptorSet,2> {pbr_descriptor_sets[0], camera_light_sets[1]}.data(), 0, nullptr);
		VkDeviceSize offset = water_bottle_model_data.device_interleaved_vertex_data_offset;
		vkCmdBindVertexBuffers(command_buffers[i], 0, 1, &device_vertex_buffer, &offset);
		vkCmdBindIndexBuffer(command_buffers[i], device_vertex_buffer, water_bottle_model_data.device_index_data_offset, VK_INDEX_TYPE_UINT16);
		vkCmdDrawIndexed(command_buffers[i], water_bottle_model_data.indices, 1, 0, 0, 0);

		vkCmdBindDescriptorSets(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, shadow_map_pipeline_layout, 0, 1, &pbr_descriptor_sets[1], 0, nullptr);
		offset = box_model_data.device_interleaved_vertex_data_offset;
		vkCmdBindVertexBuffers(command_buffers[i], 0, 1, &device_vertex_buffer, &offset);
		vkCmdBindIndexBuffer(command_buffers[i], device_vertex_buffer, box_model_data.device_index_data_offset, VK_INDEX_TYPE_UINT16);
		vkCmdDrawIndexed(command_buffers[i], box_model_data.indices, 1, 0, 0, 0);

		vkCmdBindDescriptorSets(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, shadow_map_pipeline_layout, 0, 1, &pbr_descriptor_sets[2], 0, nullptr);
		offset = table_model_data.device_interleaved_vertex_data_offset;
		vkCmdBindVertexBuffers(command_buffers[i], 0, 1, &device_vertex_buffer, &offset);
		vkCmdBindIndexBuffer(command_buffers[i], device_vertex_buffer, table_model_data.device_index_data_offset, VK_INDEX_TYPE_UINT16);
		vkCmdDrawIndexed(command_buffers[i], table_model_data.indices, 1, 0, 0, 0);

		// shadow_map Renderpass End
		vkCmdEndRenderPass(command_buffers[i]);

		// Transitioning the rg32 shadow map layout from unknown to general (for writing)
		std::array<VkImageMemoryBarrier,2> image_memory_barriers;
		image_memory_barriers[0] = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			nullptr,
			0,
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			device_vsm_depth_image,
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 1, 1 }
		};
		vkCmdPipelineBarrier(command_buffers[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 1, image_memory_barriers.data());

		vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, gaussian_blur_xy_pipelines[0]);
		std::array<VkDescriptorSet, 2> gaussian_blur_pipeline_descriptor_sets = { gaussian_blur_descriptor_sets[0], smaa_descriptor_sets[3] };
		vkCmdBindDescriptorSets(command_buffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, gaussian_blur_xy_pipeline_layouts[0],
			0, 2, gaussian_blur_pipeline_descriptor_sets.data(), 0, nullptr);
		vkCmdDispatch(command_buffers[i],std::ceil(swapchain_create_info.imageExtent.width/32.0f), std::ceil(swapchain_create_info.imageExtent.height/32.0f), 1);

		image_memory_barriers[0] = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			nullptr,
			VK_ACCESS_SHADER_READ_BIT,
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			device_vsm_depth_image,
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 1 }
		};
		image_memory_barriers[1] = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			nullptr,
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			device_vsm_depth_image,
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 1, 1 }
		};
		vkCmdPipelineBarrier(command_buffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 2, image_memory_barriers.data());

		vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, gaussian_blur_xy_pipelines[1]);
		gaussian_blur_pipeline_descriptor_sets = { gaussian_blur_descriptor_sets[1], smaa_descriptor_sets[3] };
		vkCmdBindDescriptorSets(command_buffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, gaussian_blur_xy_pipeline_layouts[1],
			0, 2, gaussian_blur_pipeline_descriptor_sets.data(), 0, nullptr);
		vkCmdDispatch(command_buffers[i],std::ceil(swapchain_create_info.imageExtent.width/32.0f), std::ceil(swapchain_create_info.imageExtent.height/32.0f), 1);

		image_memory_barriers[0] = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			nullptr,
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			device_vsm_depth_image,
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 1 }
		};
		vkCmdPipelineBarrier(command_buffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 1, image_memory_barriers.data());

		// PBR Renderpass Start
		render_pass_begin_info = {
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			nullptr,
			pbr_render_pass,
			framebuffers[1],
			{{0,0},{swapchain_create_info.imageExtent}},
			2,
			clear_colors
		};
		vkCmdBeginRenderPass(command_buffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pbr_pipeline);
		vkCmdBindDescriptorSets(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pbr_pipeline_layout, 0, 3,
			std::array<VkDescriptorSet,3> {pbr_descriptor_sets[0], camera_light_sets[0], camera_light_sets[1]}.data(), 0, nullptr);
		offset = water_bottle_model_data.device_interleaved_vertex_data_offset;
		vkCmdBindVertexBuffers(command_buffers[i], 0, 1, &device_vertex_buffer, &offset);
		vkCmdBindIndexBuffer(command_buffers[i], device_vertex_buffer, water_bottle_model_data.device_index_data_offset, VK_INDEX_TYPE_UINT16);
		vkCmdDrawIndexed(command_buffers[i], water_bottle_model_data.indices, 1, 0, 0, 0);

		vkCmdBindDescriptorSets(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pbr_pipeline_layout, 0, 1, &pbr_descriptor_sets[1], 0, nullptr);
		offset = box_model_data.device_interleaved_vertex_data_offset;
		vkCmdBindVertexBuffers(command_buffers[i], 0, 1, &device_vertex_buffer, &offset);
		vkCmdBindIndexBuffer(command_buffers[i], device_vertex_buffer, box_model_data.device_index_data_offset, VK_INDEX_TYPE_UINT16);
		vkCmdDrawIndexed(command_buffers[i], box_model_data.indices, 1, 0, 0, 0);

		vkCmdBindDescriptorSets(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pbr_pipeline_layout, 0, 1, &pbr_descriptor_sets[2], 0, nullptr);
		offset = table_model_data.device_interleaved_vertex_data_offset;
		vkCmdBindVertexBuffers(command_buffers[i], 0, 1, &device_vertex_buffer, &offset);
		vkCmdBindIndexBuffer(command_buffers[i], device_vertex_buffer, table_model_data.device_index_data_offset, VK_INDEX_TYPE_UINT16);
		vkCmdDrawIndexed(command_buffers[i], table_model_data.indices, 1, 0, 0, 0);

		vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, light_pipeline);
		vkCmdBindDescriptorSets(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, light_pipeline_layout, 0, 2,
			std::array<VkDescriptorSet,2> {camera_light_sets[0], camera_light_sets[1]}.data(), 0, nullptr);
		offset = sphere_model_data.device_interleaved_vertex_data_offset;
		vkCmdBindVertexBuffers(command_buffers[i], 0, 1, &device_vertex_buffer, &offset);
		vkCmdBindIndexBuffer(command_buffers[i], device_vertex_buffer, sphere_model_data.device_index_data_offset, VK_INDEX_TYPE_UINT16);
		vkCmdDrawIndexed(command_buffers[i], sphere_model_data.indices, 1, 0, 0, 0);

		// PBR Renderpass end
		vkCmdEndRenderPass(command_buffers[i]);

		// Smaa edge renderpass start
		render_pass_begin_info = {
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			nullptr,
			smaa_edge_renderpass,
			framebuffers[2],
			{{0,0},{swapchain_create_info.imageExtent}},
			2,
			&clear_colors[0]
		};
		vkCmdBeginRenderPass(command_buffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, smaa_edge_pipeline);
		std::array<VkDescriptorSet, 2> bind_smaa_descriptor_sets = { smaa_descriptor_sets[0], smaa_descriptor_sets[3] };
		vkCmdBindDescriptorSets(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, smaa_edge_pipeline_layout, 0, 2, bind_smaa_descriptor_sets.data(), 0, nullptr);
		vkCmdDraw(command_buffers[i], 3, 1, 0, 0);

		// Smaa edge renderpass end
		vkCmdEndRenderPass(command_buffers[i]);

		// Smaa weight renderpass start
		render_pass_begin_info = {
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			nullptr,
			smaa_weight_renderpass,
			framebuffers[3],
			{{0,0},{swapchain_create_info.imageExtent}},
			2,
			&clear_colors[0]
		};
		vkCmdBeginRenderPass(command_buffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, smaa_weight_pipeline); 
		bind_smaa_descriptor_sets = { smaa_descriptor_sets[1], smaa_descriptor_sets[3] };
		vkCmdBindDescriptorSets(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, smaa_weight_pipeline_layout, 0, 2, bind_smaa_descriptor_sets.data(), 0, nullptr);
		vkCmdDraw(command_buffers[i], 3, 1, 0, 0);

		// Smaa weight renderpass end
		vkCmdEndRenderPass(command_buffers[i]);

		// Smaa blend renderpass start
		render_pass_begin_info = {
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			nullptr,
			smaa_blend_renderpass,
			framebuffers[4],
			{{0,0},{swapchain_create_info.imageExtent}},
			1,
			&clear_colors[1]
		};
		vkCmdBeginRenderPass(command_buffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, smaa_blend_pipeline);
		bind_smaa_descriptor_sets = { smaa_descriptor_sets[2], smaa_descriptor_sets[3] };
		vkCmdBindDescriptorSets(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, smaa_blend_pipeline_layout, 0, 2, bind_smaa_descriptor_sets.data(), 0, nullptr);
		vkCmdDraw(command_buffers[i], 3, 1, 0, 0);

		// Smaa blend renderpass end
		vkCmdEndRenderPass(command_buffers[i]);

		// HDR tonemap renderpass start
		render_pass_begin_info = {
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			nullptr,
			hdr_tonemap_renderpass,
			framebuffers[5+i],
			{{0,0},{swapchain_create_info.imageExtent}},
			1,
			&clear_colors[1]
		};
		vkCmdBeginRenderPass(command_buffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, hdr_tonemap_pipeline);
		std::array<VkDescriptorSet, 2> descriptor_sets_layout = { hdr_descriptor_sets[0], smaa_descriptor_sets[3] };
		vkCmdBindDescriptorSets(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, hdr_tonemap_pipeline_layout, 0, descriptor_sets_layout.size(), descriptor_sets_layout.data(), 0, nullptr);
		vkCmdDraw(command_buffers[i], 3, 1, 0, 0);

		// HDR tonemap renderpass start
		vkCmdEndRenderPass(command_buffers[i]);

		vkEndCommandBuffer(command_buffers[i]);
	}

	vkDestroyPipelineLayout(device, light_pipeline_layout, nullptr);
	vkDestroyPipelineLayout(device, pbr_pipeline_layout, nullptr);
	vkDestroyPipelineLayout(device, smaa_edge_pipeline_layout, nullptr);
	vkDestroyPipelineLayout(device, smaa_weight_pipeline_layout, nullptr);
	vkDestroyPipelineLayout(device, smaa_blend_pipeline_layout, nullptr);
	vkDestroyPipelineLayout(device, hdr_tonemap_pipeline_layout, nullptr);
	vkDestroyPipelineLayout(device, shadow_map_pipeline_layout, nullptr);
	vkDestroyPipelineLayout(device, gaussian_blur_xy_pipeline_layouts[0], nullptr);
	vkDestroyPipelineLayout(device, gaussian_blur_xy_pipeline_layouts[1], nullptr);
}

void VulkanSSAO::create_semaphores() {
	VkSemaphoreCreateInfo semaphore_create_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0 };
	semaphores.resize(2);
	for (uint32_t i = 0; i < semaphores.size(); i++) {
		vkCreateSemaphore(device, &semaphore_create_info, nullptr, &semaphores[i]);
	}
}

void VulkanSSAO::frame_loop() {
	uint32_t rendered_frames = 0;
	std::chrono::steady_clock::time_point t1;

	camera_pos = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
	camera_dir = glm::vec4(0.0f, 0.0f, -10.0f, 1.0f);

	light_pos[0] = glm::vec4(0.0f, 1.0f, 1.0f, 1.0f);
	light_color[0] = glm::vec4(10.0f, 10.0f, 10.0f, 10.0f);

	//	bottle descriptor		box descriptor			table descriptor
	//	[---------------]-------[---------------]-------[---------------]-------
	//	[ set:0	128b    ] 128b	[ set:0	128b    ] 128b	[ set:0	128b    ] 128b
	//	[ 2*mat4 		]		[ 2*mat4 		]		[ 2*mat4		]
	//	[---------------]-------[---------------]-------[---------------]-------

	//	camera descriptor		light_descriptor		smaa_rt_metrics descriptor
	//	[---------------]-------[---------------]-------[---------------] 
	//	[ set:1	208b    ] 48b	[ set:2	160b   	] 	96b	[ set:3	16b		]
	//	[ 3*mat4+vec4	]		[ 2*mat4+2*vec4	]		[ 1*vec4		]
	//	[---------------]-------[---------------]-------[---------------]

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		v_matrix = glm::lookAt(glm::vec3(camera_pos), glm::vec3(camera_pos + camera_dir), glm::vec3(0.0f, 1.0f, 0.0f));
		inverse_v_matrix = glm::transpose(glm::inverse(v_matrix));

		water_bottle_m_matrix = glm::translate(glm::vec3(0.0f, 0.15f + glm::sin(glfwGetTime())/7.0f, 0.2f)) 
			* glm::rotate(glm::radians(10.0f), glm::vec3(0.0f, 0.0f, 1.0f))
			* glm::rotate(static_cast<float>(glfwGetTime() * 1.0f), glm::vec3(0.0f, 1.0f, 0.0f))
			* glm::scale(glm::vec3(0.3f, 0.3f, 0.3f));
		water_bottle_normal_matrix = glm::transpose(glm::inverse(water_bottle_m_matrix));

		box_m_matrix = glm::translate(glm::vec3(0.0f, -1.0f, 0.0f))*glm::scale(glm::vec3(3.0f,3.0f,3.0f));
		box_normal_matrix = glm::transpose(glm::inverse(box_m_matrix));

		table_m_matrix = glm::translate(glm::vec3(0.5f, -0.35f, 0.5f))*glm::rotate(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f))
			*glm::scale(glm::vec3(1.5f,1.5f,1.5f));
		table_normal_matrix = glm::transpose(glm::inverse(table_m_matrix));

		camera_v_matrix = glm::lookAt(glm::vec3(light_pos[0]),glm::vec3(0.0f, 0.0f, 0.0f),glm::vec3(0.0f,1.0f,0.0f));

		glm::vec4 smaa_rt_metrics = glm::vec4(1.0f / window_size.width, 1.0f / window_size.height, static_cast<float>(window_size.width), static_cast<float>(window_size.height));

		// First descriptor copy for water bottle
		uint8_t *dst_ptr = static_cast<uint8_t*>(host_data_pointer) + host_memory_requirements[0].size;
		memcpy(dst_ptr, glm::value_ptr(water_bottle_m_matrix), sizeof(glm::mat4));
		memcpy(dst_ptr + sizeof(glm::mat4), glm::value_ptr(water_bottle_normal_matrix), sizeof(glm::mat4));
		
		// Second descriptor copy for box
		dst_ptr += 256;
		memcpy(dst_ptr, glm::value_ptr(box_m_matrix), sizeof(glm::mat4));
		memcpy(dst_ptr + sizeof(glm::mat4), glm::value_ptr(box_normal_matrix), sizeof(glm::mat4));

		// Third descriptor copy for table
		dst_ptr += 256;
		memcpy(dst_ptr, glm::value_ptr(table_m_matrix), sizeof(glm::mat4));
		memcpy(dst_ptr + sizeof(glm::mat4), glm::value_ptr(table_normal_matrix), sizeof(glm::mat4));

		// Third descriptor copy for camera
		dst_ptr += 256;
		memcpy(dst_ptr, glm::value_ptr(v_matrix), sizeof(glm::mat4));
		memcpy(dst_ptr + sizeof(glm::mat4), glm::value_ptr(inverse_v_matrix), sizeof(glm::mat4));
		memcpy(dst_ptr + sizeof(glm::mat4) * 2, glm::value_ptr(p_matrix), sizeof(glm::mat4));
		memcpy(dst_ptr + sizeof(glm::mat4) * 3, glm::value_ptr(camera_pos), sizeof(glm::vec4));

		// Third descriptor copy for light
		dst_ptr += 256;
		memcpy(dst_ptr, glm::value_ptr(camera_v_matrix), sizeof(glm::mat4));
		memcpy(dst_ptr + sizeof(glm::mat4), glm::value_ptr(camera_p_matrix), sizeof(glm::mat4));
		memcpy(dst_ptr + sizeof(glm::mat4)*2, glm::value_ptr(light_pos[0]), sizeof(glm::vec4));
		memcpy(dst_ptr + sizeof(glm::mat4)*2 + sizeof(glm::vec4), glm::value_ptr(light_color[0]), sizeof(glm::vec4));

		// Fourth descriptor copy for smaa_rt_metrics
		dst_ptr += 256;
		memcpy(dst_ptr, glm::value_ptr(smaa_rt_metrics), sizeof(glm::vec4));

		VkMappedMemoryRange mapped_memory_range = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, host_memory,host_memory_requirements[0].size,host_memory_requirements[1].size };
		vkFlushMappedMemoryRanges(device, 1, &mapped_memory_range);

		uint32_t image_index = 0;
		VkResult res = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, semaphores[0], VK_NULL_HANDLE, &image_index);
		if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) {
			on_window_resize();
		}
		else if (res != VK_SUCCESS) {
			throw ACQUIRE_NEXT_IMAGE_FAILED;
		}

		VkPipelineStageFlags pipeline_stage_flags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkSubmitInfo submit_info = {
			VK_STRUCTURE_TYPE_SUBMIT_INFO,
			nullptr,
			1,
			&semaphores[0],
			&pipeline_stage_flags,
			1,
			&command_buffers[image_index],
			1,
			&semaphores[1]
		};
		res = vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);

		VkPresentInfoKHR present_info = {
			VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			nullptr,
			1,
			&semaphores[1],
			1,
			&swapchain,
			&image_index
		};
		res = vkQueuePresentKHR(queue, &present_info);
		if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) {
			on_window_resize();
		}
		else if (res != VK_SUCCESS) {
			throw QUEUE_PRESENT_FAILED;
		}
		glfwPollEvents();

		rendered_frames++;
		uint32_t time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t1).count();
		if ( time_diff > 1000) {
			std::cout << "Msec/frame: " << ( time_diff / static_cast<float>(rendered_frames)) << std::endl;
			rendered_frames = 0;
			t1 = std::chrono::steady_clock::now();
		}
	}
}

void VulkanSSAO::key_callback_static(GLFWwindow* window, int key, int scancode, int action, int mods) {
	VulkanSSAO* that = static_cast<VulkanSSAO*>(glfwGetWindowUserPointer(window));
	that->key_callback(window, key, scancode, action, mods);
}

void VulkanSSAO::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_W) {

		float angle = glm::orientedAngle(glm::normalize(glm::vec2(camera_dir[0], camera_dir[2])), glm::vec2(1.0f, 0.0f));
		float z_comp = sin(angle);
		float x_comp = cos(angle);
		float z_comp_sign = glm::sign(z_comp);
		float x_comp_sign = glm::sign(x_comp);

		camera_pos[2] -= 0.05f * z_comp_sign * pow(z_comp, 2);
		camera_pos[0] += 0.05f * x_comp_sign * pow(x_comp, 2);
	}
	else if (key == GLFW_KEY_S) {

		float angle = glm::orientedAngle(glm::normalize(glm::vec2(camera_dir[0], camera_dir[2])), glm::vec2(1.0f, 0.0f));
		float z_comp = sin(angle);
		float x_comp = cos(angle);
		float z_comp_sign = glm::sign(z_comp);
		float x_comp_sign = glm::sign(x_comp);

		camera_pos[2] += 0.05f * z_comp_sign * pow(z_comp, 2);
		camera_pos[0] -= 0.05f * x_comp_sign * pow(x_comp, 2);
	}
	else if (key == GLFW_KEY_A) {

		float angle = glm::orientedAngle(glm::normalize(glm::vec2(camera_dir[0], camera_dir[2])), glm::vec2(0.0f, -1.0f));
		float z_comp = sin(angle);
		float x_comp = cos(angle);
		float z_comp_sign = glm::sign(z_comp);
		float x_comp_sign = glm::sign(x_comp);

		camera_pos[2] += 0.05f * z_comp_sign * pow(z_comp, 2);
		camera_pos[0] -= 0.05f * x_comp_sign * pow(x_comp, 2);
	}
	else if (key == GLFW_KEY_D) {
		float angle = glm::orientedAngle(glm::normalize(glm::vec2(camera_dir[0], camera_dir[2])), glm::vec2(0.0f, -1.0f));
		float z_comp = sin(angle);
		float x_comp = cos(angle);
		float z_comp_sign = glm::sign(z_comp);
		float x_comp_sign = glm::sign(x_comp);

		camera_pos[2] -= 0.05f * z_comp_sign * pow(z_comp, 2);
		camera_pos[0] += 0.05f * x_comp_sign * pow(x_comp, 2);
	}
	else if (key == GLFW_KEY_LEFT_SHIFT) {
		camera_pos[1] += 0.05f;
	}
	else if (key == GLFW_KEY_LEFT_CONTROL) {
		camera_pos[1] -= 0.05f;
	}
	else if (key == GLFW_KEY_L && action == GLFW_PRESS) {
		if (light_pos[0].w == 0.0f) {
			light_pos[0].w = 1.0f;
			light_pos[1].w = 1.0f;
			light_pos[2].w = 1.0f;
			light_pos[3].w = 1.0f;
		}
		else {
			light_pos[0].w = 0.0f;
			light_pos[1].w = 0.0f;
			light_pos[2].w = 0.0f;
			light_pos[3].w = 0.0f;
		}
	}
	else if (key == GLFW_KEY_ESCAPE) {
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}
}

void VulkanSSAO::cursor_position_callback_static(GLFWwindow* window, double xpos, double ypos) {
	VulkanSSAO* that = static_cast<VulkanSSAO*>(glfwGetWindowUserPointer(window));
	that->cursor_position_callback(window, xpos, ypos);
}

void VulkanSSAO::cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
	double x_delta = 0, y_delta = 0;

	if (ex_xpos && ex_ypos) {
		x_delta = xpos - ex_xpos;
		ex_xpos = xpos;

		y_delta = ypos - ex_ypos;
		ex_ypos = ypos;
	}
	else {
		ex_xpos = xpos;
		ex_ypos = ypos;
	}

	float x_sensivity = 0.0005f;
	float y_sensivity = 0.0005f;

	float key_yaw = x_sensivity * -x_delta;
	float key_pitch = y_sensivity * -y_delta;

	/*camera_dir = glm::vec3( glm::rotate(key_pitch, glm::vec3(1.0f, 0.0f, 0.0f))
		*glm::rotate(key_yaw, glm::vec3(0.0f, 1.0f, 0.0f))
		*glm::vec4(camera_dir, 1.0f));*/

	glm::vec3 axis = glm::cross(glm::vec3(camera_dir), glm::vec3(0.0f, 1.0f, 0.0f));
	glm::quat pitch_quat = glm::angleAxis(key_pitch, axis);
	glm::quat heading_quat = glm::angleAxis(key_yaw, glm::vec3(0.0f, 1.0f, 0.0f));
	glm::quat temp = glm::cross(pitch_quat, heading_quat);
	temp = glm::normalize(temp);
	camera_dir = glm::rotate(temp, camera_dir);
}

void VulkanSSAO::on_window_resize() {
	vkDeviceWaitIdle(device);

	int width, height;
	glfwGetWindowSize(window, &width, &height);
	window_size = { static_cast<uint32_t>(width),static_cast<uint32_t>(height) };

	// Pipelines cleaning
	vkDestroyPipeline(device, hdr_tonemap_pipeline, nullptr);
	vkDestroyPipeline(device, smaa_blend_pipeline, nullptr);
	vkDestroyPipeline(device, smaa_weight_pipeline, nullptr);
	vkDestroyPipeline(device, smaa_edge_pipeline, nullptr);
	vkDestroyPipeline(device, pbr_pipeline, nullptr);

	// Memory Attachemnts cleaning
	vkDestroyImageView(device, device_smaa_edge_image_view, nullptr);
	vkDestroyImageView(device, device_smaa_weight_image_view, nullptr);
	vkDestroyImage(device, device_smaa_image, nullptr);

	vkDestroyImageView(device, device_render_target_second_image_view, nullptr);
	vkDestroyImageView(device, device_render_target_image_view, nullptr);
	vkDestroyImage(device, device_render_target_image, nullptr);

	vkDestroyImageView(device, device_stencil_image_view, nullptr);
	vkDestroyImage(device, device_stencil_image, nullptr);

	vkDestroyImageView(device, device_depth_image_view, nullptr);
	vkDestroyImage(device, device_depth_image, nullptr);
	vkDestroySampler(device, device_render_target_sampler, nullptr);

	vkFreeMemory(device, device_memory_attachments, nullptr);

	// Framebuffers and swapchain images view cleaning
	for (const auto& framebuffer : framebuffers) {
		vkDestroyFramebuffer(device, framebuffer, nullptr);
	}
	for (const auto& swapchain_image_view : swapchain_images_views) {
		vkDestroyImageView(device, swapchain_image_view, nullptr);
	}

	// Destroying smaa descriptor sets because render_target, smaa_weight, smaa_edge have to be recreated
	vkFreeDescriptorSets(device, descriptor_pool, smaa_descriptor_sets.size(), smaa_descriptor_sets.data());
	vkFreeDescriptorSets(device, descriptor_pool, hdr_descriptor_sets.size(), hdr_descriptor_sets.data());

	vkDestroyRenderPass(device, pbr_render_pass, nullptr);
	old_swapchain = swapchain;
	create_swapchain();
	calculate_projection_matrix();
	create_attachments();
	allocate_smaa_descriptor_sets();
	allocate_hdr_descriptor_sets();
	create_renderpass();
	create_framebuffers();
	create_pbr_pipeline();
	create_smaa_edge_pipeline();
	create_smaa_weight_pipeline();
	create_smaa_blend_pipeline();
	create_hdr_tonemap_pipeline();
	vkResetCommandPool(device, command_pool, 0);
	record_command_buffers();
}

VulkanSSAO::VulkanSSAO() {
	create_instance();
#ifndef NDEBUG
	setup_debug_callback();
#endif
	create_window();
	create_surface();
	create_logical_device();
	create_swapchain();
	calculate_projection_matrix();
	create_command_pool();
	allocate_command_buffers();
	create_host_buffers();
	create_device_buffers();
	create_attachments();
	create_descriptor_pool_and_layouts();
	allocate_gaussian_blur_descriptor_sets();
	allocate_pbr_descriptor_sets();
	allocate_camera_light_sets();
	allocate_smaa_descriptor_sets();
	allocate_hdr_descriptor_sets();
	create_renderpass();
	create_framebuffers();
	create_shadow_map_pipeline();
	create_gaussian_blur_pipelines();
	create_light_pipeline();
	create_pbr_pipeline();
	create_smaa_edge_pipeline();
	create_smaa_weight_pipeline();
	create_smaa_blend_pipeline();
	create_hdr_tonemap_pipeline();
	upload_input_data();
	record_command_buffers();
	create_semaphores();
	glfwSetWindowUserPointer(window, this);
}

void VulkanSSAO::start_main_loop() {
	frame_loop();
}

VulkanSSAO::~VulkanSSAO() {
	vkDeviceWaitIdle(device);
	for (uint32_t i = 0; i < semaphores.size(); i++) {
		vkDestroySemaphore(device, semaphores[i], nullptr);
	}
	
	// Pipelines cleaning
	vkDestroyPipeline(device, gaussian_blur_xy_pipelines[0], nullptr);
	vkDestroyPipeline(device, gaussian_blur_xy_pipelines[1], nullptr);
	vkDestroyPipeline(device, light_pipeline, nullptr);
	vkDestroyPipeline(device, shadow_map_pipeline, nullptr);
	vkDestroyPipeline(device, hdr_tonemap_pipeline, nullptr);
	vkDestroyPipeline(device, smaa_blend_pipeline, nullptr);
	vkDestroyPipeline(device, smaa_weight_pipeline, nullptr);
	vkDestroyPipeline(device, smaa_edge_pipeline, nullptr);
	vkDestroyPipeline(device, pbr_pipeline, nullptr);

	// Memory Attachemnts cleaning
	vkDestroyImageView(device, device_smaa_edge_image_view, nullptr);
	vkDestroyImageView(device, device_smaa_weight_image_view, nullptr);
	vkDestroyImage(device, device_smaa_image, nullptr);

	vkDestroyImageView(device, device_render_target_second_image_view, nullptr);
	vkDestroyImageView(device, device_render_target_image_view, nullptr);
	vkDestroyImage(device, device_render_target_image, nullptr);

	vkDestroyImageView(device, device_stencil_image_view, nullptr);
	vkDestroyImage(device, device_stencil_image, nullptr);

	vkDestroyImageView(device, device_depth_image_view, nullptr);
	vkDestroyImage(device, device_depth_image, nullptr);

	vkDestroyImageView(device, device_vsm_depth_0_image_view, nullptr);
	vkDestroyImageView(device, device_vsm_depth_1_image_view, nullptr);
	vkDestroyImage(device, device_vsm_depth_image, nullptr);

	vkDestroySampler(device, device_render_target_sampler, nullptr);
	vkDestroySampler(device, device_shadow_map_sampler, nullptr);

	vkFreeMemory(device, device_memory_attachments, nullptr);

	// Framebuffers and swapchain images view cleaning
	for (const auto& framebuffer : framebuffers) {
		vkDestroyFramebuffer(device, framebuffer, nullptr);
	}

	for (const auto& swapchain_image_view : swapchain_images_views) {
		vkDestroyImageView(device, swapchain_image_view, nullptr);
	}

	// Renderpasses cleaning
	vkDestroyRenderPass(device, shadow_map_render_pass, nullptr);
	vkDestroyRenderPass(device, hdr_tonemap_renderpass, nullptr);
	vkDestroyRenderPass(device, pbr_render_pass, nullptr);
	vkDestroyRenderPass(device, smaa_edge_renderpass, nullptr);
	vkDestroyRenderPass(device, smaa_weight_renderpass, nullptr);
	vkDestroyRenderPass(device, smaa_blend_renderpass, nullptr);

	// Descriptor Pool cleaning
	vkDestroyDescriptorSetLayout(device, hdr_descriptor_sets_layout[0], nullptr);
	vkDestroyDescriptorSetLayout(device, pbr_descriptor_sets_layout[0], nullptr);
	vkDestroyDescriptorSetLayout(device, smaa_descriptor_sets_layout[0], nullptr);
	vkDestroyDescriptorSetLayout(device, smaa_descriptor_sets_layout[1], nullptr);
	vkDestroyDescriptorSetLayout(device, smaa_descriptor_sets_layout[2], nullptr);
	vkDestroyDescriptorSetLayout(device, smaa_descriptor_sets_layout[3], nullptr);
	vkDestroyDescriptorSetLayout(device, camera_light_sets_layout[0], nullptr);
	vkDestroyDescriptorSetLayout(device, gaussian_blur_descriptor_sets_layout[0], nullptr);
	vkDestroyDescriptorPool(device, descriptor_pool, nullptr);

	// Host memory cleaning
	vkUnmapMemory(device, host_memory);
	vkDestroyBuffer(device, host_vertex_buffer, nullptr);
	vkDestroyBuffer(device, host_uniform_buffer, nullptr);
	vkFreeMemory(device, host_memory, nullptr);

	// Device memory cleaning
	vkDestroyBuffer(device, device_vertex_buffer, nullptr);
	vkDestroyBuffer(device, device_uniform_buffer, nullptr);
	vkDestroyImageView(device, device_water_bottle_color_image_view, nullptr);
	vkDestroyImageView(device, device_water_bottle_orm_image_view, nullptr);
	vkDestroyImageView(device, device_water_bottle_normal_image_view, nullptr);
	vkDestroyImageView(device, device_water_bottle_emissive_image_view, nullptr);
	vkDestroyImage(device, device_water_bottle_image, nullptr);

	vkDestroyImageView(device, device_box_color_image_view, nullptr);
	vkDestroyImageView(device, device_box_orm_image_view, nullptr);
	vkDestroyImageView(device, device_box_normal_image_view, nullptr);
	vkDestroyImageView(device, device_box_emissive_image_view, nullptr);
	vkDestroyImage(device, device_box_image, nullptr);

	vkDestroyImageView(device, device_table_color_image_view, nullptr);
	vkDestroyImageView(device, device_table_orm_image_view, nullptr);
	vkDestroyImageView(device, device_table_normal_image_view, nullptr);
	vkDestroyImageView(device, device_table_emissive_image_view, nullptr);
	vkDestroyImage(device, device_table_image, nullptr);
	vkDestroySampler(device, device_max_aniso_linear_sampler, nullptr);

	vkDestroyImageView(device, device_smaa_search_image_view, nullptr);
	vkDestroyImage(device, device_smaa_search_image, nullptr);
	vkDestroyImageView(device, device_smaa_area_image_view, nullptr);
	vkDestroyImage(device, device_smaa_area_image, nullptr);

	vkFreeMemory(device, device_memory, nullptr);

	// Command pool cleaning
	vkFreeCommandBuffers(device, command_pool, command_buffers.size(), command_buffers.data());
	vkDestroyCommandPool(device, command_pool, nullptr);
	vkDestroySwapchainKHR(device, swapchain, nullptr);

	// Device and instance destroy
	vkDestroyDevice(device, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	glfwDestroyWindow(window);
#ifndef NDEBUG
	vkDestroyDebugReportCallbackEXT(instance, debug_report_callback, nullptr);
#endif
	vkDestroyInstance(instance, nullptr);
}

int main() {
	try {
		VulkanSSAO vk_triangle;
		vk_triangle.start_main_loop();
	}
	catch (VulkanSSAO::Errors & err) {
		std::cout << "Error: " << err;
	}
	return 0;
}