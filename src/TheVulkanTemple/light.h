#ifndef BASE_VULKAN_APP_LIGHT_H
#define BASE_VULKAN_APP_LIGHT_H
#include "camera.h"
#include <glm/glm.hpp>


class Light : public Camera {
    public:
        enum class LightType : uint32_t {
            SPOT = 0,
            POINT = 1,
            DIRECTIONAL = 2
        };
        Light(glm::vec3 pos, glm::vec3 dir, glm::vec3 color, Light::LightType type, float falloff_distance, uint32_t shadow_map_index,
              glm::vec2 penumbra_umbra_angles, float fov, float aspect, float znear = 0.001f, float zfar = std::numeric_limits<float>::max());
        // View, Projection, light_pos and color
        uint32_t copy_data_to_ptr(uint8_t *ptr) override;

        // tight packed struct aligned to 16 bytes
        struct LightParams {
            glm::vec3 position;
            LightType type;
            glm::vec3 direction;
            float falloff_distance;
            glm::vec3 color;
            uint32_t shadow_map_index;
            glm::vec2 penumbra_umbra_angles;
            glm::vec2 dummy;
        };
        LightParams light_params;
};


#endif //BASE_VULKAN_APP_LIGHT_H
