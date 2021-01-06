#include "camera.h"
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

Camera::Camera() {};

Camera::Camera(glm::vec3 pos, glm::vec3 dir, float fov, float aspect, float znear, float zfar) : pos{pos}, dir{dir}, fov{fov}, aspect{aspect}, znear{znear}, zfar{zfar} {}

glm::uvec2 Camera::get_resolution_from_ratio(int size) {
    return {size*aspect, size};
}

uint32_t Camera::copy_data_to_ptr(uint8_t *ptr) {
    if (ptr != nullptr) {
        memcpy(ptr, glm::value_ptr(glm::lookAt(pos, dir, glm::vec3(0.0f, 1.0f, 0.0f))), sizeof(glm::mat4));

        glm::mat4 perspective_matrix;
        if (zfar == std::numeric_limits<float>::max()) {
            perspective_matrix = glm::infinitePerspective(fov, aspect, znear);
        }
        else {
            perspective_matrix = glm::perspective(fov, aspect, znear, zfar);
        }
        // TODO: reactive perspective matrix y inversion
        //perspective_matrix[1][1] *= -1;
        memcpy(ptr+sizeof(glm::mat4), glm::value_ptr(perspective_matrix), sizeof(glm::mat4));

        memcpy(ptr+sizeof(glm::mat4)*2, glm::value_ptr(glm::vec4(pos, 0.0f)), sizeof(glm::vec4));
    }
    return sizeof(glm::mat4)*2+sizeof(glm::vec4);
}
