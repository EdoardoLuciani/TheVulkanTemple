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
    app->get_light_ptr(1)->set_pos(app->get_camera_ptr()->pos + app->get_camera_ptr()->dir*0.1f);
    app->get_light_ptr(1)->set_dir(app->get_camera_ptr()->dir);

    app->get_light_ptr(0)->set_color(glm::vec3(5.0f*glm::clamp(glm::cos(glfwGetTime()/2), 0.0, 1.0)));

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) {
        glm::mat4 ball_3_m_matrix = glm::translate(app->get_camera_ptr()->pos + app->get_camera_ptr()->dir)*glm::scale(glm::vec3(0.2f));
        app->get_gltf_model_ptr(6)->set_model_matrix(ball_3_m_matrix);
    }
}

int main() {
    EngineOptions options;
    options.anti_aliasing = 0;
    options.shadows = 0;
    options.HDR = 0;
  
	try {
	    VkExtent2D screen_size = {800,800};
		GraphicsModuleVulkanApp app("TheVulkanTemple", screen_size, false, options);

        glm::mat4 water_bottle_m_matrix = glm::translate(glm::vec3(-1.0f, 0.02f, -0.5f))*glm::scale(glm::vec3(0.4f,0.4f,0.4f));
        glm::mat4 table_m_matrix = glm::translate(glm::vec3(0.5f, -0.35f, 0.0f))*glm::rotate(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f))
                                   *glm::scale(glm::vec3(3.0f,3.0f,3.0f));
        glm::mat4 floor_m_matrix = glm::translate(glm::vec3(-0.8f, -1.5f, 0.0f))*glm::scale(glm::vec3(100.0f,100.0f,100.0f));
        glm::mat4 chair_m_matrix = glm::translate(glm::vec3(-0.8f, -1.5f, 0.5f))*glm::rotate(glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f))
                *glm::scale(glm::vec3(1.5f,1.5f,1.5f));

        glm::mat4 ball_1_m_matrix = glm::translate(glm::vec3(2.0f, 2.0f, 4.0f))*glm::scale(glm::vec3(0.2f));
        glm::mat4 ball_2_m_matrix = glm::translate(glm::vec3(2.0f, 2.0f, -4.0f))*glm::scale(glm::vec3(0.2f));
        glm::mat4 ball_3_m_matrix = glm::translate(glm::vec3(0.0f, 0.0f, 0.0f))*glm::scale(glm::vec3(0.2f));

		app.load_3d_objects({{"resources/models/WaterBottle/WaterBottle.glb", water_bottle_m_matrix},
                             {"resources//models//Table//Table.glb", table_m_matrix},
                             {"resources//models//MarbleFloor//MarbleFloor.glb", floor_m_matrix},
                             {"resources//models//SchoolChair//SchoolChair.glb", chair_m_matrix},
                             {"resources//models//EightBall/EightBall.glb", ball_1_m_matrix},
                             {"resources//models//EightBall/EightBall.glb", ball_2_m_matrix},
                             {"resources//models//EightBall/EightBall.glb", ball_3_m_matrix}
		});
		app.load_lights({
		    {{2.0f, 2.0f, 4.0f}, glm::normalize(glm::vec3({-1.0f, -1.0f, -2.0f})), {10.0f, 10.0f, 10.0f}, Light::LightType::DIRECTIONAL,
             0.0f, glm::radians(glm::vec2(30.0f, 45.0f)), 0, glm::radians(90.0f), 1.0f, 0.01, 10.0f},
            {{2.0f, 2.0f, -4.0f}, glm::normalize(glm::vec3({-2.0f, -2.0f, 4.0f})), {17.0f, 7.0f, 13.0f}, Light::LightType::SPOT,
             30.0f, glm::radians(glm::vec2(30.0f, 45.0f)), 1000, glm::radians(90.0f), 1.0f, 0.01, 1000.0f}
		});
		app.set_camera({{-3.0f, 0.5f, 0.4f}, {-2.2f, -0.04f, 0.40f}, glm::radians(90.0f), static_cast<float>(screen_size.width)/screen_size.height, 0.1f, 1000.0f});

        glfwSetInputMode(app.get_glfw_window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        app.init_renderer();

        app.start_frame_loop(resize_callback, frame_start);
	}
	catch (std::pair<int32_t,vulkan_helper::Error>& err) {
		std::cout << "The application encounted the error: " << magic_enum::enum_name(err.second) << " with return value: " << err.first << std::endl;
	}
	return 0;
}