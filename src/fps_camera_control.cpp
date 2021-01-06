#include "fps_camera_control.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/vector_angle.hpp>

FPSCameraControl::FPSCameraControl(GLFWwindow *window_handle, Camera *camera) : camera{camera} {
    glfwSetKeyCallback(window_handle, key_callback_static);
    glfwSetCursorPosCallback(window_handle, cursor_position_callback_static);
    glfwSetInputMode(window_handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetWindowUserPointer(window_handle, this);
}

void FPSCameraControl::key_callback_static(GLFWwindow* window, int key, int scancode, int action, int mods) {
    FPSCameraControl* that = static_cast<FPSCameraControl*>(glfwGetWindowUserPointer(window));
    that->key_callback(window, key, scancode, action, mods);
}

void FPSCameraControl::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_W) {
        float angle = glm::orientedAngle(glm::normalize(glm::vec2(camera->dir[0], camera->dir[2])), glm::vec2(1.0f, 0.0f));
        float z_comp = sin(angle);
        float x_comp = cos(angle);
        float z_comp_sign = glm::sign(z_comp);
        float x_comp_sign = glm::sign(x_comp);

        camera->pos[2] -= 0.05f * z_comp_sign * pow(z_comp, 2);
        camera->pos[0] += 0.05f * x_comp_sign * pow(x_comp, 2);
    }
    else if (key == GLFW_KEY_S) {
        float angle = glm::orientedAngle(glm::normalize(glm::vec2(camera->dir[0], camera->dir[2])), glm::vec2(1.0f, 0.0f));
        float z_comp = sin(angle);
        float x_comp = cos(angle);
        float z_comp_sign = glm::sign(z_comp);
        float x_comp_sign = glm::sign(x_comp);

        camera->pos[2] += 0.05f * z_comp_sign * pow(z_comp, 2);
        camera->pos[0] -= 0.05f * x_comp_sign * pow(x_comp, 2);
    }
    else if (key == GLFW_KEY_D) {
        float angle = glm::orientedAngle(glm::normalize(glm::vec2(camera->dir[0], camera->dir[2])), glm::vec2(0.0f, -1.0f));
        float z_comp = sin(angle);
        float x_comp = cos(angle);
        float z_comp_sign = glm::sign(z_comp);
        float x_comp_sign = glm::sign(x_comp);

        camera->pos[2] += 0.05f * z_comp_sign * pow(z_comp, 2);
        camera->pos[0] -= 0.05f * x_comp_sign * pow(x_comp, 2);
    }
    else if (key == GLFW_KEY_A) {
        float angle = glm::orientedAngle(glm::normalize(glm::vec2(camera->dir[0], camera->dir[2])), glm::vec2(0.0f, -1.0f));
        float z_comp = sin(angle);
        float x_comp = cos(angle);
        float z_comp_sign = glm::sign(z_comp);
        float x_comp_sign = glm::sign(x_comp);

        camera->pos[2] -= 0.05f * z_comp_sign * pow(z_comp, 2);
        camera->pos[0] += 0.05f * x_comp_sign * pow(x_comp, 2);
    }
    else if (key == GLFW_KEY_LEFT_SHIFT) {
        camera->pos[1] += 0.05f;
    }
    else if (key == GLFW_KEY_LEFT_CONTROL) {
        camera->pos[1] -= 0.05f;
    }
    else if (key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

void FPSCameraControl::cursor_position_callback_static(GLFWwindow* window, double xpos, double ypos) {
    FPSCameraControl* that = static_cast<FPSCameraControl*>(glfwGetWindowUserPointer(window));
    that->cursor_position_callback(window, xpos, ypos);
}

void FPSCameraControl::cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
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

    float x_sensivity = 0.0003f;
    float y_sensivity = 0.0001f;

    float key_yaw = x_sensivity * x_delta;
    float key_pitch = y_sensivity * -y_delta;

    /*camera->dir = glm::vec3( glm::rotate(key_pitch, glm::vec3(1.0f, 0.0f, 0.0f))
        *glm::rotate(key_yaw, glm::vec3(0.0f, 1.0f, 0.0f))
        *glm::vec4(camera->dir, 1.0f));*/

    glm::vec3 axis = glm::cross(glm::vec3(camera->dir), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat pitch_quat = glm::angleAxis(key_pitch, axis);
    glm::quat heading_quat = glm::angleAxis(key_yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat temp = glm::cross(pitch_quat, heading_quat);
    temp = glm::normalize(temp);
    camera->dir = glm::rotate(temp, camera->dir);
}
