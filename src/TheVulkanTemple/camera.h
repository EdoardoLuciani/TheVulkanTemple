#ifndef BASE_VULKAN_APP_CAMERA_H
#define BASE_VULKAN_APP_CAMERA_H
#include <glm/glm.hpp>
#include <array>

class Camera {
    public:
        Camera();
        Camera(glm::vec3 pos, glm::vec3 dir, float fov, float aspect, float znear, float zfar);

        // View, Projection and camera_pos
        uint32_t copy_data_to_ptr(uint8_t *ptr) const;
        bool is_sphere_visible(glm::vec3 center, float radius) const;

        // Getters
        glm::vec3 get_pos() { return pos; }
        glm::vec3 get_dir() { return dir; }
        float get_fov() { return fov; }
        float get_aspect() { return aspect; }
        float get_znear() { return znear; }
        float get_zfar() { return zfar; }

        glm::mat4 get_proj_matrix() { return proj_matrix; }
        glm::mat4 get_view_matrix() { return view_matrix; }

        glm::dvec2 get_prev_pos() { return this->prev_pos; };
        float get_distance() { return distance; };

        // Setters
        void set_pos(glm::vec3 pos) { this->pos = pos; matrices_up_to_date = false; }
        void set_dir(glm::vec3 dir) { this->dir = dir; matrices_up_to_date = false; }
        void set_fov(float fov) { this->fov = fov; matrices_up_to_date = false; }
        void set_aspect(float aspect) { this->aspect = aspect; matrices_up_to_date = false; }
        void set_znear(float znear) { this->znear = znear; matrices_up_to_date = false; }
        void set_zfar(float zfar) { this->zfar = zfar; matrices_up_to_date = false; }

        void set_ex_pos(glm::dvec2 new_ex_pos) { this->prev_pos = new_ex_pos; };
        void set_distance(float new_distance) { this->distance = new_distance; };

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

        glm::dvec2 prev_pos;
        float distance = -1;
};


#endif //BASE_VULKAN_APP_CAMERA_H
