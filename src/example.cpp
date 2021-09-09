#include <iostream>
#include <utility>
#include <vector>
#include "TheVulkanTemple/graphics_module_vulkan_app.h"
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
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

    float speed = 0.0003f;
    glm::vec4 camera_pos_diff = glm::vec4(0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
    	camera_pos_diff[2] = speed;
    }
    else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
    	camera_pos_diff[2] = -speed;
    }
    else if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
    	camera_pos_diff[0] = speed;
    }
    else if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
    	camera_pos_diff[0] = -speed;
    }
    else if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
    	camera_pos_diff[1] -= speed;
    }
    else if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
    	camera_pos_diff[1] += speed;
    }
    else if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        camera_pos_diff *= glm::vec4(3.0f);
    }
    app->get_camera_ptr()->pos += glm::mat3(glm::transpose(app->get_camera_ptr()->get_view_matrix())) * camera_pos_diff * static_cast<float>(delta_time);

    // CAMERA MOUSE CONTROL
    glm::dvec2 mouse_polar;
    glfwGetCursorPos(window, &mouse_polar[1], &mouse_polar[0]);
    mouse_polar *= glm::dvec2(-0.001f, -0.001f);

    app->get_camera_ptr()->dir = glm::normalize(static_cast<glm::vec3>(glm::euclidean(mouse_polar)));

    // Other things
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) {
        glm::mat4 ball_3_m_matrix = glm::translate(app->get_camera_ptr()->pos + app->get_camera_ptr()->dir)*glm::scale(glm::vec3(0.01f));
        app->get_gltf_model_ptr(4)->set_model_matrix(ball_3_m_matrix);
    }

	app->get_light_ptr(1)->set_pos(app->get_camera_ptr()->pos + app->get_camera_ptr()->dir*0.1f);
	app->get_light_ptr(1)->set_dir(app->get_camera_ptr()->dir);
	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT)) {
		app->get_light_ptr(1)->set_color(glm::vec3(0.0f));
	}
	else {
		app->get_light_ptr(1)->set_color({17.0f, 7.0f, 13.0f});
	}

	//std::cout << glm::to_string(app->get_camera_ptr()->pos) << std::endl;
	//std::cout << glm::to_string(app->get_camera_ptr()->dir) << std::endl;
}

int main() {
    EngineOptions options;
    options.fsr_settings.preset = AmdFsr::Preset::ULTRA_QUALITY;
	options.fsr_settings.precision = AmdFsr::Precision::FP16;
  
	try {
	    VkExtent2D screen_size = {800,800};
		GraphicsModuleVulkanApp app("TheVulkanTemple", screen_size, false, options);

		glm::mat4 water_bottle_m_matrix = glm::translate(glm::vec3(0.296420, 0.45, 0.144471))*glm::scale(glm::vec3(0.05f));
        glm::mat4 table_m_matrix = glm::translate(glm::vec3(0.5f, -0.35f, 0.0f))*glm::rotate(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f))
                                   *glm::scale(glm::vec3(3.0f,3.0f,3.0f));
        glm::mat4 floor_m_matrix = glm::translate(glm::vec3(-0.8f, -1.5f, 0.0f))*glm::scale(glm::vec3(100.0f,100.0f,100.0f));
        glm::mat4 chair_m_matrix = glm::translate(glm::vec3(0.570383, 0.00, -0.065306))*glm::rotate(glm::radians(-30.0f), glm::vec3(0.0f, 1.0f, 0.0f))
                *glm::scale(glm::vec3(0.1f));

        glm::mat4 ball_3_m_matrix = glm::translate(glm::vec3(-0.516224, 1.217391, -0.000071))*glm::scale(glm::vec3(0.01f));

        glm::mat4 sponza_m_matrix = glm::scale(glm::vec3(2.0f));

		app.load_3d_objects({
							{"resources/models/WaterBottle/WaterBottle.glb", water_bottle_m_matrix},
							{"resources//models//Table//Table.glb", table_m_matrix},
                            {"resources//models//MarbleFloor//MarbleFloor.glb", floor_m_matrix},
                            {"resources//models//SchoolChair//SchoolChair.glb", chair_m_matrix},
                            {"resources//models//EightBall/EightBall.glb", ball_3_m_matrix},
                            {"resources//models//Sponza/Sponza.glb", sponza_m_matrix}
		});
		app.load_lights({
			{{-0.010837, 1.506811, -0.328537}, glm::normalize(glm::vec3({-0.004270, -0.702568, 0.711604})), {10.0f, 10.0f, 10.0f}, Light::LightType::SPOT,
             0.0f, glm::radians(glm::vec2(30.0f, 45.0f)), 2000, glm::radians(90.0f), 1.0f, 0.1, 100.0f},
            {{2.0f, 2.0f, -4.0f}, glm::normalize(glm::vec3({-2.0f, -2.0f, 4.0f})), {17.0f, 7.0f, 13.0f}, Light::LightType::SPOT,
             30.0f, glm::radians(glm::vec2(30.0f, 45.0f)), 0, glm::radians(90.0f), 1.0f, 0.01, 1000.0f},
             {{-0.516224, 1.217391, -0.000071}, glm::normalize(glm::vec3({0.773662, -0.631123, -0.055959})), {0.0f, 0.34f, 11.4f}, Light::LightType::SPOT,
			  20.0f, glm::radians(glm::vec2(30.0f, 45.0f)), 2000, glm::radians(90.0f), 1.0f, 0.1, 100.0f}
		});
		app.set_camera({{-0.20, 0.30, -0.02}, {0.987121, -0.157343, 0.028908}, glm::radians(90.0f), static_cast<float>(screen_size.width)/screen_size.height, 0.01f, 1000.0f});

        //glfwSetInputMode(app.get_glfw_window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        app.init_renderer();

        app.start_frame_loop(resize_callback, frame_start);
	}
	catch (std::pair<int32_t,vulkan_helper::Error>& err) {
		std::cout << "The application encounted the error: " << magic_enum::enum_name(err.second) << " with return value: " << err.first << std::endl;
	}
	return 0;
}