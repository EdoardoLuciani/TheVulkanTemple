#include "light.h"
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

Light::Light(glm::vec3 pos, glm::vec3 dir, glm::vec3 color, float fov, float aspect, float znear, float zfar) : Camera(pos, dir, fov, aspect, znear, zfar), color{color} {}

uint32_t Light::copy_data_to_ptr(uint8_t *ptr) {
    if (ptr != nullptr) {
        memcpy(ptr, glm::value_ptr(glm::lookAt(pos, dir, glm::vec3(0.0f, 1.0f, 0.0f))), sizeof(glm::mat4));
        if (zfar == std::numeric_limits<float>::max()) {
            memcpy(ptr+sizeof(glm::mat4), glm::value_ptr(glm::infinitePerspective(fov, aspect, znear)), sizeof(glm::mat4));
        }
        else {
            memcpy(ptr+sizeof(glm::mat4), glm::value_ptr(glm::perspective(fov, aspect, znear, zfar)), sizeof(glm::mat4));
        }
        memcpy(ptr+sizeof(glm::mat4)*2, glm::value_ptr(pos), sizeof(glm::vec4));
        memcpy(ptr+sizeof(glm::mat4)*2+sizeof(glm::vec4), glm::value_ptr(color), sizeof(glm::vec4));
    }
    return sizeof(glm::mat4)*2+sizeof(glm::vec4)*2;
}
