#ifndef BASE_VULKAN_APP_CAMERA_H
#define BASE_VULKAN_APP_CAMERA_H
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <array>

class Camera {
    public:
        Camera() = default;
        Camera(glm::vec3 pos, glm::vec3 dir, float fov, float aspect, float znear, float zfar);

        // View, Projection and camera_pos
        uint32_t copy_data_to_ptr(uint8_t *ptr) const;
        bool is_sphere_visible(glm::vec3 center, float radius) const;
        void update_prev_matrices();

        // Getters
        glm::vec3 get_pos() const { return pos; }
        glm::vec3 get_dir() const { return dir; }
        float get_fov() const { return fov; }
        float get_aspect() const { return aspect; }
        float get_znear() const { return znear; }
        float get_zfar() const { return zfar; }

        glm::mat4 get_proj_matrix() const { return proj_matrix; }
        glm::mat4 get_view_matrix() const { return view_matrix; }

        // Setters
        void set_pos(glm::vec3 pos) { this->pos = pos; matrices_up_to_date = false; }
        void set_dir(glm::vec3 dir) { this->dir = dir; matrices_up_to_date = false; }
        void set_fov(float fov) { this->fov = fov; matrices_up_to_date = false; }
        void set_aspect(float aspect) { this->aspect = aspect; matrices_up_to_date = false; }
        void set_znear(float znear) { this->znear = znear; matrices_up_to_date = false; }
        void set_zfar(float zfar) { this->zfar = zfar; matrices_up_to_date = false; }

    private:
        void update_matrices_and_planes() const;

        glm::vec3 pos = glm::vec3(0.0f);
        glm::vec3 dir = glm::vec3(0.0f);
        float fov = glm::radians(90.0f);
        float aspect = 1.0f;
        float znear = 0.001f;
        float zfar = 1000.0f;

        struct plane {
            glm::vec3 normal;
            float distance;
        };

        struct frustum {
            union {
                struct {
                    plane p_left;
                    plane p_right;
                    plane p_bottom;
                    plane p_top;
                    plane p_near;
                    plane p_far;
                };
                std::array<plane, 6> planes;
            };
        };
        mutable frustum camera_frustum;

        mutable bool matrices_up_to_date = false;
        mutable glm::mat4 proj_matrix;
        mutable glm::mat4 view_matrix;

        // Previous frame proj and view matrix
        glm::mat4 prev_proj_matrix = glm::identity<glm::mat4>();
        glm::mat4 prev_view_matrix = glm::identity<glm::mat4>();
};


#endif //BASE_VULKAN_APP_CAMERA_H
