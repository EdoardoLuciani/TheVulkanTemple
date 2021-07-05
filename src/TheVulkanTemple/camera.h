#ifndef BASE_VULKAN_APP_CAMERA_H
#define BASE_VULKAN_APP_CAMERA_H
#include <glm/glm.hpp>
#include <array>

class Camera {
    public:
        Camera();
        Camera(glm::vec3 pos, glm::vec3 dir, float fov, float aspect, float znear = 0.001f, float zfar = std::numeric_limits<float>::max());
        // View, Projection and camera_pos
        virtual uint32_t copy_data_to_ptr(uint8_t *ptr);

        // Getters
        glm::uvec2 get_resolution_from_ratio(int size);
        glm::mat4 get_proj_matrix();
        glm::mat4 get_view_matrix();

        glm::dvec2 get_prev_pos() { return this->prev_pos; };
        float get_distance() { return distance; };

        // Setters
        void set_ex_pos(glm::dvec2 new_ex_pos) { this->prev_pos = new_ex_pos; };
        void set_distance(float new_distance) { this->distance = new_distance; };

        glm::vec3 pos = glm::vec3(0.0f);
        glm::vec3 dir = glm::vec3(0.0f);
        float fov = glm::radians(90.0f);
        float aspect = 1.0f;
        float znear = 0.001f;
        float zfar = std::numeric_limits<float>::max();
    private:
        glm::dvec2 prev_pos;
        float distance = -1;
};


#endif //BASE_VULKAN_APP_CAMERA_H
