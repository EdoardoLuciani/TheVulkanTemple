#include <iostream>
#include <utility>
#include <vector>
#include "vulkan_helper.h"
#include "graphics_module_vulkan_app.h"
#include "fps_camera_control.h"
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

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

    std::vector<const char*> desired_device_level_extensions = {"VK_KHR_swapchain", "VK_EXT_descriptor_indexing"};
    VkPhysicalDeviceFeatures selected_device_features = { 0 };
    selected_device_features.samplerAnisotropy = VK_TRUE;

    VkPhysicalDeviceDescriptorIndexingFeaturesEXT required_physical_device_indexing_features = {};
    required_physical_device_indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
    required_physical_device_indexing_features.pNext = nullptr;
    required_physical_device_indexing_features.runtimeDescriptorArray = VK_TRUE;
    required_physical_device_indexing_features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

    EngineOptions options;
    options.anti_aliasing = 0;
    options.shadows = 0;
    options.HDR = 0;
  
	try {
	    VkExtent2D screen_size = {2560,1440};
		GraphicsModuleVulkanApp app("TheVulkanTemple", desired_instance_level_extensions, screen_size, true,
                                    desired_device_level_extensions, selected_device_features, VK_TRUE, options, &required_physical_device_indexing_features);
		app.load_3d_objects({"resources/models/WaterBottle/WaterBottle.glb", "resources//models//Table//Table.glb"},128);
		app.load_lights({
		    {{1.0f, 1.0f, 2.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {50.0f, 0.0f, 0.0f}, 90.0f, 1.0f},
        {{1.0f, 1.0f, -2.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 50.0f}, 90.0f, 1.0f}
		});
		app.set_camera({{0.0f, 1.0f, 5.0f, 0.0f}, {0.0f, .0f, -10.0f}, 100.0f, static_cast<float>(screen_size.width)/screen_size.height});
		FPSCameraControl fps_camera_control(app.get_glfw_window(), app.get_camera_ptr());

        app.init_renderer();

        uint8_t* water_bottle_uniform_ptr = app.get_model_uniform_data_ptr(0);
        glm::mat4 water_bottle_m_matrix = glm::translate(glm::vec3(-1.0f, 0.05f, -0.5f))*glm::scale(glm::vec3(0.4f,0.4f,0.4f));
        memcpy(water_bottle_uniform_ptr, glm::value_ptr(water_bottle_m_matrix), sizeof(glm::mat4));

        glm::mat4 water_bottle_normal_matrix = glm::transpose(glm::inverse(water_bottle_m_matrix));
        memcpy(water_bottle_uniform_ptr + sizeof(glm::mat4), glm::value_ptr(water_bottle_normal_matrix), sizeof(glm::mat4));

        uint8_t* table_uniform_ptr = app.get_model_uniform_data_ptr(1);
        glm::mat4 table_m_matrix = glm::translate(glm::vec3(0.5f, -0.35f, 0.0f))*glm::rotate(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f))
                         *glm::scale(glm::vec3(3.0f,3.0f,3.0f));
        memcpy(table_uniform_ptr, glm::value_ptr(table_m_matrix), sizeof(glm::mat4));

        glm::mat4 table_normal_matrix = glm::transpose(glm::inverse(table_m_matrix));
        memcpy(table_uniform_ptr + sizeof(glm::mat4), glm::value_ptr(table_normal_matrix), sizeof(glm::mat4));

        app.start_frame_loop();
	}
	catch (std::pair<int32_t,vulkan_helper::Error>& err) {
		std::cout << "The application encounted the error: " << magic_enum::enum_name(err.second) << " with return value: " << err.first << std::endl;
	}
	return 0;
}