#ifndef BASE_VULKAN_APP_LIGHT_H
#define BASE_VULKAN_APP_LIGHT_H
#include "camera.h"
#include <glm/glm.hpp>

class Light {
    public:
        enum class LightType : uint32_t {
            SPOT = 0,
            POINT = 1,
            DIRECTIONAL = 2
        };
        Light();
        Light(glm::vec3 pos, glm::vec3 dir, glm::vec3 color, Light::LightType type, float falloff_distance,
              glm::vec2 penumbra_umbra_angles, uint32_t shadow_map_height, float fov, float aspect, float znear, float zfar);

        inline glm::vec3 get_pos() const { return light_params.position; };
        inline LightType get_type() const { return light_params.type; };
        inline glm::vec3 get_dir() const { return light_params.direction; };
        inline float get_falloff_distance() const { return light_params.falloff_distance; };
        inline glm::vec3 get_color() const { return light_params.color; };
        inline glm::vec2 get_penumbra_umbra_angles() const { return light_params.penumbra_umbra_angles; };

        inline float get_fov() const {return fov; };
        inline float get_aspect() const {return aspect; };
        inline float get_znear() const {return znear; };
        inline float get_zfar() const {return zfar; };

        glm::uvec2 get_shadow_map_resolution() const;
        glm::mat4 get_proj_matrix() const;
        glm::mat4 get_view_matrix() const;

        inline void set_pos(glm::vec3 pos) const { light_params.position = pos; };
        inline void set_dir(glm::vec3 dir) const { light_params.direction = dir; };
        inline void set_falloff_distance(float distance) const { light_params.falloff_distance = distance; };
        inline void set_color(glm::vec3 color) const { light_params.color = color; };
        inline void set_penumbra_umbra_angles(glm::vec2 penumbra_umbra_angles) const { light_params.penumbra_umbra_angles = penumbra_umbra_angles; };

        inline float set_fov() const {return fov; };
        inline float set_aspect() const {return aspect; };
        inline float set_znear() const {return znear; };
        inline float set_zfar() const {return zfar; };

        uint32_t copy_data_to_ptr(uint8_t *ptr) const;

private:
        // tight packed struct aligned to 16 bytes
        struct LightParams {
            glm::vec3 position;
            LightType type;
            glm::vec3 direction;
            float falloff_distance;
            glm::vec3 color;
            int shadow_map_index;
            glm::vec2 penumbra_umbra_angles;
            glm::vec2 dummy;
        };
        mutable LightParams light_params;

		uint32_t shadow_map_height;

        mutable float fov;
        mutable float aspect;
        mutable float znear;
        mutable float zfar;

        // GraphicsModuleVulkanApp should be able to modify light_params, since it is directly copied to gpu memory
        friend class GraphicsModuleVulkanApp;
};

#endif //BASE_VULKAN_APP_LIGHT_H
