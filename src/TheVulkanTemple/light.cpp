#include "light.h"
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

Light::Light(glm::vec3 pos, glm::vec3 dir, glm::vec3 color, Light::LightType type, float falloff_distance,
             uint32_t shadow_map_index, glm::vec2 penumbra_umbra_angles, float fov, float aspect, float znear, float zfar):
    Camera(pos, dir, fov, aspect, znear, zfar),
    light_params{pos, type, dir, falloff_distance, color, shadow_map_index, penumbra_umbra_angles, glm::vec2(0.0f)} {}

uint32_t Light::copy_data_to_ptr(uint8_t *ptr) {
    if (ptr != nullptr) {
        memcpy(ptr, &light_params, sizeof(LightParams));
        memcpy(ptr+sizeof(LightParams), glm::value_ptr(this->get_view_matrix()), sizeof(glm::mat4));
        memcpy(ptr+sizeof(glm::mat4)+sizeof(LightParams), glm::value_ptr(this->get_proj_matrix()), sizeof(glm::mat4));
    }
    return sizeof(glm::mat4)*2+sizeof(LightParams);
}
