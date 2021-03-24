#include <iostream>
#include <utility>
#include <vector>
#include "graphics_module_vulkan_app.h"
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/vector_angle.hpp>

#define MAGIC_ENUM_RANGE_MIN 0
#define MAGIC_ENUM_RANGE_MAX 256
#include "magic_enum.hpp"

void resize_callback(GraphicsModuleVulkanApp *app) {
    int width, height;
    glfwGetWindowSize(app->get_glfw_window(), &width, &height);
    app->get_camera_ptr()->set_aspect(static_cast<float>(width)/height);
}

void frame_start(GraphicsModuleVulkanApp *app, uint32_t delta_time) {
    int key;
    GLFWwindow *window = app->get_glfw_window();

    glm::vec3 camera_pos_diff = glm::vec3(0.0f);
    glm::vec3 camera_dir = app->get_camera_ptr()->get_dir();

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        float angle = glm::orientedAngle(glm::normalize(glm::vec2(camera_dir[0], camera_dir[2])), glm::vec2(1.0f, 0.0f));
        float z_comp = sin(angle);
        float x_comp = cos(angle);
        float z_comp_sign = glm::sign(z_comp);
        float x_comp_sign = glm::sign(x_comp);

        camera_pos_diff[2] -= 0.05f * z_comp_sign * pow(z_comp, 2);
        camera_pos_diff[0] += 0.05f * x_comp_sign * pow(x_comp, 2);
    }
    else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        float angle = glm::orientedAngle(glm::normalize(glm::vec2(camera_dir[0], camera_dir[2])), glm::vec2(1.0f, 0.0f));
        float z_comp = sin(angle);
        float x_comp = cos(angle);
        float z_comp_sign = glm::sign(z_comp);
        float x_comp_sign = glm::sign(x_comp);

        camera_pos_diff[2] += 0.05f * z_comp_sign * pow(z_comp, 2);
        camera_pos_diff[0] -= 0.05f * x_comp_sign * pow(x_comp, 2);
    }
    else if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        float angle = glm::orientedAngle(glm::normalize(glm::vec2(camera_dir[0], camera_dir[2])), glm::vec2(0.0f, -1.0f));
        float z_comp = sin(angle);
        float x_comp = cos(angle);
        float z_comp_sign = glm::sign(z_comp);
        float x_comp_sign = glm::sign(x_comp);

        camera_pos_diff[2] += 0.05f * z_comp_sign * pow(z_comp, 2);
        camera_pos_diff[0] -= 0.05f * x_comp_sign * pow(x_comp, 2);
    }
    else if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        float angle = glm::orientedAngle(glm::normalize(glm::vec2(camera_dir[0], camera_dir[2])), glm::vec2(0.0f, -1.0f));
        float z_comp = sin(angle);
        float x_comp = cos(angle);
        float z_comp_sign = glm::sign(z_comp);
        float x_comp_sign = glm::sign(x_comp);

        camera_pos_diff[2] -= 0.05f * z_comp_sign * pow(z_comp, 2);
        camera_pos_diff[0] += 0.05f * x_comp_sign * pow(x_comp, 2);
    }
    else if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        camera_pos_diff[1] += 0.05f;
    }
    else if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
        camera_pos_diff[1] -= 0.05f;
    }
    else if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    glm::vec3 camera_pos = app->get_camera_ptr()->get_pos();
    app->get_camera_ptr()->set_pos(camera_pos + (camera_pos_diff * (static_cast<float>(delta_time) * 0.1f)));

    // CAMERA MOUSE CONTROL
    glm::dvec2 pos;
    glfwGetCursorPos(window, &pos[0], &pos[1]);

    glm::dvec2 delta = glm::dvec2(0, 0);
    delta = pos - app->get_camera_ptr()->get_ex_pos();
    app->get_camera_ptr()->set_ex_pos(pos);

    float x_sensivity = 0.0003f;
    float y_sensivity = 0.0001f;

    float key_yaw = x_sensivity * delta[0];
    float key_pitch = y_sensivity * -delta[1];

    /*camera->dir = glm::vec3( glm::rotate(key_pitch, glm::vec3(1.0f, 0.0f, 0.0f))
        *glm::rotate(key_yaw, glm::vec3(0.0f, 1.0f, 0.0f))
        *glm::vec4(camera->dir, 1.0f));*/

    glm::vec3 axis = glm::cross(camera_dir, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat pitch_quat = glm::angleAxis(key_pitch, axis);
    glm::quat heading_quat = glm::angleAxis(key_yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat temp = glm::cross(pitch_quat, heading_quat);
    temp = glm::normalize(temp);
    app->get_camera_ptr()->set_dir(glm::rotate(temp, camera_dir));
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

    VkPhysicalDeviceFeatures selected_device_features = { 0 };
    selected_device_features.samplerAnisotropy = VK_TRUE;

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

		app.load_3d_objects({{"resources/models/WaterBottle/WaterBottle.glb", water_bottle_m_matrix},
                             {"resources//models//Table//Table.glb", table_m_matrix}
		});
		app.load_lights({
		    {{1.0f, 1.0f, 2.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {50.0f, 0.0f, 0.0f}, 90.0f, 1.0f},
            {{1.0f, 1.0f, -2.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 50.0f}, 90.0f, 1.0f}
		});
		app.set_camera({{0.0f, 1.0f, 5.0f, 0.0f}, {0.0f, .0f, -10.0f}, 100.0f, static_cast<float>(screen_size.width)/screen_size.height});

        glfwSetInputMode(app.get_glfw_window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        app.init_renderer();

        app.start_frame_loop(resize_callback, frame_start);
	}
	catch (std::pair<int32_t,vulkan_helper::Error>& err) {
		std::cout << "The application encounted the error: " << magic_enum::enum_name(err.second) << " with return value: " << err.first << std::endl;
	}
	return 0;
}