#include <iostream>
#include <utility>
#include <vector>
#include "TheVulkanTemple/graphics_module_vulkan_app.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/polar_coordinates.hpp>

#define MAGIC_ENUM_RANGE_MIN 0
#define MAGIC_ENUM_RANGE_MAX 256
#include "magic_enum.hpp"

void resize_callback(GraphicsModuleVulkanApp *app) {
    int width, height;
    glfwGetWindowSize(app->get_glfw_window(), &width, &height);
    app->get_camera_ptr()->aspect = (static_cast<float>(width)/height);
}

void frame_start(GraphicsModuleVulkanApp *app, uint32_t delta_time) {
    GLFWwindow *window = app->get_glfw_window();

    glm::vec4 camera_pos_diff = glm::vec4(0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        camera_pos_diff[2] = 0.003f;
    }
    else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        camera_pos_diff[2] = -0.003f;
    }
    else if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        camera_pos_diff[0] = 0.003f;
    }
    else if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        camera_pos_diff[0] = -0.003f;
    }
    else if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        camera_pos_diff[1] -= 0.003f;
    }
    else if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
        camera_pos_diff[1] += 0.003f;
    }
    else if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    app->get_camera_ptr()->pos += glm::transpose(app->get_camera_ptr()->get_view_matrix()) * camera_pos_diff * static_cast<float>(delta_time);

    // CAMERA MOUSE CONTROL

    glm::dvec2 mouse_polar;
    glfwGetCursorPos(window, &mouse_polar[1], &mouse_polar[0]);
    mouse_polar *= glm::dvec2(0.001f, -0.001f);

    glm::vec3 dir = static_cast<glm::vec3>(glm::euclidean(mouse_polar)) * app->get_camera_ptr()->get_distance();
    app->get_camera_ptr()->dir = app->get_camera_ptr()->pos - glm::vec4(dir, 0.0f);
    app->get_camera_ptr()->set_distance(glm::length(app->get_camera_ptr()->dir - glm::vec3(app->get_camera_ptr()->pos)));

    // LIGHTS REGULATION
    double time = glfwGetTime();
    /*
    app->get_light_ptr(0)->pos = glm::vec4(glm::cos(time)*4, 1.0f, glm::sin(time)*4, 1.0f);
    app->get_light_ptr(1)->color = glm::vec3(glm::abs(glm::tan(time*2.0f)*2.0f), 0.0f, 0.0f);

    app->get_gltf_model_ptr(0)->set_model_matrix(glm::translate(glm::vec3(-1.0f, 0.02f, -0.5f))*glm::rotate(static_cast<float>(time*5), glm::vec3(1.0f, 0.0f, 0.0f))*glm::scale(glm::vec3(0.4f,0.4f,0.4f)));
    app->get_gltf_model_ptr(1)->set_model_matrix(glm::translate(glm::vec3(0.5f, -0.35f, 0.0f))*glm::rotate(static_cast<float>(time), glm::vec3(0.0f, 1.0f, 0.0f))
    *glm::scale(glm::vec3(2.5f,2.5f,2.5f)));
    app->get_gltf_model_ptr(3)->set_model_matrix(glm::translate(glm::vec3(-0.8f, -1.5f, 0.5f))*glm::rotate(glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f))
                                           *glm::scale(glm::vec3(0.2f,0.2f,0.2f))*glm::scale(glm::vec3(1.0f, 1.0f, glm::abs(glm::cos(time)))*10.0f+1.0f));
    */
}

int main() {
    std::vector<const char*> desired_instance_level_extensions = {"VK_KHR_surface"};
	#ifdef _WIN64
		desired_instance_level_extensions.push_back("VK_KHR_win32_surface");
	#elif __linux__
		desired_instance_level_extensions.push_back("VK_KHR_xlib_surface");
	#else
		#error "Unknown compiler or not supported OS"
    #endif

    std::vector<const char*> desired_device_level_extensions = {"VK_KHR_swapchain", "VK_EXT_descriptor_indexing", "VK_KHR_shader_float16_int8"};

    VkPhysicalDeviceFeatures selected_device_features = {0};
    selected_device_features.samplerAnisotropy = VK_TRUE;
    selected_device_features.shaderInt16 = VK_TRUE;

    VkPhysicalDeviceShaderFloat16Int8FeaturesKHR shader_float_16_int_8_features = {};
    shader_float_16_int_8_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR;
    shader_float_16_int_8_features.pNext = nullptr;
    shader_float_16_int_8_features.shaderFloat16 = VK_TRUE;
    shader_float_16_int_8_features.shaderInt8 = VK_TRUE;

    VkPhysicalDeviceDescriptorIndexingFeaturesEXT required_physical_device_indexing_features = {};
    required_physical_device_indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
    required_physical_device_indexing_features.pNext = &shader_float_16_int_8_features;
    required_physical_device_indexing_features.runtimeDescriptorArray = VK_TRUE;
    required_physical_device_indexing_features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

    EngineOptions options;
    options.anti_aliasing = 0;
    options.shadows = 0;
    options.HDR = 0;
  
	try {
	    VkExtent2D screen_size = {800,800};
		GraphicsModuleVulkanApp app("TheVulkanTemple", desired_instance_level_extensions, screen_size, false,
                                    desired_device_level_extensions, selected_device_features, VK_TRUE, options, &required_physical_device_indexing_features);

        glm::mat4 water_bottle_m_matrix = glm::translate(glm::vec3(-1.0f, 0.02f, -0.5f))*glm::scale(glm::vec3(0.4f,0.4f,0.4f));
        glm::mat4 table_m_matrix = glm::translate(glm::vec3(0.5f, -0.35f, 0.0f))*glm::rotate(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f))
                                   *glm::scale(glm::vec3(3.0f,3.0f,3.0f));
        glm::mat4 floor_m_matrix = glm::translate(glm::vec3(-0.8f, -1.5f, 0.0f))*glm::scale(glm::vec3(2.5f,2.5f,2.5f));
        glm::mat4 chair_m_matrix = glm::translate(glm::vec3(-0.8f, -1.5f, 0.5f))*glm::rotate(glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f))
                *glm::scale(glm::vec3(1.5f,1.5f,1.5f));

		app.load_3d_objects({{"resources/models/WaterBottle/WaterBottle.glb", water_bottle_m_matrix},
                             {"resources//models//Table//Table.glb", table_m_matrix},
                             {"resources//models//MarbleFloor//MarbleFloor.glb", floor_m_matrix},
                             {"resources//models//SchoolChair//SchoolChair.glb", chair_m_matrix}
		});
		app.load_lights({
		    {{1.0f, 1.0f, 2.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {50.0f, 50.0f, 0.0f}, glm::radians(90.0f), 1.0f},
            {{1.0f, 1.0f, -2.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {50.0f, 10.0f, 25.0f}, glm::radians(90.0f), 1.0f}
		});
		app.set_camera({{0.0f, 1.0f, 5.0f, 0.0f}, {0.0f, 0.0f, -10.0f}, glm::radians(90.0f), static_cast<float>(screen_size.width)/screen_size.height, 0.1f, 1000.0f});

        glfwSetInputMode(app.get_glfw_window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        app.init_renderer();

        app.start_frame_loop(resize_callback, frame_start);
	}
	catch (std::pair<int32_t,vulkan_helper::Error>& err) {
		std::cout << "The application encounted the error: " << magic_enum::enum_name(err.second) << " with return value: " << err.first << std::endl;
	}
	return 0;
}