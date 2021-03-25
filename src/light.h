#ifndef BASE_VULKAN_APP_LIGHT_H
#define BASE_VULKAN_APP_LIGHT_H
#include "camera.h"
#include <glm/glm.hpp>


class Light : public Camera {
    public:
        Light(glm::vec4 pos, glm::vec3 dir, glm::vec3 color, float fov, float aspect, float znear = 0.001f, float zfar = std::numeric_limits<float>::max());
        // View, Projection, light_pos and color
        uint32_t copy_data_to_ptr(uint8_t *ptr) override;
        glm::vec3 color;
};


#endif //BASE_VULKAN_APP_LIGHT_H
