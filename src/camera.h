#ifndef BASE_VULKAN_APP_CAMERA_H
#define BASE_VULKAN_APP_CAMERA_H
#include <glm/glm.hpp>
#include <array>

class Camera {
    public:
        Camera();
        Camera(glm::vec4 pos, glm::vec3 dir, float fov, float aspect, float znear = 0.001f, float zfar = std::numeric_limits<float>::max());
        glm::uvec2 get_resolution_from_ratio(int size);
        glm::mat4 get_proj_matrix();
        glm::mat4 get_view_matrix();
        glm::mat4 get_view_matrix2();
        // View, Projection and camera_pos
        virtual uint32_t copy_data_to_ptr(uint8_t *ptr);

        // Getters
        glm::vec4 get_pos() { return this->pos; };
        glm::vec3 get_dir() { return this->dir; };
        glm::dvec2 get_ex_pos() { return this->ex_pos; };

        // Setters
        void set_pos(glm::vec3 new_pos) { this->pos = glm::vec4(new_pos, 0.0f); };
        void set_aspect(float new_aspect) { this->aspect = new_aspect; };
        void set_dir(glm::vec3 new_dir) { this->dir = new_dir; };
        void set_ex_pos(glm::dvec2 new_ex_pos) { this->ex_pos = new_ex_pos; };

    protected:
        glm::vec4 pos;
        glm::vec3 dir;
        float fov;
        float aspect;
        float znear = 0.001f;
        float zfar = std::numeric_limits<float>::max();

        friend class FPSCameraControl;
    private:
        glm::dvec2 ex_pos;
};


#endif //BASE_VULKAN_APP_CAMERA_H
