#include "light.h"
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

Light::Light(glm::vec3 pos, glm::vec3 dir, glm::vec3 color, Light::LightType type, float falloff_distance,
             glm::vec2 penumbra_umbra_angles, uint32_t shadow_map_height, float fov, float aspect, float znear, float zfar):
    light_params{pos, type, dir, falloff_distance, color, -1, penumbra_umbra_angles, glm::vec2(0.0f)},
    shadow_map_height{shadow_map_height}, fov{fov}, aspect{aspect}, znear{znear}, zfar{zfar} {
}

glm::uvec2 Light::get_shadow_map_resolution() const {
    return {shadow_map_height*aspect, shadow_map_height};
}

glm::mat4 Light::get_proj_matrix() const {
    glm::mat4 proj_matrix;
    if (light_params.type == Light::LightType::DIRECTIONAL) {
        float focus_plane = (znear + zfar)/2;
        float halfY = fov/2;
        float top = focus_plane * glm::tan(halfY); //focus_plane is the distance from the camera
        float right = top * aspect;
        proj_matrix = glm::ortho(-right, right, -top, top, znear, zfar);
    }
    else {
        proj_matrix = glm::perspective(fov, aspect, znear, zfar);
    }
    return proj_matrix;
}

glm::mat4 Light::get_view_matrix() const {
    return glm::lookAt(light_params.position, light_params.position + light_params.direction, glm::vec3(0.0f, -1.0f, 0.0f));
}

uint32_t Light::copy_data_to_ptr(uint8_t *ptr) const {
    if (ptr != nullptr) {
        memcpy(ptr, &light_params, sizeof(LightParams));
        memcpy(ptr+sizeof(LightParams), glm::value_ptr(this->get_view_matrix()), sizeof(glm::mat4));
        memcpy(ptr+sizeof(glm::mat4)+sizeof(LightParams), glm::value_ptr(this->get_proj_matrix()), sizeof(glm::mat4));
    }
    return sizeof(glm::mat4)*2+sizeof(LightParams);
}