#include "light.h"
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

Light::Light(glm::vec4 pos, glm::vec3 dir, glm::vec3 color, float fov, float aspect, float znear, float zfar) : Camera(pos, dir, fov, aspect, znear, zfar), color{color} {}

uint32_t Light::copy_data_to_ptr(uint8_t *ptr) {
    if (ptr != nullptr) {
        memcpy(ptr, glm::value_ptr(this->get_view_matrix()), sizeof(glm::mat4));
        memcpy(ptr+sizeof(glm::mat4), glm::value_ptr(this->get_proj_matrix()), sizeof(glm::mat4));
        memcpy(ptr+sizeof(glm::mat4)*2, glm::value_ptr(pos), sizeof(glm::vec4));
        memcpy(ptr+sizeof(glm::mat4)*2+sizeof(glm::vec4), glm::value_ptr(glm::vec4(color, 0.0f)), sizeof(glm::vec4));
    }
    return sizeof(glm::mat4)*2+sizeof(glm::vec4)*2;
}
