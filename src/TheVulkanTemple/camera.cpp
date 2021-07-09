#include "camera.h"
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

Camera::Camera() {};

Camera::Camera(glm::vec3 pos, glm::vec3 dir, float fov, float aspect, float znear, float zfar) : pos{pos}, dir{dir}, fov{fov}, aspect{aspect}, znear{znear}, zfar{zfar} {}

glm::mat4 Camera::get_proj_matrix() {
    return glm::perspective(fov, aspect, znear, zfar);
}

glm::mat4 Camera::get_view_matrix() {
    return glm::lookAt(pos, pos+dir, glm::vec3(0.0f, -1.0f, 0.0f));
}

uint32_t Camera::copy_data_to_ptr(uint8_t *ptr) {
    if (ptr != nullptr) {
        memcpy(ptr, glm::value_ptr(this->get_view_matrix()), sizeof(glm::mat4));
        memcpy(ptr+sizeof(glm::mat4), glm::value_ptr(glm::transpose(glm::inverse(this->get_view_matrix()))), sizeof(glm::mat4));
        memcpy(ptr+sizeof(glm::mat4)*2, glm::value_ptr(this->get_proj_matrix()), sizeof(glm::mat4));
        memcpy(ptr+sizeof(glm::mat4)*3, glm::value_ptr(glm::vec4(pos, 0.0f)), sizeof(glm::vec4));
    }
    return sizeof(glm::mat4)*3+sizeof(glm::vec4);
}
