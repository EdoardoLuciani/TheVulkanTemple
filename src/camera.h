#ifndef BASE_VULKAN_APP_CAMERA_H
#define BASE_VULKAN_APP_CAMERA_H
#include <glm/glm.hpp>
#include <array>

class Camera {
    public:
        Camera();
        Camera(glm::vec4 pos, glm::vec3 dir, float fov, float aspect, float znear = 0.001f, float zfar = std::numeric_limits<float>::max());
        glm::uvec2 get_resolution_from_ratio(int size);
        // View, Projection and camera_pos
        virtual uint32_t copy_data_to_ptr(uint8_t *ptr);
    protected:
        glm::vec4 pos;
        glm::vec3 dir;
        float fov;
        float aspect;
        float znear = 0.001f;
        float zfar = std::numeric_limits<float>::max();

        friend class FPSCameraControl;
    private:
};


#endif //BASE_VULKAN_APP_CAMERA_H
