#include "camera.h"
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

Camera::Camera(glm::vec3 pos, glm::vec3 dir, float fov, float aspect, float znear, float zfar) : pos{pos}, dir{dir}, fov{fov}, aspect{aspect}, znear{znear}, zfar{zfar} {
    update_matrices_and_planes();
}

uint32_t Camera::copy_data_to_ptr(uint8_t *ptr) const {
    update_matrices_and_planes();
    if (ptr != nullptr) {
        memcpy(ptr, glm::value_ptr(view_matrix), sizeof(glm::mat4));
        memcpy(ptr+sizeof(glm::mat4), glm::value_ptr(glm::transpose(glm::inverse(view_matrix))), sizeof(glm::mat4));
        memcpy(ptr+sizeof(glm::mat4)*2, glm::value_ptr(proj_matrix), sizeof(glm::mat4));
        memcpy(ptr+sizeof(glm::mat4)*3, glm::value_ptr(proj_matrix * view_matrix), sizeof(glm::mat4));
        memcpy(ptr+sizeof(glm::mat4)*4, glm::value_ptr(prev_proj_matrix * prev_view_matrix), sizeof(glm::mat4));
        memcpy(ptr+sizeof(glm::mat4)*5, glm::value_ptr(glm::vec4(pos, 0.0f)), sizeof(glm::vec4));
    }
    return sizeof(glm::mat4)*5+sizeof(glm::vec4);
}

bool Camera::is_sphere_visible(glm::vec3 s_center, float s_radius) const {
    update_matrices_and_planes();
    for (uint32_t i = 0; i < 6; i++) {
        float side = glm::dot(s_center, camera_frustum.planes[i].normal) + camera_frustum.planes[i].distance;
        if (side < -s_radius) {
            return false;
        }
    }
    return true;
}

void Camera::update_matrices_and_planes() const {
    if (!matrices_up_to_date) {
        proj_matrix = glm::perspective(fov, aspect, znear, zfar);
        view_matrix = glm::lookAt(pos, pos+dir, glm::vec3(0.0f, -1.0f, 0.0f));

        glm::mat4 vp_matrix = proj_matrix * view_matrix;

        glm::vec3 col1(vp_matrix[0][0], vp_matrix[1][0], vp_matrix[2][0]);
        glm::vec3 col2(vp_matrix[0][1], vp_matrix[1][1], vp_matrix[2][1]);
        glm::vec3 col3(vp_matrix[0][2], vp_matrix[1][2], vp_matrix[2][2]);
        glm::vec3 col4(vp_matrix[0][3], vp_matrix[1][3], vp_matrix[2][3]);

        camera_frustum.p_left.normal = col4 + col1;
        camera_frustum.p_right.normal = col4 - col1;
        camera_frustum.p_bottom.normal = col4 + col2;
        camera_frustum.p_top.normal = col4 - col2;
        camera_frustum.p_near.normal = col3;
        camera_frustum.p_far.normal = col4 - col3;

        camera_frustum.p_left.distance = vp_matrix[3][3] + vp_matrix[3][0];
        camera_frustum.p_right.distance = vp_matrix[3][3] - vp_matrix[3][0];
        camera_frustum.p_bottom.distance = vp_matrix[3][3] + vp_matrix[3][1];
        camera_frustum.p_top.distance = vp_matrix[3][3] - vp_matrix[3][1];
        camera_frustum.p_near.distance = vp_matrix[3][2];
        camera_frustum.p_far.distance = vp_matrix[3][3] - vp_matrix[3][2];

        for (uint32_t i = 0; i < 6; i++) {
            float mag = 1.0f / glm::length(camera_frustum.planes[i].normal);
            camera_frustum.planes[i].normal *= mag;
            camera_frustum.planes[i].distance *= mag;
        }

        matrices_up_to_date = true;
    }
}

void Camera::update_prev_matrices() {
    prev_proj_matrix = proj_matrix;
    prev_view_matrix = view_matrix;
}
