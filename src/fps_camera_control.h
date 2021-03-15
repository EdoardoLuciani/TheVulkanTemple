#ifndef BASE_VULKAN_APP_FPS_CAMERA_CONTROL_H
#define BASE_VULKAN_APP_FPS_CAMERA_CONTROL_H

#include <GLFW/glfw3.h>
#include "camera.h"

class FPSCameraControl {
    public:
        FPSCameraControl(GLFWwindow *window_handle, Camera *camera);
    private:
        Camera *camera;
        double ex_xpos = -1, ex_ypos = -1;

        static void key_callback_static(GLFWwindow* window, int key, int scancode, int action, int mods);
        void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

        static void cursor_position_callback_static(GLFWwindow* window, double xpos, double ypos);
        void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);

        static void window_size_callback_static(GLFWwindow* window, int width, int height);
        void window_size_callback(GLFWwindow* window, int width, int height);
};

#endif